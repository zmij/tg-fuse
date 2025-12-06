#include "fuse/telegram_provider.hpp"

#include "fuse/constants.hpp"
#include "fuse/message_formatter.hpp"
#include "tg/bustache_formatters.hpp"
#include "tg/formatters.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <bustache/render/string.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
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

    // Check for /.uploads directory
    if (components.size() == 1 && components[0] == kUploadsDir) {
        info.category = PathCategory::UPLOADS_DIR;
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
            } else if (components[2] == kFilesDir) {
                info.category = PathCategory::USER_FILES_DIR;
            } else if (components[2] == kMediaDir) {
                info.category = PathCategory::USER_MEDIA_DIR;
            } else {
                // Unknown file in user directory - could be an upload
                info.file_entry_name = components[2];
                info.category = PathCategory::USER_UPLOAD;
            }
        } else if (components.size() == 4 && components[2] == kFilesDir) {
            info.entity_name = components[1];
            info.file_entry_name = components[3];
            info.category = PathCategory::USER_FILE;
        } else if (components.size() == 4 && components[2] == kMediaDir) {
            info.entity_name = components[1];
            info.file_entry_name = components[3];
            info.category = PathCategory::USER_MEDIA;
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
            } else if (components[2] == kFilesDir) {
                info.category = PathCategory::GROUP_FILES_DIR;
            } else if (components[2] == kMediaDir) {
                info.category = PathCategory::GROUP_MEDIA_DIR;
            } else {
                // Unknown file in group directory - could be an upload
                info.file_entry_name = components[2];
                info.category = PathCategory::GROUP_UPLOAD;
            }
        } else if (components.size() == 4 && components[2] == kFilesDir) {
            info.entity_name = components[1];
            info.file_entry_name = components[3];
            info.category = PathCategory::GROUP_FILE;
        } else if (components.size() == 4 && components[2] == kMediaDir) {
            info.entity_name = components[1];
            info.file_entry_name = components[3];
            info.category = PathCategory::GROUP_MEDIA;
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
            } else if (components[2] == kFilesDir) {
                info.category = PathCategory::CHANNEL_FILES_DIR;
            } else if (components[2] == kMediaDir) {
                info.category = PathCategory::CHANNEL_MEDIA_DIR;
            } else {
                // Unknown file in channel directory - could be an upload
                info.file_entry_name = components[2];
                info.category = PathCategory::CHANNEL_UPLOAD;
            }
        } else if (components.size() == 4 && components[2] == kFilesDir) {
            info.entity_name = components[1];
            info.file_entry_name = components[3];
            info.category = PathCategory::CHANNEL_FILE;
        } else if (components.size() == 4 && components[2] == kMediaDir) {
            info.entity_name = components[1];
            info.file_entry_name = components[3];
            info.category = PathCategory::CHANNEL_MEDIA;
        }
    }

    return info;
}

std::vector<Entry> TelegramDataProvider::list_directory(std::string_view path) {
    ensure_users_loaded();
    ensure_current_user_loaded();
    ensure_groups_loaded();
    ensure_channels_loaded();

    std::unique_lock<std::mutex> lock(mutex_);
    std::vector<Entry> entries;

    auto info = parse_path(path);

    switch (info.category) {
        case PathCategory::ROOT:
            // Top-level directories
            entries.push_back(Entry::directory(std::string(kUsersDir)));
            entries.push_back(Entry::directory(std::string(kContactsDir)));
            entries.push_back(Entry::directory(std::string(kGroupsDir)));
            entries.push_back(Entry::directory(std::string(kChannelsDir)));
            entries.push_back(Entry::directory(std::string(kUploadsDir)));
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

        case PathCategory::UPLOADS_DIR: {
            // List pending uploads from temp directory
            std::lock_guard<std::mutex> upload_lock(uploads_mutex_);
            for (const auto& [fh, upload] : pending_uploads_) {
                auto entry = Entry::file(upload.original_filename, upload.bytes_written, 0644);
                entry.mtime = std::time(nullptr);
                entry.atime = entry.mtime;
                entry.ctime = entry.mtime;
                entries.push_back(std::move(entry));
            }
            break;
        }

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

                // files directory
                auto files_entry = Entry::directory(std::string(kFilesDir));
                if (user->last_message_timestamp > 0) {
                    files_entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    files_entry.atime = files_entry.mtime;
                    files_entry.ctime = files_entry.mtime;
                }
                entries.push_back(std::move(files_entry));

                // media directory
                auto media_entry = Entry::directory(std::string(kMediaDir));
                if (user->last_message_timestamp > 0) {
                    media_entry.mtime = static_cast<std::time_t>(user->last_message_timestamp);
                    media_entry.atime = media_entry.mtime;
                    media_entry.ctime = media_entry.mtime;
                }
                entries.push_back(std::move(media_entry));

                // Add pending/completed uploads in this directory
                add_uploads_to_listing(path, entries);
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

                // files directory
                auto files_entry = Entry::directory(std::string(kFilesDir));
                if (group->last_message_timestamp > 0) {
                    files_entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    files_entry.atime = files_entry.mtime;
                    files_entry.ctime = files_entry.mtime;
                }
                entries.push_back(std::move(files_entry));

                // media directory
                auto media_entry = Entry::directory(std::string(kMediaDir));
                if (group->last_message_timestamp > 0) {
                    media_entry.mtime = static_cast<std::time_t>(group->last_message_timestamp);
                    media_entry.atime = media_entry.mtime;
                    media_entry.ctime = media_entry.mtime;
                }
                entries.push_back(std::move(media_entry));

                // Add pending/completed uploads in this directory
                add_uploads_to_listing(path, entries);
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

                // files directory
                auto files_entry = Entry::directory(std::string(kFilesDir));
                if (channel->last_message_timestamp > 0) {
                    files_entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
                    files_entry.atime = files_entry.mtime;
                    files_entry.ctime = files_entry.mtime;
                }
                entries.push_back(std::move(files_entry));

                // media directory
                auto media_entry = Entry::directory(std::string(kMediaDir));
                if (channel->last_message_timestamp > 0) {
                    media_entry.mtime = static_cast<std::time_t>(channel->last_message_timestamp);
                    media_entry.atime = media_entry.mtime;
                    media_entry.ctime = media_entry.mtime;
                }
                entries.push_back(std::move(media_entry));

                // Add pending/completed uploads in this directory
                add_uploads_to_listing(path, entries);
            }
            break;
        }

        case PathCategory::USER_FILES_DIR:
        case PathCategory::GROUP_FILES_DIR:
        case PathCategory::CHANNEL_FILES_DIR: {
            // Get chat ID while holding lock
            int64_t chat_id = 0;
            if (info.category == PathCategory::USER_FILES_DIR) {
                auto* user = find_user_by_dir_name(info.entity_name);
                chat_id = user ? user->id : 0;
            } else if (info.category == PathCategory::GROUP_FILES_DIR) {
                auto* group = find_group_by_dir_name(info.entity_name);
                chat_id = group ? group->id : 0;
            } else {
                auto* channel = find_channel_by_dir_name(info.entity_name);
                chat_id = channel ? channel->id : 0;
            }

            if (chat_id != 0) {
                // Get cached files
                auto files = client_.cache().get_cached_file_list(chat_id);

                // If no cached files, release lock and fetch from API
                if (files.empty()) {
                    lock.unlock();
                    ensure_files_loaded(chat_id);
                    lock.lock();
                    files = client_.cache().get_cached_file_list(chat_id);
                }

                for (const auto& file : files) {
                    // Only show documents in files/ directory (not photos/videos)
                    if (!tg::is_document_type(file.type)) {
                        continue;
                    }
                    auto entry = Entry::file(
                        format_file_entry_name(file), static_cast<std::size_t>(file.file_size > 0 ? file.file_size : 0)
                    );
                    entry.mtime = static_cast<std::time_t>(file.timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                    entries.push_back(std::move(entry));
                }

                // Add pending/completed uploads in this directory
                add_uploads_to_listing(path, entries);
            }
            break;
        }

        case PathCategory::USER_MEDIA_DIR:
        case PathCategory::GROUP_MEDIA_DIR:
        case PathCategory::CHANNEL_MEDIA_DIR: {
            // Get chat ID while holding lock
            int64_t chat_id = 0;
            if (info.category == PathCategory::USER_MEDIA_DIR) {
                auto* user = find_user_by_dir_name(info.entity_name);
                chat_id = user ? user->id : 0;
            } else if (info.category == PathCategory::GROUP_MEDIA_DIR) {
                auto* group = find_group_by_dir_name(info.entity_name);
                chat_id = group ? group->id : 0;
            } else {
                auto* channel = find_channel_by_dir_name(info.entity_name);
                chat_id = channel ? channel->id : 0;
            }

            if (chat_id != 0) {
                // Get cached files
                auto files = client_.cache().get_cached_file_list(chat_id);

                // If no cached files, release lock and fetch from API
                if (files.empty()) {
                    lock.unlock();
                    ensure_files_loaded(chat_id);
                    lock.lock();
                    files = client_.cache().get_cached_file_list(chat_id);
                }

                for (const auto& file : files) {
                    // Only show media in media/ directory (photos/videos/animations)
                    if (!tg::is_media_type(file.type)) {
                        continue;
                    }
                    auto entry = Entry::file(
                        format_file_entry_name(file), static_cast<std::size_t>(file.file_size > 0 ? file.file_size : 0)
                    );
                    entry.mtime = static_cast<std::time_t>(file.timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                    entries.push_back(std::move(entry));
                }

                // Add pending/completed uploads in this directory
                add_uploads_to_listing(path, entries);
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

    std::unique_lock<std::mutex> lock(mutex_);
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

        case PathCategory::UPLOADS_DIR:
            return Entry::directory(std::string(kUploadsDir));

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

        case PathCategory::USER_FILES_DIR:
        case PathCategory::GROUP_FILES_DIR:
        case PathCategory::CHANNEL_FILES_DIR: {
            // Verify the parent entity exists
            bool exists = false;
            if (info.category == PathCategory::USER_FILES_DIR) {
                exists = find_user_by_dir_name(info.entity_name) != nullptr;
            } else if (info.category == PathCategory::GROUP_FILES_DIR) {
                exists = find_group_by_dir_name(info.entity_name) != nullptr;
            } else {
                exists = find_channel_by_dir_name(info.entity_name) != nullptr;
            }
            if (exists) {
                return Entry::directory(std::string(kFilesDir));
            }
            break;
        }

        case PathCategory::USER_FILE:
        case PathCategory::GROUP_FILE:
        case PathCategory::CHANNEL_FILE: {
            // Get chat ID while holding lock
            int64_t chat_id = 0;
            if (info.category == PathCategory::USER_FILE) {
                auto* user = find_user_by_dir_name(info.entity_name);
                chat_id = user ? user->id : 0;
            } else if (info.category == PathCategory::GROUP_FILE) {
                auto* group = find_group_by_dir_name(info.entity_name);
                chat_id = group ? group->id : 0;
            } else {
                auto* channel = find_channel_by_dir_name(info.entity_name);
                chat_id = channel ? channel->id : 0;
            }

            if (chat_id != 0) {
                // Check cache first
                auto files = client_.cache().get_cached_file_list(chat_id);
                if (files.empty()) {
                    // Release lock and fetch from API
                    lock.unlock();
                    ensure_files_loaded(chat_id);
                    lock.lock();
                    files = client_.cache().get_cached_file_list(chat_id);
                }

                // Find the file by entry name
                auto* file = find_file_by_entry_name(chat_id, info.file_entry_name);
                // Only return documents (not photos/videos)
                if (file && tg::is_document_type(file->type)) {
                    auto entry = Entry::file(
                        format_file_entry_name(*file),
                        static_cast<std::size_t>(file->file_size > 0 ? file->file_size : 0)
                    );
                    entry.mtime = static_cast<std::time_t>(file->timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                    return entry;
                }
            }
            break;
        }

        case PathCategory::USER_MEDIA_DIR:
        case PathCategory::GROUP_MEDIA_DIR:
        case PathCategory::CHANNEL_MEDIA_DIR: {
            // Verify the parent entity exists
            bool exists = false;
            if (info.category == PathCategory::USER_MEDIA_DIR) {
                exists = find_user_by_dir_name(info.entity_name) != nullptr;
            } else if (info.category == PathCategory::GROUP_MEDIA_DIR) {
                exists = find_group_by_dir_name(info.entity_name) != nullptr;
            } else {
                exists = find_channel_by_dir_name(info.entity_name) != nullptr;
            }
            if (exists) {
                return Entry::directory(std::string(kMediaDir));
            }
            break;
        }

        case PathCategory::USER_MEDIA:
        case PathCategory::GROUP_MEDIA:
        case PathCategory::CHANNEL_MEDIA: {
            // Get chat ID while holding lock
            int64_t chat_id = 0;
            if (info.category == PathCategory::USER_MEDIA) {
                auto* user = find_user_by_dir_name(info.entity_name);
                chat_id = user ? user->id : 0;
            } else if (info.category == PathCategory::GROUP_MEDIA) {
                auto* group = find_group_by_dir_name(info.entity_name);
                chat_id = group ? group->id : 0;
            } else {
                auto* channel = find_channel_by_dir_name(info.entity_name);
                chat_id = channel ? channel->id : 0;
            }

            if (chat_id != 0) {
                // Check cache first
                auto files = client_.cache().get_cached_file_list(chat_id);
                if (files.empty()) {
                    // Release lock and fetch from API
                    lock.unlock();
                    ensure_files_loaded(chat_id);
                    lock.lock();
                    files = client_.cache().get_cached_file_list(chat_id);
                }

                // Find the file by entry name
                auto* file = find_file_by_entry_name(chat_id, info.file_entry_name);
                // Only return media (photos/videos/animations)
                if (file && tg::is_media_type(file->type)) {
                    auto entry = Entry::file(
                        format_file_entry_name(*file),
                        static_cast<std::size_t>(file->file_size > 0 ? file->file_size : 0)
                    );
                    entry.mtime = static_cast<std::time_t>(file->timestamp);
                    entry.atime = entry.mtime;
                    entry.ctime = entry.mtime;
                    return entry;
                }
            }
            break;
        }

        default:
            break;
    }

    // Check if this is a pending upload (file being created)
    // We need to release the main lock before checking uploads_mutex_ to avoid deadlock
    lock.unlock();
    if (auto* upload = find_pending_upload_by_path(path)) {
        // Return a synthetic entry for the file being uploaded
        auto filename = std::filesystem::path(upload->virtual_path).filename().string();
        auto entry = Entry::file(filename, upload->bytes_written, 0644);
        entry.mtime = std::time(nullptr);
        entry.atime = entry.mtime;
        entry.ctime = entry.mtime;
        return entry;
    }

    // Check if this is a recently completed upload (for post-release operations like setxattr)
    if (auto* completed = find_completed_upload_by_path(path)) {
        auto entry = Entry::file(completed->filename, completed->size, 0644);
        entry.mtime = std::time(nullptr);
        entry.atime = entry.mtime;
        entry.ctime = entry.mtime;
        return entry;
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
    } else if (is_file_path(info.category)) {
        int64_t chat_id = get_chat_id_for_files(info);
        if (chat_id != 0) {
            ensure_files_loaded(chat_id);
            auto* file = find_file_by_entry_name(chat_id, info.file_entry_name);
            if (file && tg::is_document_type(file->type)) {
                content = download_and_read_file(*file);
            }
        }
    } else if (is_media_path(info.category)) {
        int64_t chat_id = get_chat_id_for_files(info);
        if (chat_id != 0) {
            ensure_files_loaded(chat_id);
            auto* file = find_file_by_entry_name(chat_id, info.file_entry_name);
            if (file && tg::is_media_type(file->type)) {
                content = download_and_read_file(*file);
            }
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

bool TelegramDataProvider::is_files_dir_path(PathCategory category) const {
    return category == PathCategory::USER_FILES_DIR || category == PathCategory::GROUP_FILES_DIR ||
           category == PathCategory::CHANNEL_FILES_DIR;
}

bool TelegramDataProvider::is_file_path(PathCategory category) const {
    return category == PathCategory::USER_FILE || category == PathCategory::GROUP_FILE ||
           category == PathCategory::CHANNEL_FILE;
}

bool TelegramDataProvider::is_media_dir_path(PathCategory category) const {
    return category == PathCategory::USER_MEDIA_DIR || category == PathCategory::GROUP_MEDIA_DIR ||
           category == PathCategory::CHANNEL_MEDIA_DIR;
}

bool TelegramDataProvider::is_media_path(PathCategory category) const {
    return category == PathCategory::USER_MEDIA || category == PathCategory::GROUP_MEDIA ||
           category == PathCategory::CHANNEL_MEDIA;
}

std::string TelegramDataProvider::format_file_entry_name(const tg::FileListItem& item) const {
    std::time_t time = static_cast<std::time_t>(item.timestamp);
    std::tm* tm = std::localtime(&time);
    return fmt::format(
        "{:04d}{:02d}{:02d}-{:02d}{:02d}-{}",
        tm->tm_year + 1900,
        tm->tm_mon + 1,
        tm->tm_mday,
        tm->tm_hour,
        tm->tm_min,
        item.filename
    );
}

std::optional<std::pair<std::string, int64_t>> TelegramDataProvider::parse_file_entry_name(
    const std::string& entry_name
) const {
    // Format: YYYYMMDD-HHMM-filename
    // Minimum length: 8 + 1 + 4 + 1 + 1 = 15 (e.g., "20241205-1430-a")
    if (entry_name.size() < 15) {
        return std::nullopt;
    }

    // Check format: YYYYMMDD-HHMM-
    if (entry_name[8] != '-' || entry_name[13] != '-') {
        return std::nullopt;
    }

    // Parse date and time
    try {
        int year = std::stoi(entry_name.substr(0, 4));
        int month = std::stoi(entry_name.substr(4, 2));
        int day = std::stoi(entry_name.substr(6, 2));
        int hour = std::stoi(entry_name.substr(9, 2));
        int minute = std::stoi(entry_name.substr(11, 2));

        // Extract filename
        std::string filename = entry_name.substr(14);

        // Convert to timestamp
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_isdst = -1;

        int64_t timestamp = static_cast<int64_t>(std::mktime(&tm));
        return std::make_pair(filename, timestamp);
    } catch (...) {
        return std::nullopt;
    }
}

const tg::FileListItem* TelegramDataProvider::find_file_by_entry_name(int64_t chat_id, const std::string& entry_name) {
    // Parse the entry name to get original filename and timestamp
    auto parsed = parse_file_entry_name(entry_name);
    if (!parsed) {
        return nullptr;
    }

    const auto& [filename, timestamp] = *parsed;

    // Get cached files
    auto files = client_.cache().get_cached_file_list(chat_id);

    // Find matching file (match by filename and approximate timestamp within same minute)
    for (const auto& file : files) {
        if (file.filename == filename) {
            // Check if timestamp matches within the same minute
            int64_t file_minute = file.timestamp / 60;
            int64_t target_minute = timestamp / 60;
            if (file_minute == target_minute) {
                // Store in a static to return pointer (safe because we're under mutex)
                static tg::FileListItem found_file;
                found_file = file;
                return &found_file;
            }
        }
    }

    return nullptr;
}

void TelegramDataProvider::ensure_files_loaded(int64_t chat_id) {
    // Check if we have cached files
    auto files = client_.cache().get_cached_file_list(chat_id);
    if (!files.empty()) {
        return;  // Already have cached files
    }

    // Fetch from Telegram API
    try {
        spdlog::debug("Fetching files for chat {} from API", chat_id);

        // Fetch both documents and media (cache all, filter at display time)
        auto files_task = client_.list_files(chat_id);
        auto media_task = client_.list_media(chat_id);

        auto file_list = files_task.get_result();
        auto media_list = media_task.get_result();

        // Combine both lists
        file_list.insert(file_list.end(), media_list.begin(), media_list.end());

        // Cache the results
        if (!file_list.empty()) {
            client_.cache().cache_file_list(chat_id, file_list);
            spdlog::info("Cached {} files for chat {}", file_list.size(), chat_id);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to fetch files for chat {}: {}", chat_id, e.what());
    }
}

FileContent TelegramDataProvider::download_and_read_file(const tg::FileListItem& file) {
    FileContent content;
    content.readable = false;

    try {
        spdlog::debug("Downloading {} (id: {})", file.filename, file.file_id);

        auto local_path = client_.download_file(file.file_id).get_result();

        std::ifstream ifs(local_path, std::ios::binary);
        if (ifs) {
            std::ostringstream oss;
            oss << ifs.rdbuf();
            content.data = oss.str();
            content.readable = true;
            spdlog::debug("Read {} bytes from {}", content.data.size(), local_path);
        } else {
            spdlog::error("Failed to open downloaded file: {}", local_path);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to download {}: {}", file.filename, e.what());
    }

    return content;
}

int64_t TelegramDataProvider::get_chat_id_for_files(const PathInfo& info) const {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (info.category) {
        case PathCategory::USER_FILES_DIR:
        case PathCategory::USER_FILE:
        case PathCategory::USER_MEDIA_DIR:
        case PathCategory::USER_MEDIA: {
            auto* user = find_user_by_dir_name(info.entity_name);
            return user ? user->id : 0;
        }
        case PathCategory::GROUP_FILES_DIR:
        case PathCategory::GROUP_FILE:
        case PathCategory::GROUP_MEDIA_DIR:
        case PathCategory::GROUP_MEDIA: {
            auto* group = find_group_by_dir_name(info.entity_name);
            return group ? group->id : 0;
        }
        case PathCategory::CHANNEL_FILES_DIR:
        case PathCategory::CHANNEL_FILE:
        case PathCategory::CHANNEL_MEDIA_DIR:
        case PathCategory::CHANNEL_MEDIA: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            return channel ? channel->id : 0;
        }
        default:
            return 0;
    }
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
    // First check in-memory TLRU cache
    std::size_t cached_size = messages_cache_->get_content_size(chat_id);
    if (cached_size > 0) {
        return cached_size;
    }

    // Check persisted stats in SQLite
    auto stats = client_.cache().get_chat_message_stats(chat_id);
    if (stats && stats->content_size > 0) {
        return stats->content_size;
    }

    // Default for unknown chats - use a reasonable default that allows reading
    return 4096;
}

std::string TelegramDataProvider::fetch_and_format_messages(int64_t chat_id) {
    // Try to get from TLRU cache first (returns nullopt if stale or not cached)
    auto cached = messages_cache_->get(chat_id);
    if (cached) {
        spdlog::debug("fetch_and_format_messages: TLRU hit for chat {}, size {}", chat_id, cached->size());
        return std::string(*cached);
    }

    // TLRU miss - try to get messages from SQLite first
    const auto& config = messages_cache_->get_config();
    auto max_age_secs = static_cast<int64_t>(config.max_history_age.count());
    auto messages = client_.cache().get_messages_for_display(chat_id, max_age_secs);

    // If SQLite has enough messages, format and cache
    if (!messages.empty() && messages.size() >= config.min_messages) {
        spdlog::debug(
            "fetch_and_format_messages: formatting {} messages from SQLite for chat {}", messages.size(), chat_id
        );
        return format_and_cache_messages(chat_id, messages);
    }

    // Not enough in SQLite - fetch from Telegram API
    try {
        spdlog::debug("fetch_and_format_messages: fetching from API for chat {}", chat_id);
        auto task = client_.get_messages_until(chat_id, config.min_messages, config.max_history_age);
        messages = task.get_result();

        // Store raw messages in SQLite
        for (const auto& msg : messages) {
            client_.cache().cache_message(msg);
        }

        // Sort by timestamp for display (oldest first)
        std::sort(messages.begin(), messages.end(), [](const tg::Message& a, const tg::Message& b) {
            return a.timestamp < b.timestamp;
        });

        // Format and cache
        auto content = format_and_cache_messages(chat_id, messages);

        // Evict old messages from SQLite
        auto cutoff = std::chrono::system_clock::now() - config.max_history_age;
        auto cutoff_ts = std::chrono::duration_cast<std::chrono::seconds>(cutoff.time_since_epoch()).count();
        client_.cache().evict_old_messages(chat_id, cutoff_ts);

        return content;
    } catch (const std::exception& e) {
        spdlog::error("Failed to fetch messages for chat {}: {}", chat_id, e.what());
        return "";
    }
}

UserResolver TelegramDataProvider::make_user_resolver() const {
    // Static fallback user for unknown senders
    static tg::User unknown_user;

    return [this](int64_t sender_id) -> const tg::User& {
        // Try to find sender in users cache
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, user] : users_) {
            if (user.id == sender_id) {
                return user;
            }
        }

        // Fallback for unknown senders
        unknown_user.id = sender_id;
        unknown_user.first_name = "User";
        unknown_user.last_name = std::to_string(sender_id);
        return unknown_user;
    };
}

ChatResolver TelegramDataProvider::make_chat_resolver() const {
    // Static fallback chat for unknown chats
    static tg::Chat unknown_chat;

    return [this](int64_t chat_id) -> const tg::Chat& {
        // Try to find chat in users cache (private chats)
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, user] : users_) {
            if (user.id == chat_id) {
                // Create a chat from user data
                static tg::Chat user_chat;
                user_chat.id = user.id;
                user_chat.type = tg::ChatType::PRIVATE;
                user_chat.title = user.display_name();
                user_chat.username = user.username;
                return user_chat;
            }
        }

        // Try groups
        for (const auto& [name, chat] : groups_) {
            if (chat.id == chat_id) {
                return chat;
            }
        }

        // Try channels
        for (const auto& [name, chat] : channels_) {
            if (chat.id == chat_id) {
                return chat;
            }
        }

        // Fallback for unknown chats
        unknown_chat.id = chat_id;
        unknown_chat.title = "Chat " + std::to_string(chat_id);
        return unknown_chat;
    };
}

void TelegramDataProvider::setup_message_callback() {
    client_.set_message_callback([this](const tg::Message& message) {
        // Store message in SQLite
        client_.cache().cache_message(message);

        // Update chat message stats
        auto stats = client_.cache().get_chat_message_stats(message.chat_id);
        tg::ChatMessageStats new_stats;
        new_stats.chat_id = message.chat_id;
        new_stats.message_count = stats ? stats->message_count + 1 : 1;
        new_stats.content_size = stats ? stats->content_size : 0;  // Will be updated on next format
        new_stats.last_message_time = message.timestamp;
        new_stats.oldest_message_time = stats ? stats->oldest_message_time : message.timestamp;
        new_stats.last_fetch_time = stats ? stats->last_fetch_time : 0;
        client_.cache().update_chat_message_stats(new_stats);

        // Invalidate TLRU cache for this chat (forces reformat on next read)
        messages_cache_->invalidate(message.chat_id);
        spdlog::debug("New message {} for chat {}, cache invalidated", message.id, message.chat_id);
    });
}

std::string TelegramDataProvider::format_and_cache_messages(int64_t chat_id, const std::vector<tg::Message>& messages) {
    if (messages.empty()) {
        return "";
    }

    // Format all messages
    std::vector<tg::MessageInfo> infos;
    infos.reserve(messages.size());

    auto user_resolver = make_user_resolver();
    auto chat_resolver = make_chat_resolver();

    for (const auto& msg : messages) {
        infos.push_back({msg, user_resolver(msg.sender_id), chat_resolver(chat_id)});
    }

    // Format messages using bustache template
    std::string content;
    const auto& tmpl = messages_cache_->message_template();
    for (const auto& info : infos) {
        content += bustache::to_string(tmpl(info));
        content += '\n';
    }

    // Store in TLRU cache
    int64_t newest_id = messages.empty() ? 0 : messages.back().id;
    messages_cache_->store(chat_id, content, messages.size(), newest_id);

    // Update stats in SQLite
    tg::ChatMessageStats stats;
    stats.chat_id = chat_id;
    stats.message_count = messages.size();
    stats.content_size = content.size();
    stats.last_message_time = messages.back().timestamp;
    stats.oldest_message_time = messages.front().timestamp;
    stats.last_fetch_time = std::time(nullptr);
    client_.cache().update_chat_message_stats(stats);

    return content;
}

bool TelegramDataProvider::is_writable(std::string_view path) const {
    auto info = parse_path(path);
    return is_messages_path(info.category) || is_upload_path(info.category);
}

bool TelegramDataProvider::is_upload_path(PathCategory category) const {
    return is_files_dir_path(category) || is_file_path(category) || is_media_dir_path(category) ||
           is_media_path(category) || category == PathCategory::USER_UPLOAD || category == PathCategory::GROUP_UPLOAD ||
           category == PathCategory::CHANNEL_UPLOAD;
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

int64_t TelegramDataProvider::get_chat_id_for_upload(const PathInfo& info) const {
    std::lock_guard<std::mutex> lock(mutex_);

    switch (info.category) {
        case PathCategory::USER_UPLOAD:
        case PathCategory::USER_FILES_DIR:
        case PathCategory::USER_FILE:
        case PathCategory::USER_MEDIA_DIR:
        case PathCategory::USER_MEDIA: {
            auto* user = find_user_by_dir_name(info.entity_name);
            return user ? user->id : 0;
        }
        case PathCategory::GROUP_UPLOAD:
        case PathCategory::GROUP_FILES_DIR:
        case PathCategory::GROUP_FILE:
        case PathCategory::GROUP_MEDIA_DIR:
        case PathCategory::GROUP_MEDIA: {
            auto* group = find_group_by_dir_name(info.entity_name);
            return group ? group->id : 0;
        }
        case PathCategory::CHANNEL_UPLOAD:
        case PathCategory::CHANNEL_FILES_DIR:
        case PathCategory::CHANNEL_FILE:
        case PathCategory::CHANNEL_MEDIA_DIR:
        case PathCategory::CHANNEL_MEDIA: {
            auto* channel = find_channel_by_dir_name(info.entity_name);
            return channel ? channel->id : 0;
        }
        default:
            return 0;
    }
}

bool TelegramDataProvider::is_valid_media_extension(const std::string& filename) const {
    namespace fs = std::filesystem;
    auto ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    static const std::set<std::string> media_extensions = {
        ".jpg",
        ".jpeg",
        ".png",
        ".gif",
        ".webp",
        ".bmp",
        ".tiff",
        ".mp4",
        ".mov",
        ".avi",
        ".mkv",
        ".webm",
        ".m4v",
        ".3gp"
    };
    return media_extensions.count(ext) > 0;
}

std::string TelegramDataProvider::extract_original_filename(const std::string& entry_name) const {
    // Strip YYYYMMDD-HHMM- prefix if present
    if (entry_name.size() > 14 && entry_name[8] == '-' && entry_name[13] == '-') {
        return entry_name.substr(14);
    }
    return entry_name;
}

std::filesystem::path TelegramDataProvider::get_upload_temp_dir() const {
    return std::filesystem::temp_directory_path() / "tg-fuse" / "uploads";
}

TelegramDataProvider::UploadAction
TelegramDataProvider::detect_upload_action(const std::string& path, const std::string& filename) const {
    namespace fs = std::filesystem;
    auto ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Only .txt and .md are sent as text messages
    static const std::set<std::string> text_extensions = {".txt", ".md"};
    static const std::set<std::string> media_extensions = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp", ".mp4", ".mov", ".avi", ".mkv", ".webm", ".m4v", ".3gp"
    };

    if (media_extensions.count(ext)) {
        return UploadAction::SEND_AS_MEDIA;
    }

    if (text_extensions.count(ext)) {
        // Verify it's actually valid UTF-8 text
        if (is_valid_text_file(path)) {
            return UploadAction::SEND_AS_TEXT;
        }
    }

    return UploadAction::SEND_AS_DOCUMENT;
}

bool TelegramDataProvider::is_valid_text_file(const std::string& path) const {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return false;
    }

    // Read file content
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // Use existing text validation
    return MessageFormatter::is_valid_text(content.data(), content.size());
}

int TelegramDataProvider::send_file_as_text(int64_t chat_id, const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        spdlog::error("Failed to open file for text send: {}", path);
        return -EIO;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    auto result = send_message(chat_id, content.data(), content.size());

    // Clean up temp file
    std::error_code ec;
    std::filesystem::remove(path, ec);

    return result.success ? 0 : -EIO;
}

std::string TelegramDataProvider::compute_file_hash(const std::string& path) const {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return "";
    }

    // Simple hash using std::hash on file content
    // For production, consider using OpenSSL SHA256
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    std::size_t hash = std::hash<std::string>{}(content);
    return fmt::format("{:016x}", hash);
}

bool TelegramDataProvider::send_file_by_remote_id(
    int64_t chat_id,
    const std::string& remote_file_id,
    const std::string& filename,
    tg::SendMode mode
) {
    try {
        auto task = client_.send_file_by_id(chat_id, remote_file_id, filename, mode);
        task.get_result();
        spdlog::info("Sent cached file {} to chat {}", filename, chat_id);
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to send cached file, will re-upload: {}", e.what());
        return false;
    }
}

int TelegramDataProvider::create_file(std::string_view path, mode_t mode, uint64_t& fh) {
    (void)mode;
    auto info = parse_path(path);

    if (!is_upload_path(info.category)) {
        return -EACCES;
    }

    int64_t chat_id = get_chat_id_for_upload(info);
    if (chat_id == 0) {
        return -ENOENT;
    }

    // Determine upload mode based on target path
    tg::SendMode upload_mode = tg::SendMode::AUTO;
    if (is_files_dir_path(info.category) || is_file_path(info.category)) {
        upload_mode = tg::SendMode::DOCUMENT;
    } else if (is_media_dir_path(info.category) || is_media_path(info.category)) {
        upload_mode = tg::SendMode::MEDIA;
        // Validate media type - reject non-media files in media/ directory
        if (!is_valid_media_extension(info.file_entry_name)) {
            spdlog::warn("Rejected non-media file in media/: {}", info.file_entry_name);
            return -EINVAL;
        }
    }
    // USER_UPLOAD, GROUP_UPLOAD, CHANNEL_UPLOAD remain AUTO

    // Extract filename (strip timestamp prefix if present)
    std::string filename = extract_original_filename(info.file_entry_name);

    // Create temp directory and file
    auto temp_dir = get_upload_temp_dir();
    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);
    if (ec) {
        spdlog::error("Failed to create temp directory: {}", ec.message());
        return -EIO;
    }

    auto temp_path = temp_dir / fmt::format("{}_{}", next_upload_handle_.load(), filename);

    // Track pending upload
    fh = next_upload_handle_++;
    {
        std::lock_guard<std::mutex> lock(uploads_mutex_);
        pending_uploads_[fh] = PendingUpload{
            .temp_path = temp_path.string(),
            .original_filename = filename,
            .virtual_path = std::string(path),
            .chat_id = chat_id,
            .mode = upload_mode,
            .bytes_written = 0
        };
    }

    spdlog::debug("create_file: path={}, fh={}, temp={}", path, fh, temp_path.string());
    return 0;
}

WriteResult
TelegramDataProvider::write_file(std::string_view path, const char* data, std::size_t size, off_t offset, uint64_t fh) {
    // Check if this is a pending upload
    {
        std::lock_guard<std::mutex> lock(uploads_mutex_);
        auto it = pending_uploads_.find(fh);
        if (it != pending_uploads_.end()) {
            auto& upload = it->second;

            // Write to temp file
            std::ofstream ofs(upload.temp_path, std::ios::binary | (offset == 0 ? std::ios::trunc : std::ios::app));
            if (!ofs) {
                spdlog::error("Failed to open temp file: {}", upload.temp_path);
                return WriteResult{false, 0, "Failed to open temp file"};
            }
            ofs.seekp(offset);
            ofs.write(data, static_cast<std::streamsize>(size));
            upload.bytes_written += size;

            spdlog::debug("write_file: fh={}, offset={}, size={}, total={}", fh, offset, size, upload.bytes_written);
            return WriteResult{true, static_cast<int>(size), ""};
        }
    }

    // Fall through to existing message handling
    return write_file(path, data, size, offset);
}

int TelegramDataProvider::release_file(std::string_view path, uint64_t fh) {
    (void)path;

    // Clean up old completed uploads periodically
    cleanup_completed_uploads();

    PendingUpload upload;
    std::string virtual_path;
    {
        std::lock_guard<std::mutex> lock(uploads_mutex_);
        auto it = pending_uploads_.find(fh);
        if (it == pending_uploads_.end()) {
            return 0;  // Not an upload
        }
        virtual_path = it->second.virtual_path;  // Save before move
        upload = std::move(it->second);
        pending_uploads_.erase(it);
    }

    spdlog::debug("release_file: fh={}, file={}, bytes_written={}", fh, upload.original_filename, upload.bytes_written);

    // Check file exists and get size
    std::error_code ec;
    auto file_size = std::filesystem::file_size(upload.temp_path, ec);
    if (ec) {
        spdlog::error("Failed to get file size: {}", ec.message());
        std::filesystem::remove(upload.temp_path, ec);
        return -EIO;
    }

    // Check file size limit
    if (static_cast<int64_t>(file_size) > tg::kMaxFileSizeRegular) {
        std::filesystem::remove(upload.temp_path, ec);
        spdlog::error("File too large: {} bytes (limit: {} bytes)", file_size, tg::kMaxFileSizeRegular);
        return -EFBIG;
    }

    // Handle AUTO mode - detect content type
    if (upload.mode == tg::SendMode::AUTO) {
        auto detected = detect_upload_action(upload.temp_path, upload.original_filename);
        if (detected == UploadAction::SEND_AS_TEXT) {
            // Read file and send as text message(s)
            int result = send_file_as_text(upload.chat_id, upload.temp_path);
            if (result == 0) {
                mark_upload_completed(virtual_path, upload.original_filename, file_size);
            }
            return result;
        }
        // Convert UploadAction to SendMode for file uploads
        upload.mode = (detected == UploadAction::SEND_AS_MEDIA) ? tg::SendMode::MEDIA : tg::SendMode::DOCUMENT;
    }

    // Note: Upload caching is disabled because TDLib's sendMessage returns before
    // the upload completes, and the file_id in the returned message still references
    // the local file. To implement caching properly, we'd need to track uploads and
    // get the final remote file ID from updateMessageSendSucceeded.

    // Upload new file - rename to original filename so TDLib uses correct name
    auto upload_path = std::filesystem::path(upload.temp_path).parent_path() / upload.original_filename;
    spdlog::debug("Renaming {} -> {}", upload.temp_path, upload_path.string());
    std::filesystem::rename(upload.temp_path, upload_path, ec);
    if (ec) {
        spdlog::error("Failed to rename temp file: {}", ec.message());
        std::filesystem::remove(upload.temp_path, ec);
        return -EIO;
    }

    // Verify file exists after rename
    if (!std::filesystem::exists(upload_path)) {
        spdlog::error("File does not exist after rename: {}", upload_path.string());
        return -EIO;
    }

    try {
        std::string path_str = upload_path.string();
        spdlog::info(
            "Uploading {} to chat {} as {} (path={})",
            upload.original_filename,
            upload.chat_id,
            upload.mode == tg::SendMode::MEDIA ? "media" : "document",
            path_str
        );
        auto task = client_.send_file(upload.chat_id, path_str, upload.mode);
        auto msg = task.get_result();

        // Note: Don't cache remote_file_id here - the upload may not be complete yet.
        // TDLib uploads asynchronously; the file_id in the returned message may still
        // reference the local file path. Caching is disabled until we have a proper
        // way to get the final remote file ID after upload completes.
    } catch (const std::exception& e) {
        spdlog::error("Failed to send file: {}", e.what());
        std::filesystem::remove(upload_path, ec);
        return -EIO;
    }

    // Note: Do NOT delete the file here. TDLib uploads asynchronously and still
    // needs access to the file. The TelegramClient will delete it when
    // updateMessageSendSucceeded is received.
    mark_upload_completed(virtual_path, upload.original_filename, file_size);
    return 0;
}

const TelegramDataProvider::PendingUpload* TelegramDataProvider::find_pending_upload_by_path(
    std::string_view path
) const {
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    for (const auto& [fh, upload] : pending_uploads_) {
        if (upload.virtual_path == path) {
            return &upload;
        }
    }
    return nullptr;
}

const TelegramDataProvider::CompletedUpload* TelegramDataProvider::find_completed_upload_by_path(
    std::string_view path
) const {
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    auto it = completed_uploads_.find(std::string(path));
    if (it != completed_uploads_.end()) {
        return &it->second;
    }
    return nullptr;
}

void TelegramDataProvider::mark_upload_completed(
    const std::string& virtual_path,
    const std::string& filename,
    std::size_t size
) {
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    completed_uploads_[virtual_path] = CompletedUpload{
        .filename = filename,
        .size = size,
        .completed_at = std::chrono::steady_clock::now(),
    };
    spdlog::debug("Marked upload completed: {} ({} bytes)", virtual_path, size);
}

void TelegramDataProvider::cleanup_completed_uploads() {
    std::lock_guard<std::mutex> lock(uploads_mutex_);
    auto now = std::chrono::steady_clock::now();
    constexpr auto kMaxAge = std::chrono::seconds(30);  // Keep for 30s for slow post-release ops

    for (auto it = completed_uploads_.begin(); it != completed_uploads_.end();) {
        if (now - it->second.completed_at > kMaxAge) {
            spdlog::debug("Cleaning up completed upload: {}", it->first);
            it = completed_uploads_.erase(it);
        } else {
            ++it;
        }
    }
}

void TelegramDataProvider::add_uploads_to_listing(std::string_view dir_path, std::vector<Entry>& entries) const {
    std::lock_guard<std::mutex> lock(uploads_mutex_);

    // Ensure dir_path ends with / for proper prefix matching
    std::string dir_prefix(dir_path);
    if (!dir_prefix.empty() && dir_prefix.back() != '/') {
        dir_prefix += '/';
    }

    // Add pending uploads in this directory
    for (const auto& [fh, upload] : pending_uploads_) {
        // Check if this upload's virtual_path starts with dir_prefix
        if (upload.virtual_path.size() > dir_prefix.size() &&
            upload.virtual_path.compare(0, dir_prefix.size(), dir_prefix) == 0) {
            // Extract filename (part after dir_prefix, should not contain /)
            auto remaining = upload.virtual_path.substr(dir_prefix.size());
            if (remaining.find('/') == std::string::npos) {
                auto entry = Entry::file(upload.original_filename, upload.bytes_written, 0644);
                entry.mtime = std::time(nullptr);
                entry.atime = entry.mtime;
                entry.ctime = entry.mtime;
                entries.push_back(std::move(entry));
            }
        }
    }

    // Add completed uploads in this directory
    for (const auto& [vpath, completed] : completed_uploads_) {
        // Check if this upload's virtual_path starts with dir_prefix
        if (vpath.size() > dir_prefix.size() && vpath.compare(0, dir_prefix.size(), dir_prefix) == 0) {
            // Extract filename (part after dir_prefix, should not contain /)
            auto remaining = vpath.substr(dir_prefix.size());
            if (remaining.find('/') == std::string::npos) {
                auto entry = Entry::file(completed.filename, completed.size, 0644);
                entry.mtime = std::time(nullptr);
                entry.atime = entry.mtime;
                entry.ctime = entry.mtime;
                entries.push_back(std::move(entry));
            }
        }
    }
}

}  // namespace tgfuse
