#pragma once

#include "fuse/data_provider.hpp"
#include "fuse/messages_cache.hpp"
#include "tg/client.hpp"
#include "tg/types.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tgfuse {

/// Telegram data provider implementation
///
/// Provides a virtual filesystem backed by real Telegram data.
/// Displays users from private chats with .info files containing
/// user details and last seen status.
class TelegramDataProvider : public DataProvider {
public:
    explicit TelegramDataProvider(tg::TelegramClient& client);
    ~TelegramDataProvider() override = default;

    // DataProvider interface implementation
    [[nodiscard]] std::vector<Entry> list_directory(std::string_view path) override;
    [[nodiscard]] std::optional<Entry> get_entry(std::string_view path) override;
    [[nodiscard]] bool exists(std::string_view path) override;
    [[nodiscard]] bool is_directory(std::string_view path) override;
    [[nodiscard]] bool is_symlink(std::string_view path) override;
    [[nodiscard]] FileContent read_file(std::string_view path) override;
    [[nodiscard]] std::string read_link(std::string_view path) override;
    [[nodiscard]] std::string get_filesystem_name() const override { return "tg-fuse"; }

    // Write operations
    WriteResult write_file(std::string_view path, const char* data, std::size_t size, off_t offset) override;
    int truncate_file(std::string_view path, off_t size) override;
    [[nodiscard]] bool is_writable(std::string_view path) const override;
    [[nodiscard]] bool is_append_only(std::string_view path) const override;

    // File upload operations
    int create_file(std::string_view path, mode_t mode, uint64_t& fh) override;
    WriteResult
    write_file(std::string_view path, const char* data, std::size_t size, off_t offset, uint64_t fh) override;
    int release_file(std::string_view path, uint64_t fh) override;

    /// Refresh user cache from Telegram
    void refresh_users();

private:
    /// Path category enumeration
    enum class PathCategory {
        NOT_FOUND,  // Default value - path not recognised
        ROOT,
        USERS_DIR,          // /users
        CONTACTS_DIR,       // /contacts
        GROUPS_DIR,         // /groups
        CHANNELS_DIR,       // /channels
        USER_DIR,           // /users/alice
        USER_INFO,          // /users/alice/.info
        USER_MESSAGES,      // /users/alice/messages
        USER_FILES_DIR,     // /users/alice/files
        USER_FILE,          // /users/alice/files/20241205-1430-report.pdf
        USER_MEDIA_DIR,     // /users/alice/media
        USER_MEDIA,         // /users/alice/media/20241205-1430-photo.jpg
        GROUP_DIR,          // /groups/dev_chat
        GROUP_INFO,         // /groups/dev_chat/.info
        GROUP_MESSAGES,     // /groups/dev_chat/messages
        GROUP_FILES_DIR,    // /groups/dev_chat/files
        GROUP_FILE,         // /groups/dev_chat/files/...
        GROUP_MEDIA_DIR,    // /groups/dev_chat/media
        GROUP_MEDIA,        // /groups/dev_chat/media/...
        CHANNEL_DIR,        // /channels/news
        CHANNEL_INFO,       // /channels/news/.info
        CHANNEL_MESSAGES,   // /channels/news/messages
        CHANNEL_FILES_DIR,  // /channels/news/files
        CHANNEL_FILE,       // /channels/news/files/...
        CHANNEL_MEDIA_DIR,  // /channels/news/media
        CHANNEL_MEDIA,      // /channels/news/media/...
        CONTACT_SYMLINK,    // /contacts/alice
        ROOT_SYMLINK,       // /@alice
        SELF_SYMLINK,       // /self
        UPLOADS_DIR,        // /.uploads (lists pending uploads)
        // Upload categories (for cp operations)
        USER_UPLOAD,    // /users/alice/newfile.txt (auto-detect)
        GROUP_UPLOAD,   // /groups/chat/newfile.pdf (auto-detect)
        CHANNEL_UPLOAD  // /channels/news/newfile.mp4 (auto-detect)
    };

    /// Parsed path information
    struct PathInfo {
        PathCategory category{PathCategory::NOT_FOUND};
        std::string entity_name;      // Username or display name
        std::string file_entry_name;  // For file paths: "20241205-1430-report.pdf"
    };

    /// Parse a path into its components
    [[nodiscard]] PathInfo parse_path(std::string_view path) const;

    /// Generate info content for a user
    [[nodiscard]] std::string generate_user_info(const tg::User& user) const;

    /// Get directory name for a user (username or id)
    [[nodiscard]] std::string get_user_dir_name(const tg::User& user) const;

    /// Find user by directory name
    [[nodiscard]] const tg::User* find_user_by_dir_name(const std::string& dir_name) const;

    /// Check if user is a contact
    [[nodiscard]] bool is_user_contact(const tg::User& user) const { return user.is_contact; }

    /// Check if user has a username
    [[nodiscard]] bool has_username(const tg::User& user) const { return !user.username.empty(); }

    /// Sanitise a string for use as a filesystem path component
    [[nodiscard]] std::filesystem::path sanitise_for_path(const std::string& name) const;

    /// Build symlink target path (absolute if mount point is set)
    [[nodiscard]] std::string make_symlink_target(const std::string& relative_path) const;

    /// Ensure users are loaded (lazy loading)
    void ensure_users_loaded();

    /// Ensure current user is loaded (lazy loading)
    void ensure_current_user_loaded();

    /// Refresh group cache from Telegram
    void refresh_groups();

    /// Ensure groups are loaded (lazy loading)
    void ensure_groups_loaded();

    /// Get directory name for a group (username or sanitised title)
    [[nodiscard]] std::string get_group_dir_name(const tg::Chat& chat) const;

    /// Find group by directory name
    [[nodiscard]] const tg::Chat* find_group_by_dir_name(const std::string& dir_name) const;

    /// Generate info content for a group
    [[nodiscard]] std::string generate_group_info(const tg::Chat& chat) const;

    /// Refresh channel cache from Telegram
    void refresh_channels();

    /// Ensure channels are loaded (lazy loading)
    void ensure_channels_loaded();

    /// Get directory name for a channel (username or sanitised title)
    [[nodiscard]] std::string get_channel_dir_name(const tg::Chat& chat) const;

    /// Find channel by directory name
    [[nodiscard]] const tg::Chat* find_channel_by_dir_name(const std::string& dir_name) const;

    /// Generate info content for a channel
    [[nodiscard]] std::string generate_channel_info(const tg::Chat& chat) const;

    /// Fetch and format messages for a chat
    [[nodiscard]] std::string fetch_and_format_messages(int64_t chat_id);

    /// Format messages and store in cache
    [[nodiscard]] std::string format_and_cache_messages(int64_t chat_id, const std::vector<tg::Message>& messages);

    /// Get chat ID from path info
    [[nodiscard]] int64_t get_chat_id_from_path(const PathInfo& info) const;

    /// Check if a path category is a messages file
    [[nodiscard]] bool is_messages_path(PathCategory category) const;

    /// Check if a path category is a files directory
    [[nodiscard]] bool is_files_dir_path(PathCategory category) const;

    /// Check if a path category is a file entry
    [[nodiscard]] bool is_file_path(PathCategory category) const;

    /// Check if a path category is a media directory
    [[nodiscard]] bool is_media_dir_path(PathCategory category) const;

    /// Check if a path category is a media entry
    [[nodiscard]] bool is_media_path(PathCategory category) const;

    /// Estimate messages file size from cache
    [[nodiscard]] std::size_t estimate_messages_size(int64_t chat_id) const;

    /// Format a file entry name with timestamp prefix (YYYYMMDD-HHMM-filename)
    [[nodiscard]] std::string format_file_entry_name(const tg::FileListItem& item) const;

    /// Parse a file entry name back to original filename and timestamp
    [[nodiscard]] std::optional<std::pair<std::string, int64_t>> parse_file_entry_name(
        const std::string& entry_name
    ) const;

    /// Find a FileListItem by its formatted entry name
    [[nodiscard]] const tg::FileListItem* find_file_by_entry_name(int64_t chat_id, const std::string& entry_name);

    /// Ensure files are loaded for a chat (lazy loading from API)
    void ensure_files_loaded(int64_t chat_id);

    /// Get chat ID from path info for files categories
    [[nodiscard]] int64_t get_chat_id_for_files(const PathInfo& info) const;

    /// Send a message to a chat, handling large messages
    [[nodiscard]] WriteResult send_message(int64_t chat_id, const char* data, std::size_t size);

    /// Download a file and read its content
    [[nodiscard]] FileContent download_and_read_file(const tg::FileListItem& file);

    /// Check if a path category is an upload target
    [[nodiscard]] bool is_upload_path(PathCategory category) const;

    /// Get chat ID for upload operations
    [[nodiscard]] int64_t get_chat_id_for_upload(const PathInfo& info) const;

    /// Check if a file extension is valid for media/ directory
    [[nodiscard]] bool is_valid_media_extension(const std::string& filename) const;

    /// Extract original filename (strip timestamp prefix if present)
    [[nodiscard]] std::string extract_original_filename(const std::string& entry_name) const;

    /// Get temp directory for uploads
    [[nodiscard]] std::filesystem::path get_upload_temp_dir() const;

    /// Action to take when uploading a file
    enum class UploadAction { SEND_AS_TEXT, SEND_AS_MEDIA, SEND_AS_DOCUMENT };

    /// Detect upload action based on file content/extension
    [[nodiscard]] UploadAction detect_upload_action(const std::string& path, const std::string& filename) const;

    /// Check if file contains valid UTF-8 text
    [[nodiscard]] bool is_valid_text_file(const std::string& path) const;

    /// Send file content as text message(s)
    [[nodiscard]] int send_file_as_text(int64_t chat_id, const std::string& path);

    /// Compute SHA256 hash of a file
    [[nodiscard]] std::string compute_file_hash(const std::string& path) const;

    /// Send file using cached remote file ID
    void send_file_by_remote_id(
        int64_t chat_id,
        const std::string& remote_file_id,
        const std::string& filename,
        tg::SendMode mode
    );

    tg::TelegramClient& client_;

    // Cached user data (keyed by directory name)
    std::map<std::string, tg::User> users_;
    mutable std::atomic<bool> users_loaded_;        // mutable for const-correctness with lazy loading
    mutable std::optional<tg::User> current_user_;  // Cached current user for /self symlink

    // Cached group data (keyed by directory name)
    std::map<std::string, tg::Chat> groups_;
    mutable std::atomic<bool> groups_loaded_;

    // Cached channel data (keyed by directory name)
    std::map<std::string, tg::Chat> channels_;
    std::atomic<bool> channels_loaded_;

    // Formatted messages cache (RCU-style, updated on message notifications)
    std::unique_ptr<FormattedMessagesCache> messages_cache_;

    // Pending file upload tracking
    struct PendingUpload {
        std::string temp_path;
        std::string original_filename;
        std::string virtual_path;  // Full virtual path for getattr lookups
        int64_t chat_id;
        tg::SendMode mode;
        std::size_t bytes_written{0};
    };
    std::map<uint64_t, PendingUpload> pending_uploads_;
    mutable std::mutex uploads_mutex_;
    std::atomic<uint64_t> next_upload_handle_{1};

    // Recently completed uploads (kept briefly for post-release operations like setxattr)
    struct CompletedUpload {
        std::string filename;
        std::size_t size;
        std::chrono::steady_clock::time_point completed_at;
    };
    std::map<std::string, CompletedUpload> completed_uploads_;  // keyed by virtual_path

    /// Find a pending upload by virtual path
    [[nodiscard]] const PendingUpload* find_pending_upload_by_path(std::string_view path) const;

    /// Find a recently completed upload by virtual path
    [[nodiscard]] const CompletedUpload* find_completed_upload_by_path(std::string_view path) const;

    /// Mark an upload as completed (moves from pending to completed)
    void mark_upload_completed(const std::string& virtual_path, const std::string& filename, std::size_t size);

    /// Clean up old completed uploads (called periodically)
    void cleanup_completed_uploads();

    /// Add pending and completed uploads to directory listing for a given directory path
    void add_uploads_to_listing(std::string_view dir_path, std::vector<Entry>& entries) const;

    mutable std::mutex mutex_;

    /// Create user resolver function for message formatting
    [[nodiscard]] UserResolver make_user_resolver() const;

    /// Create chat resolver function for message formatting
    [[nodiscard]] ChatResolver make_chat_resolver() const;

    /// Set up message callback to update cache on new messages
    void setup_message_callback();
};

}  // namespace tgfuse
