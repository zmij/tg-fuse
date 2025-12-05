#include "fuse/telegram_provider.hpp"

#include "fuse/constants.hpp"
#include "fuse/message_formatter.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace tgfuse {

TelegramDataProvider::TelegramDataProvider(tg::TelegramClient& client)
    : client_(client),
      users_loaded_(false),
      groups_loaded_(false),
      channels_loaded_(false),
      messages_cache_(std::make_unique<FormattedMessagesCache>()) {
    setup_message_callback();
}

void TelegramDataProvider::refresh_users() {
    try {
        // TDLib tasks are thread-safe; get_result() blocks until completion
        auto users_task = client_.get_users();
        auto users_list = users_task.get_result();

        // Mutex protects the users_ map during modification
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
    // users_loaded_ is std::atomic<bool>, so this check is thread-safe.
    // Multiple threads may enter refresh_users() concurrently on first access,
    // but the mutex inside refresh_users() ensures only one actually populates the cache.
    if (!users_loaded_) {
        refresh_users();
    }
}

void TelegramDataProvider::ensure_current_user_loaded() {
    // Check without lock first for performance
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (current_user_.has_value()) {
            return;
        }
    }

    try {
        auto me_task = client_.get_me();
        auto me = me_task.get_result();

        std::lock_guard<std::mutex> lock(mutex_);
        current_user_ = std::move(me);
        spdlog::debug("Loaded current user: {}", current_user_->display_name());
    } catch (const std::exception& e) {
        spdlog::error("Failed to get current user: {}", e.what());
    }
}

std::filesystem::path TelegramDataProvider::sanitise_for_path(const std::string& name) const {
    std::string result;
    result.reserve(name.size());

    // Helper to check if a Unicode codepoint is an emoji
    auto is_emoji = [](uint32_t cp) -> bool {
        // Common emoji ranges
        if (cp >= 0x1F600 && cp <= 0x1F64F) return true;  // Emoticons
        if (cp >= 0x1F300 && cp <= 0x1F5FF) return true;  // Misc Symbols and Pictographs
        if (cp >= 0x1F680 && cp <= 0x1F6FF) return true;  // Transport and Map
        if (cp >= 0x1F700 && cp <= 0x1F77F) return true;  // Alchemical Symbols
        if (cp >= 0x1F780 && cp <= 0x1F7FF) return true;  // Geometric Shapes Extended
        if (cp >= 0x1F800 && cp <= 0x1F8FF) return true;  // Supplemental Arrows-C
        if (cp >= 0x1F900 && cp <= 0x1F9FF) return true;  // Supplemental Symbols and Pictographs
        if (cp >= 0x1FA00 && cp <= 0x1FA6F) return true;  // Chess Symbols
        if (cp >= 0x1FA70 && cp <= 0x1FAFF) return true;  // Symbols and Pictographs Extended-A
        if (cp >= 0x2600 && cp <= 0x26FF) return true;    // Misc symbols
        if (cp >= 0x2700 && cp <= 0x27BF) return true;    // Dingbats
        if (cp >= 0x231A && cp <= 0x231B) return true;    // Watch, hourglass
        if (cp >= 0x23E9 && cp <= 0x23F3) return true;    // Media control symbols
        if (cp >= 0x23F8 && cp <= 0x23FA) return true;    // More media symbols
        if (cp >= 0x25AA && cp <= 0x25AB) return true;    // Small squares
        if (cp >= 0x25B6 && cp <= 0x25C0) return true;    // Play/reverse buttons
        if (cp >= 0x25FB && cp <= 0x25FE) return true;    // Medium squares
        if (cp >= 0x2614 && cp <= 0x2615) return true;    // Umbrella, hot beverage
        if (cp >= 0x2648 && cp <= 0x2653) return true;    // Zodiac signs
        if (cp >= 0x267F && cp <= 0x267F) return true;    // Wheelchair
        if (cp >= 0x2693 && cp <= 0x2693) return true;    // Anchor
        if (cp >= 0x26A1 && cp <= 0x26A1) return true;    // High voltage
        if (cp >= 0x26AA && cp <= 0x26AB) return true;    // White/black circles
        if (cp >= 0x26BD && cp <= 0x26BE) return true;    // Sports
        if (cp >= 0x26C4 && cp <= 0x26C5) return true;    // Snowman, sun
        if (cp >= 0x26CE && cp <= 0x26CE) return true;    // Ophiuchus
        if (cp >= 0x26D4 && cp <= 0x26D4) return true;    // No entry
        if (cp >= 0x26EA && cp <= 0x26EA) return true;    // Church
        if (cp >= 0x26F2 && cp <= 0x26F3) return true;    // Fountain, golf
        if (cp >= 0x26F5 && cp <= 0x26F5) return true;    // Sailboat
        if (cp >= 0x26FA && cp <= 0x26FA) return true;    // Tent
        if (cp >= 0x26FD && cp <= 0x26FD) return true;    // Fuel pump
        if (cp >= 0x2702 && cp <= 0x2702) return true;    // Scissors
        if (cp >= 0x2705 && cp <= 0x2705) return true;    // Check mark
        if (cp >= 0x2708 && cp <= 0x270D) return true;    // Airplane to writing hand
        if (cp >= 0x270F && cp <= 0x270F) return true;    // Pencil
        if (cp >= 0x2712 && cp <= 0x2712) return true;    // Black nib
        if (cp >= 0x2714 && cp <= 0x2714) return true;    // Check mark
        if (cp >= 0x2716 && cp <= 0x2716) return true;    // X mark
        if (cp >= 0x271D && cp <= 0x271D) return true;    // Latin cross
        if (cp >= 0x2721 && cp <= 0x2721) return true;    // Star of David
        if (cp >= 0x2728 && cp <= 0x2728) return true;    // Sparkles
        if (cp >= 0x2733 && cp <= 0x2734) return true;    // Eight spoked asterisk
        if (cp >= 0x2744 && cp <= 0x2744) return true;    // Snowflake
        if (cp >= 0x2747 && cp <= 0x2747) return true;    // Sparkle
        if (cp >= 0x274C && cp <= 0x274C) return true;    // Cross mark
        if (cp >= 0x274E && cp <= 0x274E) return true;    // Cross mark
        if (cp >= 0x2753 && cp <= 0x2755) return true;    // Question marks
        if (cp >= 0x2757 && cp <= 0x2757) return true;    // Exclamation mark
        if (cp >= 0x2763 && cp <= 0x2764) return true;    // Heart
        if (cp >= 0x2795 && cp <= 0x2797) return true;    // Plus, minus, division
        if (cp >= 0x27A1 && cp <= 0x27A1) return true;    // Right arrow
        if (cp >= 0x27B0 && cp <= 0x27B0) return true;    // Curly loop
        if (cp >= 0x27BF && cp <= 0x27BF) return true;    // Double curly loop
        if (cp >= 0x2934 && cp <= 0x2935) return true;    // Arrows
        if (cp >= 0x2B05 && cp <= 0x2B07) return true;    // Arrows
        if (cp >= 0x2B1B && cp <= 0x2B1C) return true;    // Squares
        if (cp >= 0x2B50 && cp <= 0x2B50) return true;    // Star
        if (cp >= 0x2B55 && cp <= 0x2B55) return true;    // Circle
        if (cp >= 0x3030 && cp <= 0x3030) return true;    // Wavy dash
        if (cp >= 0x303D && cp <= 0x303D) return true;    // Part alternation mark
        if (cp >= 0x3297 && cp <= 0x3297) return true;    // Circled ideograph congratulation
        if (cp >= 0x3299 && cp <= 0x3299) return true;    // Circled ideograph secret
        if (cp >= 0x1F1E0 && cp <= 0x1F1FF) return true;  // Regional indicators (flags)
        if (cp >= 0x1F004 && cp <= 0x1F0CF) return true;  // Mahjong, cards
        if (cp >= 0xFE00 && cp <= 0xFE0F) return true;    // Variation selectors
        if (cp >= 0x200D && cp <= 0x200D) return true;    // Zero-width joiner
        return false;
    };

    size_t i = 0;
    while (i < name.size()) {
        unsigned char c = name[i];

        // ASCII character
        if (c < 0x80) {
            if (c == '/' || c == '\0') {
                result += '_';
            } else {
                result += static_cast<char>(c);
            }
            ++i;
            continue;
        }

        // Decode UTF-8 codepoint
        uint32_t codepoint = 0;
        size_t bytes = 0;

        if ((c & 0xE0) == 0xC0) {
            // 2-byte sequence
            codepoint = c & 0x1F;
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte sequence
            codepoint = c & 0x0F;
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte sequence
            codepoint = c & 0x07;
            bytes = 4;
        } else {
            // Invalid UTF-8, skip byte
            ++i;
            continue;
        }

        // Check if we have enough bytes
        if (i + bytes > name.size()) {
            break;
        }

        // Read continuation bytes
        bool valid = true;
        for (size_t j = 1; j < bytes; ++j) {
            unsigned char cb = name[i + j];
            if ((cb & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (cb & 0x3F);
        }

        if (!valid) {
            ++i;
            continue;
        }

        // Skip emojis, keep other characters
        if (!is_emoji(codepoint)) {
            for (size_t j = 0; j < bytes; ++j) {
                result += name[i + j];
            }
        }

        i += bytes;
    }

    // Collapse multiple consecutive spaces into one
    std::string collapsed;
    collapsed.reserve(result.size());
    bool last_was_space = false;
    for (char c : result) {
        if (c == ' ') {
            if (!last_was_space) {
                collapsed += c;
                last_was_space = true;
            }
        } else {
            collapsed += c;
            last_was_space = false;
        }
    }
    result = std::move(collapsed);

    // Trim trailing spaces and dots (problematic on some filesystems)
    while (!result.empty() && (result.back() == ' ' || result.back() == '.')) {
        result.pop_back();
    }

    // Trim leading spaces
    size_t start = 0;
    while (start < result.size() && result[start] == ' ') {
        ++start;
    }
    if (start > 0) {
        result = result.substr(start);
    }

    return std::filesystem::path(result.empty() ? "_" : result);
}

std::string TelegramDataProvider::get_user_dir_name(const tg::User& user) const {
    // Prefer username, fallback to display name (sanitised)
    if (!user.username.empty()) {
        return user.username;
    }
    auto name = user.display_name();
    if (!name.empty()) {
        return sanitise_for_path(name).string();
    }
    return std::to_string(user.id);
}

std::string TelegramDataProvider::make_symlink_target(const std::string& relative_path) const {
    if (mount_point_.empty()) {
        return relative_path;
    }
    // Use std::filesystem::path for proper path concatenation
    std::filesystem::path base(mount_point_);
    return (base / relative_path).string();
}

const tg::User* TelegramDataProvider::find_user_by_dir_name(const std::string& dir_name) const {
    auto it = users_.find(dir_name);
    if (it != users_.end()) {
        return &it->second;
    }
    return nullptr;
}

void TelegramDataProvider::refresh_groups() {
    try {
        auto groups_task = client_.get_groups();
        auto groups_list = groups_task.get_result();

        std::lock_guard<std::mutex> lock(mutex_);
        groups_.clear();

        for (auto& group : groups_list) {
            auto dir_name = get_group_dir_name(group);
            groups_[dir_name] = std::move(group);
        }

        if (!groups_list.empty()) {
            groups_loaded_ = true;
        }
        spdlog::info("Loaded {} groups from Telegram", groups_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh groups: {}", e.what());
    }
}

void TelegramDataProvider::ensure_groups_loaded() {
    if (!groups_loaded_) {
        refresh_groups();
    }
}

std::string TelegramDataProvider::get_group_dir_name(const tg::Chat& chat) const {
    // Prefer username, fallback to title (sanitised)
    if (!chat.username.empty()) {
        return chat.username;
    }
    if (!chat.title.empty()) {
        return sanitise_for_path(chat.title);
    }
    return std::to_string(chat.id);
}

const tg::Chat* TelegramDataProvider::find_group_by_dir_name(const std::string& dir_name) const {
    auto it = groups_.find(dir_name);
    if (it != groups_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string TelegramDataProvider::generate_group_info(const tg::Chat& chat) const {
    std::ostringstream oss;

    // Title
    if (!chat.title.empty()) {
        oss << "Title: " << chat.title << "\n";
    }

    // Username
    if (!chat.username.empty()) {
        oss << "Username: @" << chat.username << "\n";
    }

    // Type
    oss << "Type: " << (chat.type == tg::ChatType::SUPERGROUP ? "supergroup" : "group") << "\n";

    return oss.str();
}

void TelegramDataProvider::refresh_channels() {
    try {
        auto channels_task = client_.get_channels();
        auto channels_list = channels_task.get_result();

        std::lock_guard<std::mutex> lock(mutex_);
        channels_.clear();

        for (auto& channel : channels_list) {
            auto dir_name = get_channel_dir_name(channel);
            channels_[dir_name] = std::move(channel);
        }

        if (!channels_list.empty()) {
            channels_loaded_ = true;
        }
        spdlog::info("Loaded {} channels from Telegram", channels_.size());
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh channels: {}", e.what());
    }
}

void TelegramDataProvider::ensure_channels_loaded() {
    if (!channels_loaded_) {
        refresh_channels();
    }
}

std::string TelegramDataProvider::get_channel_dir_name(const tg::Chat& chat) const {
    // Prefer username, fallback to title (sanitised)
    if (!chat.username.empty()) {
        return chat.username;
    }
    if (!chat.title.empty()) {
        return sanitise_for_path(chat.title);
    }
    return std::to_string(chat.id);
}

const tg::Chat* TelegramDataProvider::find_channel_by_dir_name(const std::string& dir_name) const {
    auto it = channels_.find(dir_name);
    if (it != channels_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::string TelegramDataProvider::generate_channel_info(const tg::Chat& chat) const {
    std::ostringstream oss;

    // Title
    if (!chat.title.empty()) {
        oss << "Title: " << chat.title << "\n";
    }

    // Username
    if (!chat.username.empty()) {
        oss << "Username: @" << chat.username << "\n";
    }

    oss << "Type: channel\n";

    return oss.str();
}

TelegramDataProvider::PathInfo TelegramDataProvider::parse_path(std::string_view path) const {
    PathInfo info;

    if (path.empty() || path == "/") {
        info.category = PathCategory::ROOT;
        return info;
    }

    // Use std::filesystem::path for normalisation and component parsing
    std::filesystem::path fs_path(path);
    fs_path = fs_path.lexically_normal();

    // Collect path components (skip root)
    std::vector<std::string> components;
    for (const auto& component : fs_path) {
        auto str = component.string();
        if (str != "/" && !str.empty()) {
            components.push_back(str);
        }
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

    // Check for /self symlink
    if (components.size() == 1 && components[0] == kSelfSymlink) {
        info.category = PathCategory::SELF_SYMLINK;
        return info;
    }

    // Check for top-level directories
    if (components[0] == kUsersDir) {
        if (components.size() == 1) {
            info.category = PathCategory::USERS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::USER_DIR;
            info.entity_name = components[1];
        } else if (components.size() == 3) {
            info.entity_name = components[1];
            if (components[2] == kInfoFile) {
                info.category = PathCategory::USER_INFO;
            } else if (components[2] == kMessagesFile) {
                info.category = PathCategory::USER_MESSAGES;
            }
        }
    } else if (components[0] == kContactsDir) {
        if (components.size() == 1) {
            info.category = PathCategory::CONTACTS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::CONTACT_SYMLINK;
            info.entity_name = components[1];
        }
    } else if (components[0] == kGroupsDir) {
        if (components.size() == 1) {
            info.category = PathCategory::GROUPS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::GROUP_DIR;
            info.entity_name = components[1];
        } else if (components.size() == 3) {
            info.entity_name = components[1];
            if (components[2] == kInfoFile) {
                info.category = PathCategory::GROUP_INFO;
            } else if (components[2] == kMessagesFile) {
                info.category = PathCategory::GROUP_MESSAGES;
            }
        }
    } else if (components[0] == kChannelsDir) {
        if (components.size() == 1) {
            info.category = PathCategory::CHANNELS_DIR;
        } else if (components.size() == 2) {
            info.category = PathCategory::CHANNEL_DIR;
            info.entity_name = components[1];
        } else if (components.size() == 3) {
            info.entity_name = components[1];
            if (components[2] == kInfoFile) {
                info.category = PathCategory::CHANNEL_INFO;
            } else if (components[2] == kMessagesFile) {
                info.category = PathCategory::CHANNEL_MESSAGES;
            }
        }
    }

    return info;
}

std::vector<Entry> TelegramDataProvider::list_directory(std::string_view path) {
    ensure_users_loaded();
    ensure_current_user_loaded();
    ensure_groups_loaded();
    ensure_channels_loaded();

    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Entry> entries;

    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            // Top-level directories
            entries.push_back(Entry::directory(std::string(kUsersDir)));
            entries.push_back(Entry::directory(std::string(kContactsDir)));
            entries.push_back(Entry::directory(std::string(kGroupsDir)));
            entries.push_back(Entry::directory(std::string(kChannelsDir)));
            // Self symlink pointing to current user's directory
            if (current_user_) {
                auto dir_name = get_user_dir_name(*current_user_);
                auto target = (std::filesystem::path(kUsersDir) / dir_name).string();
                entries.push_back(Entry::symlink(std::string(kSelfSymlink), make_symlink_target(target)));
            }
            // User symlinks at root (contacts with usernames only)
            for (const auto& [name, user] : users_) {
                if (is_user_contact(user) && has_username(user)) {
                    auto target = (std::filesystem::path(kUsersDir) / name).string();
                    entries.push_back(Entry::symlink("@" + user.username, make_symlink_target(target)));
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
                    auto target = (std::filesystem::path(kUsersDir) / name).string();
                    entries.push_back(Entry::symlink(name, make_symlink_target(target)));
                }
            }
            break;

        case PathCategory::USER_DIR: {
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user) {
                // .info file
                auto info_entry = Entry::file(std::string(kInfoFile), 4096);
                if (user->last_message_timestamp > 0) {
                    info_entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    info_entry.atime = info_entry.mtime;
                    info_entry.ctime = info_entry.mtime;
                }
                entries.push_back(std::move(info_entry));

                // messages file (mode 0600 = owner read/write only)
                auto msg_entry = Entry::file(std::string(kMessagesFile), estimate_messages_size(user->id), 0600);
                if (user->last_message_timestamp > 0) {
                    msg_entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    msg_entry.atime = msg_entry.mtime;
                    msg_entry.ctime = msg_entry.mtime;
                }
                entries.push_back(std::move(msg_entry));
            }
            break;
        }

        case PathCategory::GROUPS_DIR:
            for (const auto& [name, group] : groups_) {
                auto entry = Entry::directory(name);
                if (group.last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(group.last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                entries.push_back(std::move(entry));
            }
            break;

        case PathCategory::GROUP_DIR: {
            auto* group = find_group_by_dir_name(info.entity_name);
            if (group) {
                // .info file
                auto info_entry = Entry::file(std::string(kInfoFile), 4096);
                if (group->last_message_timestamp > 0) {
                    info_entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    info_entry.atime = info_entry.mtime;
                    info_entry.ctime = info_entry.mtime;
                }
                entries.push_back(std::move(info_entry));

                // messages file (mode 0600 = owner read/write only)
                auto msg_entry = Entry::file(std::string(kMessagesFile), estimate_messages_size(group->id), 0600);
                if (group->last_message_timestamp > 0) {
                    msg_entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    msg_entry.atime = msg_entry.mtime;
                    msg_entry.ctime = msg_entry.mtime;
                }
                entries.push_back(std::move(msg_entry));
            }
            break;
        }

        case PathCategory::CHANNELS_DIR:
            for (const auto& [name, channel] : channels_) {
                auto entry = Entry::directory(name);
                if (channel.last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(channel.last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                entries.push_back(std::move(entry));
            }
            break;

        case PathCategory::CHANNEL_DIR: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            if (channel) {
                // .info file
                auto info_entry = Entry::file(std::string(kInfoFile), 4096);
                if (channel->last_message_timestamp > 0) {
                    info_entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
                    info_entry.atime = info_entry.mtime;
                    info_entry.ctime = info_entry.mtime;
                }
                entries.push_back(std::move(info_entry));

                // messages file (mode 0600 = owner read/write only)
                auto msg_entry = Entry::file(std::string(kMessagesFile), estimate_messages_size(channel->id), 0600);
                if (channel->last_message_timestamp > 0) {
                    msg_entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
                    msg_entry.atime = msg_entry.mtime;
                    msg_entry.ctime = msg_entry.mtime;
                }
                entries.push_back(std::move(msg_entry));
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
    ensure_current_user_loaded();
    ensure_groups_loaded();
    ensure_channels_loaded();

    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            return Entry::directory("");

        case PathCategory::USERS_DIR:
            return Entry::directory(std::string(kUsersDir));

        case PathCategory::CONTACTS_DIR:
            return Entry::directory(std::string(kContactsDir));

        case PathCategory::GROUPS_DIR:
            return Entry::directory(std::string(kGroupsDir));

        case PathCategory::CHANNELS_DIR:
            return Entry::directory(std::string(kChannelsDir));

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
                // Use a large fixed size since content is generated dynamically
                // (bio and other fields are fetched lazily in read_file)
                auto entry = Entry::file(std::string(kInfoFile), 4096);
                if (user->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::GROUP_DIR: {
            auto* group = find_group_by_dir_name(info.entity_name);
            if (group) {
                auto entry = Entry::directory(info.entity_name);
                if (group->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::GROUP_INFO: {
            auto* group = find_group_by_dir_name(info.entity_name);
            if (group) {
                auto entry = Entry::file(std::string(kInfoFile), 4096);
                if (group->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::CHANNEL_DIR: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            if (channel) {
                auto entry = Entry::directory(info.entity_name);
                if (channel->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::CHANNEL_INFO: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            if (channel) {
                auto entry = Entry::file(std::string(kInfoFile), 4096);
                if (channel->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::USER_MESSAGES: {
            auto* user = find_user_by_dir_name(info.entity_name);
            if (user) {
                auto entry = Entry::file("messages", estimate_messages_size(user->id), 0600);
                if (user->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::GROUP_MESSAGES: {
            auto* group = find_group_by_dir_name(info.entity_name);
            if (group) {
                auto entry = Entry::file("messages", estimate_messages_size(group->id), 0600);
                if (group->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                }
                return entry;
            }
            break;
        }

        case PathCategory::CHANNEL_MESSAGES: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            if (channel) {
                auto entry = Entry::file("messages", estimate_messages_size(channel->id), 0600);
                if (channel->last_message_timestamp > 0) {
                    entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
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
                auto target = (std::filesystem::path(kUsersDir) / info.entity_name).string();
                return Entry::symlink(info.entity_name, make_symlink_target(target));
            }
            break;
        }

        case PathCategory::ROOT_SYMLINK: {
            // Root symlinks are only for users with actual usernames
            // info.entity_name is the username (without @)
            // Find user by username, not by dir_name
            for (const auto& [dir_name, user] : users_) {
                if (user.username == info.entity_name && is_user_contact(user)) {
                    auto target = (std::filesystem::path(kUsersDir) / dir_name).string();
                    return Entry::symlink("@" + user.username, make_symlink_target(target));
                }
            }
            break;
        }

        case PathCategory::SELF_SYMLINK: {
            if (current_user_) {
                auto dir_name = get_user_dir_name(*current_user_);
                auto target = (std::filesystem::path(kUsersDir) / dir_name).string();
                return Entry::symlink(std::string(kSelfSymlink), make_symlink_target(target));
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
    ensure_groups_loaded();
    ensure_channels_loaded();

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
    } else if (info.category == PathCategory::GROUP_INFO) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* group = find_group_by_dir_name(info.entity_name);
        if (group) {
            content.data = generate_group_info(*group);
            content.readable = true;
        }
    } else if (info.category == PathCategory::CHANNEL_INFO) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* channel = find_channel_by_dir_name(info.entity_name);
        if (channel) {
            content.data = generate_channel_info(*channel);
            content.readable = true;
        }
    } else if (is_messages_path(info.category)) {
        int64_t chat_id = get_chat_id_from_path(info);
        if (chat_id != 0) {
            content.data = fetch_and_format_messages(chat_id);
            content.readable = true;
        }
    }

    return content;
}

std::string TelegramDataProvider::read_link(std::string_view path) {
    ensure_current_user_loaded();

    std::lock_guard<std::mutex> lock(mutex_);
    auto info = parse_path(path);

    if (info.category == PathCategory::ROOT_SYMLINK) {
        // Find user by username
        for (const auto& [dir_name, user] : users_) {
            if (user.username == info.entity_name && is_user_contact(user)) {
                auto target = (std::filesystem::path(kUsersDir) / dir_name).string();
                return make_symlink_target(target);
            }
        }
    } else if (info.category == PathCategory::CONTACT_SYMLINK) {
        auto* user = find_user_by_dir_name(info.entity_name);
        if (user && is_user_contact(*user)) {
            auto target = (std::filesystem::path(kUsersDir) / info.entity_name).string();
            return make_symlink_target(target);
        }
    } else if (info.category == PathCategory::SELF_SYMLINK) {
        if (current_user_) {
            auto dir_name = get_user_dir_name(*current_user_);
            auto target = (std::filesystem::path(kUsersDir) / dir_name).string();
            return make_symlink_target(target);
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

bool TelegramDataProvider::is_messages_path(PathCategory category) const {
    return category == PathCategory::USER_MESSAGES || category == PathCategory::GROUP_MESSAGES ||
           category == PathCategory::CHANNEL_MESSAGES;
}

int64_t TelegramDataProvider::get_chat_id_from_path(const PathInfo& info) const {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (info.category) {
        case PathCategory::USER_MESSAGES: {
            auto* user = find_user_by_dir_name(info.entity_name);
            return user ? user->id : 0;
        }
        case PathCategory::GROUP_MESSAGES: {
            auto* group = find_group_by_dir_name(info.entity_name);
            return group ? group->id : 0;
        }
        case PathCategory::CHANNEL_MESSAGES: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            return channel ? channel->id : 0;
        }
        default:
            return 0;
    }
}

std::size_t TelegramDataProvider::estimate_messages_size(int64_t chat_id) const {
    // First check the formatted messages cache
    std::size_t cached_size = messages_cache_->get_content_size(chat_id);
    if (cached_size > 0) {
        return cached_size;
    }

    // Fall back to estimate from raw message cache
    auto cached = client_.cache().get_last_n_messages(chat_id, 10);
    if (cached.empty()) {
        return MessageFormatter::DEFAULT_FALLBACK_SIZE;  // Default fallback for display
    }
    return MessageFormatter::estimate_size(cached.size());
}

std::string TelegramDataProvider::fetch_and_format_messages(int64_t chat_id) {
    // Try to get from cache first
    auto cached = messages_cache_->get(chat_id);
    if (cached) {
        spdlog::debug("fetch_and_format_messages: cache hit for chat {}, size {}", chat_id, cached->size());
        return std::string(*cached);
    }

    // Cache miss - fetch and populate
    try {
        auto task = client_.get_messages(chat_id, 10);
        auto messages = task.get_result();

        if (messages.empty()) {
            return "";
        }

        // Populate the cache
        messages_cache_->populate(chat_id, messages, make_sender_resolver());

        // Return the cached content
        cached = messages_cache_->get(chat_id);
        if (cached) {
            return std::string(*cached);
        }
        return "";
    } catch (const std::exception& e) {
        spdlog::error("Failed to fetch messages for chat {}: {}", chat_id, e.what());
        return "";
    }
}

SenderResolver TelegramDataProvider::make_sender_resolver() const {
    return [this](int64_t sender_id) -> SenderInfo {
        SenderInfo info;

        // Try to find sender in users cache
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, user] : users_) {
            if (user.id == sender_id) {
                info.display_name = user.display_name();
                info.username = user.username;
                return info;
            }
        }

        // Fallback for unknown senders
        info.display_name = "User " + std::to_string(sender_id);
        return info;
    };
}

void TelegramDataProvider::setup_message_callback() {
    client_.set_message_callback([this](const tg::Message& message) {
        // Append new message to cache if chat is cached
        if (messages_cache_->contains(message.chat_id)) {
            messages_cache_->append_message(message.chat_id, message, make_sender_resolver());
            spdlog::debug("Appended message {} to cache for chat {}", message.id, message.chat_id);
        }
    });
}

bool TelegramDataProvider::is_writable(std::string_view path) const {
    auto info = parse_path(path);
    return is_messages_path(info.category);
}

bool TelegramDataProvider::is_append_only(std::string_view path) const {
    return is_writable(path);  // All writable files are append-only
}

int TelegramDataProvider::truncate_file(std::string_view path, off_t size) {
    auto info = parse_path(path);

    // Append-only files: only allow truncate to current size (no-op) or zero
    if (is_messages_path(info.category)) {
        if (size == 0) {
            // Truncate to zero is allowed (it's a no-op for append-only)
            return 0;
        }
        // Other truncates are not permitted
        return -EPERM;
    }

    return -EACCES;  // Not writable
}

WriteResult TelegramDataProvider::write_file(std::string_view path, const char* data, std::size_t size, off_t offset) {
    auto info = parse_path(path);

    if (!is_messages_path(info.category)) {
        return WriteResult{false, 0, "Path is not writable"};
    }

    int64_t chat_id = get_chat_id_from_path(info);
    if (chat_id == 0) {
        return WriteResult{false, 0, "Chat not found"};
    }

    // Get content size from the formatted messages cache
    std::size_t current_size = messages_cache_->get_content_size(chat_id);
    spdlog::debug("write_file: offset={}, size={}, cached_content_size={}", offset, size, current_size);

    // If cache is empty (current_size == 0), treat all content as new
    if (current_size == 0) {
        spdlog::debug("write_file: no cached messages, sending all content");
        return send_message(chat_id, data, size);
    }

    // If writing at offset 0 with size larger than current content,
    // the shell likely read the file, appended new content, and wrote everything back.
    // Extract just the new content beyond the current size.
    if (offset == 0 && size > current_size) {
        const char* new_content = data + current_size;
        std::size_t new_size = size - current_size;
        spdlog::debug("write_file: extracting new content at offset {}, size {}", current_size, new_size);
        return send_message(chat_id, new_content, new_size);
    }

    // If offset is beyond current content, ignore (likely stale data)
    if (static_cast<std::size_t>(offset) > current_size) {
        spdlog::debug("write_file: ignoring write at offset {} beyond content size {}", offset, current_size);
        return WriteResult{true, static_cast<int>(size), ""};
    }

    // Writing within existing content - ignore (it's a rewrite of existing messages)
    spdlog::debug("write_file: ignoring write within existing content");
    return WriteResult{true, static_cast<int>(size), ""};
}

WriteResult TelegramDataProvider::send_message(int64_t chat_id, const char* data, std::size_t size) {
    spdlog::debug("send_message called: chat_id={}, size={}", chat_id, size);

    // Empty writes are no-ops (success)
    if (size == 0) {
        spdlog::debug("send_message: empty write, returning success");
        return WriteResult{true, 0, ""};
    }

    // Validate text content (reject binary data)
    if (!MessageFormatter::is_valid_text(data, size)) {
        spdlog::warn("Rejected binary data write to chat {}", chat_id);
        return WriteResult{false, 0, "Binary data not allowed"};
    }

    // Convert to string and trim trailing newlines
    std::string text(data, size);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }

    if (text.empty()) {
        spdlog::debug("send_message: text empty after trimming newlines, returning success");
        return WriteResult{true, static_cast<int>(size), ""};
    }

    spdlog::debug("send_message: sending text '{}'", text.substr(0, 100));

    // Split message if too large
    auto chunks = MessageFormatter::split_message(text);

    try {
        for (const auto& chunk : chunks) {
            auto task = client_.send_text(chat_id, chunk);
            task.get_result();  // Wait for completion
        }

        spdlog::debug("Sent {} message(s) to chat {}", chunks.size(), chat_id);
        return WriteResult{true, static_cast<int>(size), ""};
    } catch (const std::exception& e) {
        spdlog::error("Failed to send message to chat {}: {}", chat_id, e.what());
        return WriteResult{false, 0, e.what()};
    }
}

}  // namespace tgfuse
