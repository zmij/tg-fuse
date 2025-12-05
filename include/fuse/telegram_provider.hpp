#pragma once

#include "fuse/data_provider.hpp"
#include "tg/client.hpp"
#include "tg/types.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
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

    /// Refresh user cache from Telegram
    void refresh_users();

private:
    /// Path category enumeration
    enum class PathCategory {
        NOT_FOUND,  // Default value - path not recognised
        ROOT,
        USERS_DIR,        // /users
        CONTACTS_DIR,     // /contacts
        GROUPS_DIR,       // /groups
        USER_DIR,         // /users/alice
        USER_INFO,        // /users/alice/.info
        GROUP_DIR,        // /groups/dev_chat
        GROUP_INFO,       // /groups/dev_chat/.info
        CONTACT_SYMLINK,  // /contacts/alice
        ROOT_SYMLINK,     // /@alice
        SELF_SYMLINK      // /self
    };

    /// Parsed path information
    struct PathInfo {
        PathCategory category{PathCategory::NOT_FOUND};
        std::string entity_name;  // Username or display name
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

    tg::TelegramClient& client_;

    // Cached user data (keyed by directory name)
    std::map<std::string, tg::User> users_;
    mutable std::atomic<bool> users_loaded_;        // mutable for const-correctness with lazy loading
    mutable std::optional<tg::User> current_user_;  // Cached current user for /self symlink

    // Cached group data (keyed by directory name)
    std::map<std::string, tg::Chat> groups_;
    mutable std::atomic<bool> groups_loaded_;

    mutable std::mutex mutex_;
};

}  // namespace tgfuse
