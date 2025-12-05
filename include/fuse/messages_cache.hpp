#pragma once

#include "tg/types.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tgfuse {

/// Callback type for resolving sender and chat information
using UserResolver = std::function<const tg::User&(int64_t sender_id)>;
using ChatResolver = std::function<const tg::Chat&(int64_t chat_id)>;

/// Configuration for the TLRU formatted messages cache
struct MessagesCacheConfig {
    std::size_t max_chats = 100;                                    // Maximum number of chats in LRU
    std::chrono::seconds format_ttl = std::chrono::hours{1};        // Formatted text staleness TTL (1 hour)
    std::chrono::seconds max_history_age = std::chrono::hours{48};  // Max age of messages to display
    std::size_t min_messages = 10;                                  // Minimum messages to fetch from API
};

/// TLRU (Time-aware LRU) cache for formatted message content per chat
///
/// This cache stores pre-formatted message strings that are ready to be
/// served via FUSE reads. Raw messages are stored in SQLite; this cache
/// only holds formatted text with a TTL for staleness.
///
/// Key design decisions:
/// - TLRU eviction: entries expire after format_ttl (1 hour default)
/// - LRU eviction when over capacity
/// - Raw messages stored in SQLite (not in this cache)
/// - Thread-safe access with mutex protection
/// - On new message: invalidate entry (lazy reformat on next read)
class FormattedMessagesCache {
public:
    using Config = MessagesCacheConfig;

    /// Cache entry for a single chat (formatted text only, no raw messages)
    struct CacheEntry {
        std::string content;                                 // Formatted messages content
        std::size_t message_count{0};                        // Number of messages in content
        int64_t newest_message_id{0};                        // ID of newest message (for append detection)
        std::chrono::steady_clock::time_point formatted_at;  // When this was formatted (for TTL)
    };

    explicit FormattedMessagesCache(Config config = {});
    ~FormattedMessagesCache() = default;

    // Disable copy
    FormattedMessagesCache(const FormattedMessagesCache&) = delete;
    FormattedMessagesCache& operator=(const FormattedMessagesCache&) = delete;

    /// Get formatted content for a chat (returns nullopt if not cached or stale)
    /// @param chat_id The chat ID
    /// @return The formatted content, or nullopt if not cached or TTL expired
    [[nodiscard]] std::optional<std::string_view> get(int64_t chat_id);

    /// Get the content size for a chat (for fstat reporting)
    /// @param chat_id The chat ID
    /// @return The content size, or 0 if not cached
    [[nodiscard]] std::size_t get_content_size(int64_t chat_id) const;

    /// Check if a chat is cached (regardless of staleness)
    [[nodiscard]] bool contains(int64_t chat_id) const;

    /// Check if a chat's cache entry is stale (TTL expired)
    /// @param chat_id The chat ID
    /// @return true if stale or not cached
    [[nodiscard]] bool is_stale(int64_t chat_id) const;

    /// Store formatted content for a chat
    /// Called after formatting messages from SQLite
    /// @param chat_id The chat ID
    /// @param content The pre-formatted content string
    /// @param message_count Number of messages in the content
    /// @param newest_message_id ID of the newest message
    void store(int64_t chat_id, std::string content, std::size_t message_count, int64_t newest_message_id);

    /// Invalidate cache for a specific chat (forces reformat on next read)
    void invalidate(int64_t chat_id);

    /// Clear all cached content
    void clear();

    /// Get the cache configuration
    [[nodiscard]] const Config& get_config() const { return config_; }

    /// Get cache statistics
    struct Stats {
        std::size_t chat_count;
        std::size_t total_content_size;
        std::size_t hit_count;
        std::size_t miss_count;
    };
    [[nodiscard]] Stats get_stats() const;

private:
    /// Touch a chat to mark it as recently used (moves to front of LRU)
    void touch(int64_t chat_id);

    /// Evict least recently used entries if over capacity
    void evict_if_needed();

    Config config_;

    // LRU list: front = most recently used, back = least recently used
    std::list<int64_t> lru_list_;

    // Map from chat_id to (iterator in lru_list, cache entry)
    using LruIterator = std::list<int64_t>::iterator;
    std::unordered_map<int64_t, std::pair<LruIterator, CacheEntry>> cache_;

    mutable std::mutex mutex_;
    mutable std::size_t hit_count_{0};
    mutable std::size_t miss_count_{0};
};

}  // namespace tgfuse
