#pragma once

#include "tg/types.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace tg {

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

private:
    void init_database();
    void create_tables();

    sqlite3* db_;
    std::mutex mutex_;  // Protect concurrent access
};

}  // namespace tg
