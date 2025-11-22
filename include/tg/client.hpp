#pragma once

#include "tg/async.hpp"
#include "tg/cache.hpp"
#include "tg/types.hpp"

#include <memory>
#include <string>
#include <vector>

namespace tg {

class TelegramClient {
public:
    // Configuration for the client
    struct Config {
        int32_t api_id;
        std::string api_hash;
        std::string database_directory;
        std::string files_directory;
        bool use_test_dc = false;           // Use test data center
        bool use_file_database = true;
        bool use_chat_info_database = true;
        bool use_message_database = true;
        bool enable_storage_optimiser = true;
    };

    explicit TelegramClient(const Config& config);
    ~TelegramClient();

    // Disable copy
    TelegramClient(const TelegramClient&) = delete;
    TelegramClient& operator=(const TelegramClient&) = delete;

    // Initialisation & lifecycle
    Task<void> start();
    Task<void> stop();

    // Authentication
    Task<AuthState> get_auth_state();
    Task<void> login(const std::string& phone);
    Task<void> submit_code(const std::string& code);
    Task<void> submit_password(const std::string& password);
    Task<void> logout();

    // Entity listing
    Task<std::vector<User>> get_users();
    Task<std::vector<Chat>> get_groups();
    Task<std::vector<Chat>> get_channels();
    Task<std::vector<Chat>> get_all_chats();

    // Entity lookup
    Task<std::optional<Chat>> resolve_username(const std::string& username);
    Task<std::optional<Chat>> get_chat(int64_t chat_id);
    Task<std::optional<User>> get_user(int64_t user_id);

    // Messaging
    Task<Message> send_text(int64_t chat_id, const std::string& text);
    Task<std::vector<Message>> get_messages(int64_t chat_id, int limit = 100);
    Task<std::vector<Message>> get_last_n_messages(int64_t chat_id, int n);

    // File operations
    Task<Message> send_file(int64_t chat_id, const std::string& path, SendMode mode = SendMode::AUTO);
    Task<std::vector<FileListItem>> list_media(int64_t chat_id);
    Task<std::vector<FileListItem>> list_files(int64_t chat_id);
    Task<std::string> download_file(const std::string& file_id, const std::string& destination_path = "");

    // Chat status polling
    Task<ChatStatus> get_chat_status(int64_t chat_id);

    // Cache access
    CacheManager& cache() { return *cache_; }
    const CacheManager& cache() const { return *cache_; }

private:
    class Impl;
    Config config_;
    std::unique_ptr<CacheManager> cache_;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tg
