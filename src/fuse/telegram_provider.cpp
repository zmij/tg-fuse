#include "fuse/telegram_provider.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>

namespace tgfuse {

TelegramDataProvider::TelegramDataProvider(tg::TelegramClient& client) : client_(client), users_loaded_(false) {}

void TelegramDataProvider::refresh_users() {
    try {
        auto users_task = client_.get_users();
        auto users_list = users_task.get_result();

        std::lock_guard<std::mutex> lock(mutex_);
        users_.clear();

        for (auto& user : users_list) {
            auto dir_name = get_user_dir_name(user);
            users_[dir_name] = std::move(user);
        }

        // Only mark as fully loaded if we got some users
        // Otherwise allow retry on next access
        if (!users_list.empty()) {
            users_loaded_ = true;
        }
        spdlog::info("Loaded {} users from Telegram", users_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh users: {}", e.what());
        // Don't mark as loaded on error - allow retry
    }
}

void TelegramDataProvider::ensure_users_loaded() {
    if (!users_loaded_) {
        refresh_users();
    }
}

std::string TelegramDataProvider::sanitise_for_path(const std::string& name) const {
    std::string result;
    result.reserve(name.size());

    for (char c : name) {
        // Replace problematic filesystem characters
        if (c == '/' || c == '\0') {
            result += '_';
        } else {
            result += c;
        }
    }

    // Trim trailing spaces and dots (problematic on some filesystems)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    return result.empty() ? "_" : result;
}

std::string TelegramDataProvider::get_user_dir_name(const tg::User& user) const {
    // Prefer username, fallback to display name (sanitised)
    if (!user.username.empty()) {
        return user.username;
    }
    auto name = user.display_name();
    if (!name.empty()) {
        return sanitise_for_path(name);
    }
    return std::to_string(user.id);
}

std::string TelegramDataProvider::make_symlink_target(const std::string& relative_path) const {
    if (mount_point_.empty()) {
        return relative_path;
    }
    // Build absolute path: mount_point + "/" + relative_path
    if (mount_point_.back() == '/') {
        return mount_point_ + relative_path;
    }
    return mount_point_ + "/" + relative_path;
}

const tg::User* TelegramDataProvider::find_user_by_dir_name(const std::string& dir_name) const {
    auto it = users_.find(dir_name);
    if (it != users_.end()) {
        return &it->second;
    }
    return nullptr;
}

TelegramDataProvider::PathInfo TelegramDataProvider::parse_path(std::string_view path) const {
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
        info.category = PathCategory::ROOT_SYMLINK;
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
    } else if (components[0] == "contacts") {
        if (components.size() == 1) {
            info.category = PathCategory::CONTACTS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::CONTACT_SYMLINK;
            info.entity_name = components[1];
        }
    }

    return info;
}

std::vector<Entry> TelegramDataProvider::list_directory(std::string_view path) {
    ensure_users_loaded();

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Entry> entries;

    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            // Top-level directories
            entries.push_back(Entry::directory("users"));
            entries.push_back(Entry::directory("contacts"));
            // User symlinks at root (contacts with usernames only)
            for (const auto& [name, user] : users_) {
                if (is_user_contact(user) && has_username(user)) {
                    entries.push_back(Entry::symlink("@" + user.username, make_symlink_target("users/" + name)));
                }
            }
            break;

        case PathCategory::USERS_DIR:
            for (const auto& [name, user] : users_) {
                auto entry = Entry::directory(name);
                // Set mtime to last message timestamp
                if (user.last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(user.last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                entries.push_back(std::move(entry));
            }
            break;

        case PathCategory::CONTACTS_DIR:
            // Symlinks to users directory for contacts only
            for (const auto& [name, user] : users_) {
                if (is_user_contact(user)) {
                    entries.push_back(Entry::symlink(name, make_symlink_target("users/" + name)));
                }
            }
            break;

        case PathCategory::USER_DIR: {
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user) {
                auto content = generate_user_info(*user);
                auto entry = Entry::file(".info", content.size());
                if (user->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                entries.push_back(std::move(entry));
            }
            break;
        }

        default:
            break;
    }

    return entries;
}

std::optional<Entry> TelegramDataProvider::get_entry(std::string_view path) {
    ensure_users_loaded();

    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            return Entry::directory("");

        case PathCategory::USERS_DIR:
            return Entry::directory("users");

        case PathCategory::CONTACTS_DIR:
            return Entry::directory("contacts");

        case PathCategory::USER_DIR: {
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user) {
                auto entry = Entry::directory(info.entity_name);
                if (user->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::USER_INFO: {
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user) {
                auto content = generate_user_info(*user);
                auto entry = Entry::file(".info", content.size());
                if (user->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::CONTACT_SYMLINK: {
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user && is_user_contact(*user)) {
                return Entry::symlink(info.entity_name, make_symlink_target("users/" + info.entity_name));
            }
            break;
        }

        case PathCategory::ROOT_SYMLINK: {
            // Root symlinks are only for users with actual usernames
            // info.entity_name is the username (without @)
            // Find user by username, not by dir_name
            for (const auto& [dir_name, user] : users_) {
                if (user.username == info.entity_name && is_user_contact(user)) {
                    return Entry::symlink("@" + user.username, make_symlink_target("users/" + dir_name));
                }
            }
            break;
        }

        default:
            break;
    }

    return std::nullopt;
}

bool TelegramDataProvider::exists(std::string_view path) { return get_entry(path).has_value(); }

bool TelegramDataProvider::is_directory(std::string_view path) {
    auto entry = get_entry(path);
    return entry.has_value() && entry->is_directory();
}

bool TelegramDataProvider::is_symlink(std::string_view path) {
    auto entry = get_entry(path);
    return entry.has_value() && entry->is_symlink();
}

FileContent TelegramDataProvider::read_file(std::string_view path) {
    ensure_users_loaded();

    auto info = parse_path(path);

    FileContent content;
    content.readable = false;

    if (info.category == PathCategory::USER_INFO) {
        tg::User user_copy;
        bool found = false;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user) {
                user_copy = *user;
                found = true;
            }
        }

        if (found) {
            // Fetch full user info lazily if not already loaded
            if (user_copy.phone_number.empty() && user_copy.status == tg::UserStatus::UNKNOWN) {
                try {
                    auto user_task = client_.get_user(user_copy.id);
                    auto full_user = user_task.get_result();
                    if (full_user) {
                        // Preserve last_message info from chat
                        full_user->last_message_id = user_copy.last_message_id;
                        full_user->last_message_timestamp = user_copy.last_message_timestamp;
                        user_copy = *full_user;

                        // Cache for future reads
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = users_.find(info.entity_name);
                        if (it != users_.end()) {
                            it->second = user_copy;
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::debug("Failed to fetch user info for {}: {}", user_copy.id, e.what());
                }
            }

            // Fetch bio lazily if not already loaded
            if (user_copy.bio.empty()) {
                try {
                    auto bio_task = client_.get_user_bio(user_copy.id);
                    user_copy.bio = bio_task.get_result();

                    // Cache the bio for future reads
                    if (!user_copy.bio.empty()) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        auto it = users_.find(info.entity_name);
                        if (it != users_.end()) {
                            it->second.bio = user_copy.bio;
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::debug("Failed to fetch bio for user {}: {}", user_copy.id, e.what());
                }
            }

            content.data = generate_user_info(user_copy);
            content.readable = true;
        }
    }

    return content;
}

std::string TelegramDataProvider::read_link(std::string_view path) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    if (info.category == PathCategory::ROOT_SYMLINK) {
        // Find user by username
        for (const auto& [dir_name, user] : users_) {
            if (user.username == info.entity_name && is_user_contact(user)) {
                return make_symlink_target("users/" + dir_name);
            }
        }
    } else if (info.category == PathCategory::CONTACT_SYMLINK) {
        auto* user = find_user_by_dir_name(info.entity_name);
        if (user && is_user_contact(*user)) {
            return make_symlink_target("users/" + info.entity_name);
        }
    }

    return "";
}

std::string TelegramDataProvider::generate_user_info(const tg::User& user) const {
    std::ostringstream oss;

    // Username
    if (!user.username.empty()) {
        oss << "Username: @" << user.username << "\n";
    }

    // Name
    auto name = user.display_name();
    if (!name.empty()) {
        oss << "Name: " << name << "\n";
    }

    // Bio
    if (!user.bio.empty()) {
        oss << "Bio: " << user.bio << "\n";
    }

    // Phone
    if (!user.phone_number.empty()) {
        oss << "Phone: " << user.phone_number << "\n";
    }

    // Last seen
    oss << "Last seen: " << user.get_last_seen_string() << "\n";

    return oss.str();
}

}  // namespace tgfuse
