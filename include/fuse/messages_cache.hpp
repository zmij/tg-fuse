#pragma once

#include "fuse/message_formatter.hpp"
#include "tg/types.hpp"

#include <cstdint>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace tgfuse {

/// Callback type for resolving sender information
using SenderResolver = std::function<SenderInfo(int64_t sender_id)>;

/// LRU cache for formatted message content per chat
///
/// This cache stores pre-formatted message strings that are ready to be
/// served via FUSE reads. The cache is updated incrementally when new
/// messages arrive from TDLib, avoiding expensive re-formatting on each read.
///
/// Key design decisions:
/// - LRU eviction to bound memory usage
/// - Append-only updates (new messages are appended to existing content)
/// - Thread-safe access with mutex protection
/// - Tracks content size for write detection
class FormattedMessagesCache {
public:
    /// Cache entry for a single chat
    struct CacheEntry {
        std::string content;           // Formatted messages content
        int64_t oldest_message_id{0};  // ID of oldest message in content
        int64_t newest_message_id{0};  // ID of newest message in content
        std::size_t message_count{0};  // Number of messages in content
    };

    /// Configuration for the cache
    struct Config {
        std::size_t max_chats;              // Maximum number of chats to cache
        std::size_t max_messages_per_chat;  // Maximum messages per chat
        std::string message_template;       // Mustache template for messages

        Config() : max_chats(100), max_messages_per_chat(50) {}
    };

    explicit FormattedMessagesCache(Config config = {});
    ~FormattedMessagesCache() = default;

    // Disable copy
    FormattedMessagesCache(const FormattedMessagesCache&) = delete;
    FormattedMessagesCache& operator=(const FormattedMessagesCache&) = delete;

    /// Get formatted content for a chat
    /// @param chat_id The chat ID
    /// @return The formatted content, or nullopt if not cached
    [[nodiscard]] std::optional<std::string_view> get(int64_t chat_id);

    /// Get the content size for a chat (for write detection)
    /// @param chat_id The chat ID
    /// @return The content size, or 0 if not cached
    [[nodiscard]] std::size_t get_content_size(int64_t chat_id) const;

    /// Check if a chat is cached
    [[nodiscard]] bool contains(int64_t chat_id) const;

    /// Set the complete formatted content for a chat
    /// Used for initial population from message history
    /// @param chat_id The chat ID
    /// @param messages The messages to format (oldest first)
    /// @param resolver Function to resolve sender information
    void populate(int64_t chat_id, const std::vector<tg::Message>& messages, const SenderResolver& resolver);

    /// Append a new message to a chat's content
    /// Called when TDLib sends updateNewMessage
    /// @param chat_id The chat ID
    /// @param message The new message
    /// @param resolver Function to resolve sender information
    void append_message(int64_t chat_id, const tg::Message& message, const SenderResolver& resolver);

    /// Invalidate cache for a specific chat
    void invalidate(int64_t chat_id);

    /// Clear all cached content
    void clear();

    /// Get cache statistics
    struct Stats {
        std::size_t chat_count;
        std::size_t total_content_size;
        std::size_t hit_count;
        std::size_t miss_count;
    };
    [[nodiscard]] Stats get_stats() const;

    /// Set the message template (mustache format)
    void set_template(std::string_view tmpl);

private:
    /// Format a single message using the template
    [[nodiscard]] std::string format_message(const tg::Message& msg, const SenderInfo& sender) const;

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
