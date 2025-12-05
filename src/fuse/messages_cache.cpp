#include "fuse/messages_cache.hpp"

#include "tg/formatters.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace tgfuse {

FormattedMessagesCache::FormattedMessagesCache(Config config) : config_(std::move(config)) {}

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
    const UserResolver& user_resolver,
    const ChatResolver& chat_resolver
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

    // Build MessageInfo vector for formatting
    const auto& chat = chat_resolver(chat_id);
    std::vector<tg::MessageInfo> infos;
    infos.reserve(sorted.size());
    for (const auto& msg : sorted) {
        infos.push_back({msg, user_resolver(msg.sender_id), chat});
    }

    // Format all messages using fmt::format with ranges
    std::string content = fmt::format("{}\n", fmt::join(infos, "\n"));

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
    const UserResolver& user_resolver,
    const ChatResolver& chat_resolver
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

    // Format single message using MessageInfo
    tg::MessageInfo info{message, user_resolver(message.sender_id), chat_resolver(chat_id)};
    entry.content += fmt::format("{}\n", info);
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
