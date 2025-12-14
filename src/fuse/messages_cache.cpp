#include "fuse/messages_cache.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace tgfuse {

FormattedMessagesCache::FormattedMessagesCache(Config config)
    : config_(std::move(config)), message_template_(config_.message_format) {}

std::optional<std::string_view> FormattedMessagesCache::get(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it == cache_.end()) {
        ++miss_count_;
        return std::nullopt;
    }

    // Check TTL - if stale, return nullopt (caller should reformat)
    auto now = std::chrono::steady_clock::now();
    if ((now - it->second.second.formatted_at) > config_.format_ttl) {
        ++miss_count_;
        spdlog::debug("FormattedMessagesCache: TTL expired for chat {}", chat_id);
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

bool FormattedMessagesCache::is_stale(int64_t chat_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it == cache_.end()) {
        return true;  // Not cached = stale
    }

    auto now = std::chrono::steady_clock::now();
    return (now - it->second.second.formatted_at) > config_.format_ttl;
}

void FormattedMessagesCache::store(
    int64_t chat_id,
    std::string content,
    std::size_t message_count,
    int64_t newest_message_id
) {
    std::lock_guard<std::mutex> lock(mutex_);
    evict_if_needed();

    // Remove old entry if exists
    auto it = cache_.find(chat_id);
    if (it != cache_.end()) {
        lru_list_.erase(it->second.first);
        cache_.erase(it);
    }

    // Create new entry
    CacheEntry entry;
    entry.content = std::move(content);
    entry.message_count = message_count;
    entry.newest_message_id = newest_message_id;
    entry.formatted_at = std::chrono::steady_clock::now();

    // Add to LRU
    lru_list_.push_front(chat_id);
    cache_[chat_id] = {lru_list_.begin(), std::move(entry)};

    spdlog::debug(
        "FormattedMessagesCache: stored chat {} with {} messages, {} bytes",
        chat_id,
        message_count,
        cache_[chat_id].second.content.size()
    );
}

void FormattedMessagesCache::invalidate(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(chat_id);
    if (it != cache_.end()) {
        lru_list_.erase(it->second.first);
        cache_.erase(it);
        spdlog::debug("FormattedMessagesCache: invalidated chat {}", chat_id);
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
