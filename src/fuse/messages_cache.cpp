#include "fuse/messages_cache.hpp"

#include "tg/formatters.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <bustache/format.hpp>
#include <bustache/render.hpp>

#include <algorithm>

namespace tgfuse {

namespace {

// Default message template (mustache format)
constexpr std::string_view DEFAULT_TEMPLATE = R"(> **{{sender}}** *{{time}}* {{text}}{{#media}} {{media}}{{/media}}

)";

// Bustache value wrapper for message data
struct MessageData {
    std::string sender;
    std::string time;
    std::string text;
    std::string media;
    bool has_media;
};

}  // namespace

FormattedMessagesCache::FormattedMessagesCache(Config config) : config_(std::move(config)) {
    if (config_.message_template.empty()) {
        config_.message_template = std::string(DEFAULT_TEMPLATE);
    }
}

std::optional<std::string_view> FormattedMessagesCache::get(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it == cache_.end()) {
        ++miss_count_;
        return std::nullopt;
    }

    ++hit_count_;
    touch(chat_id);
    return std::string_view(it->second.second.content);
}

std::size_t FormattedMessagesCache::get_content_size(int64_t chat_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it == cache_.end()) {
        return 0;
    }
    return it->second.second.content.size();
}

bool FormattedMessagesCache::contains(int64_t chat_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cache_.find(chat_id) != cache_.end();
}

void FormattedMessagesCache::populate(
    int64_t chat_id,
    const std::vector<tg::Message>& messages,
    const SenderResolver& resolver
) {
    if (messages.empty()) {
        return;
    }

    // Sort messages by timestamp (oldest first)
    std::vector<tg::Message> sorted = messages;
    std::sort(sorted.begin(), sorted.end(), [](const tg::Message& a, const tg::Message& b) {
        return a.timestamp < b.timestamp;
    });

    // Remove duplicates by message ID
    auto last = std::unique(sorted.begin(), sorted.end(), [](const tg::Message& a, const tg::Message& b) {
        return a.id == b.id;
    });
    sorted.erase(last, sorted.end());

    // Limit to max messages
    if (sorted.size() > config_.max_messages_per_chat) {
        sorted.erase(sorted.begin(), sorted.begin() + (sorted.size() - config_.max_messages_per_chat));
    }

    // Format all messages
    std::string content;
    content.reserve(sorted.size() * 150);  // Estimate ~150 bytes per message

    for (const auto& msg : sorted) {
        SenderInfo sender = resolver(msg.sender_id);
        sender.is_outgoing = msg.is_outgoing;
        content += format_message(msg, sender);
    }

    // Store in cache
    {
        std::lock_guard<std::mutex> lock(mutex_);
        evict_if_needed();

        CacheEntry entry;
        entry.content = std::move(content);
        entry.oldest_message_id = sorted.front().id;
        entry.newest_message_id = sorted.back().id;
        entry.message_count = sorted.size();

        // Remove old entry if exists
        auto it = cache_.find(chat_id);
        if (it != cache_.end()) {
            lru_list_.erase(it->second.first);
            cache_.erase(it);
        }

        // Add new entry
        lru_list_.push_front(chat_id);
        cache_[chat_id] = {lru_list_.begin(), std::move(entry)};
    }

    spdlog::debug(
        "FormattedMessagesCache: populated chat {} with {} messages, {} bytes",
        chat_id,
        sorted.size(),
        cache_[chat_id].second.content.size()
    );
}

void FormattedMessagesCache::append_message(
    int64_t chat_id,
    const tg::Message& message,
    const SenderResolver& resolver
) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it == cache_.end()) {
        // Chat not cached, nothing to append to
        return;
    }

    auto& entry = it->second.second;

    // Skip if message already in cache (duplicate)
    if (message.id <= entry.newest_message_id) {
        return;
    }

    SenderInfo sender = resolver(message.sender_id);
    sender.is_outgoing = message.is_outgoing;

    entry.content += format_message(message, sender);
    entry.newest_message_id = message.id;
    ++entry.message_count;

    touch(chat_id);

    spdlog::debug(
        "FormattedMessagesCache: appended message {} to chat {}, now {} bytes",
        message.id,
        chat_id,
        entry.content.size()
    );
}

void FormattedMessagesCache::invalidate(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it != cache_.end()) {
        lru_list_.erase(it->second.first);
        cache_.erase(it);
    }
}

void FormattedMessagesCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    lru_list_.clear();
    cache_.clear();
}

FormattedMessagesCache::Stats FormattedMessagesCache::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Stats stats;
    stats.chat_count = cache_.size();
    stats.total_content_size = 0;
    for (const auto& [id, pair] : cache_) {
        stats.total_content_size += pair.second.content.size();
    }
    stats.hit_count = hit_count_;
    stats.miss_count = miss_count_;
    return stats;
}

void FormattedMessagesCache::set_template(std::string_view tmpl) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.message_template = std::string(tmpl);
    // Invalidate all cached content since format changed
    lru_list_.clear();
    cache_.clear();
}

std::string FormattedMessagesCache::format_message(const tg::Message& msg, const SenderInfo& sender) const {
    // Format sender name
    std::string sender_str;
    if (sender.is_outgoing) {
        sender_str = "You";
    } else if (!sender.username.empty()) {
        sender_str = fmt::format("{} (@{})", sender.display_name, sender.username);
    } else {
        sender_str = sender.display_name;
    }

    // Format time
    std::string time_str = tg::format_time(msg.timestamp);

    // Format text (handle multiline - continue blockquote on each line)
    std::string text = msg.text;
    std::size_t pos = 0;
    while ((pos = text.find('\n', pos)) != std::string::npos) {
        text.replace(pos, 1, "\n> ");
        pos += 3;
    }

    // Format media
    std::string media_str;
    if (msg.has_media()) {
        media_str = fmt::format("{}", msg.media.value());
    }

    // Use fmt for now (bustache integration can be added later for custom templates)
    // The mustache template would allow users to customize the format
    return fmt::format("> **{}** *{}* {}{}\n\n", sender_str, time_str, text, media_str.empty() ? "" : " " + media_str);
}

void FormattedMessagesCache::touch(int64_t chat_id) {
    // Caller must hold mutex_
    auto it = cache_.find(chat_id);
    if (it != cache_.end()) {
        lru_list_.erase(it->second.first);
        lru_list_.push_front(chat_id);
        it->second.first = lru_list_.begin();
    }
}

void FormattedMessagesCache::evict_if_needed() {
    // Caller must hold mutex_
    while (cache_.size() >= config_.max_chats && !lru_list_.empty()) {
        int64_t victim = lru_list_.back();
        lru_list_.pop_back();
        cache_.erase(victim);
        spdlog::debug("FormattedMessagesCache: evicted chat {}", victim);
    }
}

}  // namespace tgfuse
