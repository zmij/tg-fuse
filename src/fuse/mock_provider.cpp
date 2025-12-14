#include "fuse/mock_provider.hpp"

#include <algorithm>
#include <sstream>

namespace tgfuse {

MockDataProvider::MockDataProvider() { populate_default_data(); }

void MockDataProvider::populate_default_data() {
    // Add default mock users
    users_["alice"] = MockUser{
        .username = "alice",
        .display_name = "Alice Smith",
        .user_id = 123456789,
        .status = "Online",
        .last_seen = "Recently"
    };
    users_["bob"] = MockUser{
        .username = "bob",
        .display_name = "Bob Johnson",
        .user_id = 234567890,
        .status = "Offline",
        .last_seen = "Last seen yesterday at 18:30"
    };
    users_["charlie"] = MockUser{
        .username = "charlie",
        .display_name = "Charlie Brown",
        .user_id = 345678901,
        .status = "Online",
        .last_seen = "Recently"
    };

    // Add default mock groups
    groups_["family"] = MockGroup{
        .name = "family",
        .title = "Family Chat",
        .group_id = -1001234567890,
        .member_count = 5,
        .description = "Family group chat"
    };
    groups_["work"] = MockGroup{
        .name = "work",
        .title = "Work Team",
        .group_id = -1001234567891,
        .member_count = 12,
        .description = "Work team discussions"
    };

    // Add default mock channels
    channels_["news_channel"] = MockChannel{
        .name = "news_channel",
        .title = "Daily News",
        .channel_id = -1009876543210,
        .subscriber_count = 1500,
        .description = "Daily news updates"
    };
    channels_["tech_updates"] = MockChannel{
        .name = "tech_updates",
        .title = "Tech Updates",
        .channel_id = -1009876543211,
        .subscriber_count = 850,
        .description = "Technology news and updates"
    };
}

void MockDataProvider::add_user(const MockUser& user) {
    std::lock_guard<std::mutex> lock(mutex_);
    users_[user.username] = user;
}

void MockDataProvider::add_group(const MockGroup& group) {
    std::lock_guard<std::mutex> lock(mutex_);
    groups_[group.name] = group;
}

void MockDataProvider::add_channel(const MockChannel& channel) {
    std::lock_guard<std::mutex> lock(mutex_);
    channels_[channel.name] = channel;
}

void MockDataProvider::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.clear();
    groups_.clear();
    channels_.clear();
}

MockDataProvider::PathInfo MockDataProvider::parse_path(std::string_view path) const {
    PathInfo info;

    // Normalise path (remove trailing slash)
    if (path.size() > 1 && path.back() == '/') {
        path = path.substr(0, path.size() - 1);
    }

    if (path.empty() || path == "/") {
        info.category = PathCategory::ROOT;
        return info;
    }

    // Split path into components
    std::vector<std::string> components;
    std::string current;
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                components.push_back(std::move(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        components.push_back(std::move(current));
    }

    if (components.empty()) {
        info.category = PathCategory::ROOT;
        return info;
    }

    // Check for user symlink at root level (starts with @)
    if (components.size() == 1 && !components[0].empty() && components[0][0] == '@') {
        info.category = PathCategory::USER_SYMLINK;
        info.entity_name = components[0].substr(1);  // Remove @
        return info;
    }

    // Check for top-level directories
    if (components[0] == "users") {
        if (components.size() == 1) {
            info.category = PathCategory::USERS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::USER_DIR;
            info.entity_name = components[1];
        } else if (components.size() == 3 && components[2] == ".info") {
            info.category = PathCategory::USER_INFO;
            info.entity_name = components[1];
        }
    } else if (components[0] == "groups") {
        if (components.size() == 1) {
            info.category = PathCategory::GROUPS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::GROUP_DIR;
            info.entity_name = components[1];
        } else if (components.size() == 3 && components[2] == ".info") {
            info.category = PathCategory::GROUP_INFO;
            info.entity_name = components[1];
        }
    } else if (components[0] == "channels") {
        if (components.size() == 1) {
            info.category = PathCategory::CHANNELS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::CHANNEL_DIR;
            info.entity_name = components[1];
        } else if (components.size() == 3 && components[2] == ".info") {
            info.category = PathCategory::CHANNEL_INFO;
            info.entity_name = components[1];
        }
    }

    return info;
}

std::vector<Entry> MockDataProvider::list_directory(std::string_view path) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Entry> entries;

    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            // Top-level directories
            entries.push_back(Entry::directory("users"));
            entries.push_back(Entry::directory("groups"));
            entries.push_back(Entry::directory("channels"));
            // User symlinks at root
            for (const auto& [name, user] : users_) {
                entries.push_back(Entry::symlink("@" + name, "users/" + name));
            }
            break;

        case PathCategory::USERS_DIR:
            for (const auto& [name, user] : users_) {
                entries.push_back(Entry::directory(name));
            }
            break;

        case PathCategory::GROUPS_DIR:
            for (const auto& [name, group] : groups_) {
                entries.push_back(Entry::directory(name));
            }
            break;

        case PathCategory::CHANNELS_DIR:
            for (const auto& [name, channel] : channels_) {
                entries.push_back(Entry::directory(name));
            }
            break;

        case PathCategory::USER_DIR:
            if (users_.count(info.entity_name)) {
                auto content = generate_user_info(users_.at(info.entity_name));
                entries.push_back(Entry::file(".info", content.size()));
            }
            break;

        case PathCategory::GROUP_DIR:
            if (groups_.count(info.entity_name)) {
                auto content = generate_group_info(groups_.at(info.entity_name));
                entries.push_back(Entry::file(".info", content.size()));
            }
            break;

        case PathCategory::CHANNEL_DIR:
            if (channels_.count(info.entity_name)) {
                auto content = generate_channel_info(channels_.at(info.entity_name));
                entries.push_back(Entry::file(".info", content.size()));
            }
            break;

        default:
            break;
    }

    return entries;
}

std::optional<Entry> MockDataProvider::get_entry(std::string_view path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            return Entry::directory("");

        case PathCategory::USERS_DIR:
            return Entry::directory("users");

        case PathCategory::GROUPS_DIR:
            return Entry::directory("groups");

        case PathCategory::CHANNELS_DIR:
            return Entry::directory("channels");

        case PathCategory::USER_DIR:
            if (users_.count(info.entity_name)) {
                return Entry::directory(info.entity_name);
            }
            break;

        case PathCategory::GROUP_DIR:
            if (groups_.count(info.entity_name)) {
                return Entry::directory(info.entity_name);
            }
            break;

        case PathCategory::CHANNEL_DIR:
            if (channels_.count(info.entity_name)) {
                return Entry::directory(info.entity_name);
            }
            break;

        case PathCategory::USER_INFO:
            if (users_.count(info.entity_name)) {
                auto content = generate_user_info(users_.at(info.entity_name));
                return Entry::file(".info", content.size());
            }
            break;

        case PathCategory::GROUP_INFO:
            if (groups_.count(info.entity_name)) {
                auto content = generate_group_info(groups_.at(info.entity_name));
                return Entry::file(".info", content.size());
            }
            break;

        case PathCategory::CHANNEL_INFO:
            if (channels_.count(info.entity_name)) {
                auto content = generate_channel_info(channels_.at(info.entity_name));
                return Entry::file(".info", content.size());
            }
            break;

        case PathCategory::USER_SYMLINK:
            if (users_.count(info.entity_name)) {
                return Entry::symlink("@" + info.entity_name, "users/" + info.entity_name);
            }
            break;

        default:
            break;
    }

    return std::nullopt;
}

bool MockDataProvider::exists(std::string_view path) { return get_entry(path).has_value(); }

bool MockDataProvider::is_directory(std::string_view path) {
    auto entry = get_entry(path);
    return entry.has_value() && entry->is_directory();
}

bool MockDataProvider::is_symlink(std::string_view path) {
    auto entry = get_entry(path);
    return entry.has_value() && entry->is_symlink();
}

FileContent MockDataProvider::read_file(std::string_view path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    FileContent content;
    content.readable = false;

    switch (info.category) {
        case PathCategory::USER_INFO:
            if (users_.count(info.entity_name)) {
                content.data = generate_user_info(users_.at(info.entity_name));
                content.readable = true;
            }
            break;

        case PathCategory::GROUP_INFO:
            if (groups_.count(info.entity_name)) {
                content.data = generate_group_info(groups_.at(info.entity_name));
                content.readable = true;
            }
            break;

        case PathCategory::CHANNEL_INFO:
            if (channels_.count(info.entity_name)) {
                content.data = generate_channel_info(channels_.at(info.entity_name));
                content.readable = true;
            }
            break;

        default:
            break;
    }

    return content;
}

std::string MockDataProvider::read_link(std::string_view path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    if (info.category == PathCategory::USER_SYMLINK && users_.count(info.entity_name)) {
        return "users/" + info.entity_name;
    }

    return "";
}

std::string MockDataProvider::generate_user_info(const MockUser& user) const {
    std::ostringstream oss;
    oss << "Username: " << user.username << "\n";
    oss << "Display Name: " << user.display_name << "\n";
    oss << "User ID: " << user.user_id << "\n";
    oss << "Status: " << user.status << "\n";
    oss << "Last seen: " << user.last_seen << "\n";
    return oss.str();
}

std::string MockDataProvider::generate_group_info(const MockGroup& group) const {
    std::ostringstream oss;
    oss << "Group: " << group.name << "\n";
    oss << "Title: " << group.title << "\n";
    oss << "Group ID: " << group.group_id << "\n";
    oss << "Members: " << group.member_count << "\n";
    oss << "Description: " << group.description << "\n";
    return oss.str();
}

std::string MockDataProvider::generate_channel_info(const MockChannel& channel) const {
    std::ostringstream oss;
    oss << "Channel: " << channel.name << "\n";
    oss << "Title: " << channel.title << "\n";
    oss << "Channel ID: " << channel.channel_id << "\n";
    oss << "Subscribers: " << channel.subscriber_count << "\n";
    oss << "Description: " << channel.description << "\n";
    return oss.str();
}

}  // namespace tgfuse
