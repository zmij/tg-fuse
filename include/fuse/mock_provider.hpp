#pragma once

#include "fuse/data_provider.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace tgfuse {

/// Mock user entry
struct MockUser {
    std::string username;
    std::string display_name;
    std::int64_t user_id{0};
    std::string status{"Online"};
    std::string last_seen{"Recently"};
};

/// Mock group entry
struct MockGroup {
    std::string name;
    std::string title;
    std::int64_t group_id{0};
    int member_count{0};
    std::string description;
};

/// Mock channel entry
struct MockChannel {
    std::string name;
    std::string title;
    std::int64_t channel_id{0};
    int subscriber_count{0};
    std::string description;
};

/// Mock data provider implementation
///
/// Provides a virtual filesystem with mock Telegram data for testing
/// and development purposes.
class MockDataProvider : public DataProvider {
public:
    MockDataProvider();
    ~MockDataProvider() override = default;

    // DataProvider interface implementation
    [[nodiscard]] std::vector<Entry> list_directory(std::string_view path) override;
    [[nodiscard]] std::optional<Entry> get_entry(std::string_view path) override;
    [[nodiscard]] bool exists(std::string_view path) override;
    [[nodiscard]] bool is_directory(std::string_view path) override;
    [[nodiscard]] bool is_symlink(std::string_view path) override;
    [[nodiscard]] FileContent read_file(std::string_view path) override;
    [[nodiscard]] std::string read_link(std::string_view path) override;
    [[nodiscard]] std::string get_filesystem_name() const override { return "tg-fuse-mock"; }

    // Mock-specific methods for testing/configuration
    void add_user(const MockUser& user);
    void add_group(const MockGroup& group);
    void add_channel(const MockChannel& channel);
    void clear_all();

private:
    /// Path category enumeration
    enum class PathCategory {
        ROOT,
        USERS_DIR,
        GROUPS_DIR,
        CHANNELS_DIR,
        USER_DIR,
        GROUP_DIR,
        CHANNEL_DIR,
        USER_INFO,
        GROUP_INFO,
        CHANNEL_INFO,
        USER_SYMLINK,
        NOT_FOUND
    };

    /// Parsed path information
    struct PathInfo {
        PathCategory category{PathCategory::NOT_FOUND};
        std::string entity_name;  // Username, group name, or channel name
    };

    /// Parse a path into its components
    [[nodiscard]] PathInfo parse_path(std::string_view path) const;

    /// Generate info content for a user
    [[nodiscard]] std::string generate_user_info(const MockUser& user) const;

    /// Generate info content for a group
    [[nodiscard]] std::string generate_group_info(const MockGroup& group) const;

    /// Generate info content for a channel
    [[nodiscard]] std::string generate_channel_info(const MockChannel& channel) const;

    /// Populate with default mock data
    void populate_default_data();

    // Mock data storage (keyed by name)
    std::map<std::string, MockUser> users_;
    std::map<std::string, MockGroup> groups_;
    std::map<std::string, MockChannel> channels_;

    mutable std::mutex mutex_;
};

}  // namespace tgfuse
