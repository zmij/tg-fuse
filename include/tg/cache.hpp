#pragma once

#include "tg/types.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace tg {

/// Statistics for cached messages of a chat (persisted in SQLite)
struct ChatMessageStats {
    int64_t chat_id{0};
    std::size_t message_count{0};    // Number of cached messages
    std::size_t content_size{0};     // Formatted content size in bytes
    int64_t last_message_time{0};    // Timestamp of newest message (for mtime)
    int64_t last_fetch_time{0};      // When we last fetched messages from API
    int64_t oldest_message_time{0};  // Timestamp of oldest message (for age check)
};

class CacheManager {
public:
    explicit CacheManager(const std::string& db_path);
    ~CacheManager();

    // Disable copy
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    // User caching
    void cache_user(const User& user);
    std::optional<User> get_cached_user(int64_t id);
    std::optional<User> get_cached_user_by_username(const std::string& username);
    std::vector<User> get_all_cached_users();

    // Chat caching
    void cache_chat(const Chat& chat);
    std::optional<Chat> get_cached_chat(int64_t id);
    std::optional<Chat> get_cached_chat_by_username(const std::string& username);
    std::vector<Chat> get_all_cached_chats();
    std::vector<Chat> get_cached_chats_by_type(ChatType type);

    // Message caching
    void cache_message(const Message& msg);
    void cache_messages(const std::vector<Message>& messages);
    std::optional<Message> get_cached_message(int64_t chat_id, int64_t message_id);
    std::vector<Message> get_cached_messages(int64_t chat_id, int limit = 100);
    std::vector<Message> get_last_n_messages(int64_t chat_id, int n);

    // Update chat status
    void update_chat_status(int64_t chat_id, int64_t last_message_id, int64_t last_message_timestamp);

    // File metadata caching
    void cache_file_item(int64_t chat_id, const FileListItem& item);
    void cache_file_list(int64_t chat_id, const std::vector<FileListItem>& files);
    std::vector<FileListItem> get_cached_file_list(int64_t chat_id, std::optional<MediaType> type = std::nullopt);

    // Cache invalidation
    void invalidate_chat_messages(int64_t chat_id);
    void invalidate_chat_files(int64_t chat_id);
    void invalidate_chat(int64_t chat_id);
    void clear_all();

    // Database maintenance
    void vacuum();
    void cleanup_old_messages(int64_t older_than_timestamp);

    // Chat message statistics
    void update_chat_message_stats(const ChatMessageStats& stats);
    std::optional<ChatMessageStats> get_chat_message_stats(int64_t chat_id);
    std::vector<ChatMessageStats> get_all_chat_message_stats();

    // Get messages within time range for formatting (sorted by timestamp ASC)
    std::vector<Message> get_messages_for_display(int64_t chat_id, int64_t max_age_seconds);

    // Evict old messages from SQLite for a specific chat
    void evict_old_messages(int64_t chat_id, int64_t older_than_timestamp);

private:
    void init_database();
    void create_tables();

    sqlite3* db_;
    std::mutex mutex_;  // Protect concurrent access
};

}  // namespace tg
