#include "tg/types.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace tg {

// User methods
std::string User::display_name() const {
    if (!first_name.empty() && !last_name.empty()) {
        return first_name + " " + last_name;
    } else if (!first_name.empty()) {
        return first_name;
    } else if (!last_name.empty()) {
        return last_name;
    } else if (!username.empty()) {
        return "@" + username;
    } else {
        return "User " + std::to_string(id);
    }
}

std::string User::get_identifier() const {
    if (!username.empty()) {
        return "@" + username;
    }
    return display_name();
}

std::string User::get_last_seen_string() const {
    switch (status) {
        case UserStatus::ONLINE:
            return "online";
        case UserStatus::OFFLINE: {
            if (last_seen == 0) {
                return "a long time ago";
            }
            std::time_t time = static_cast<std::time_t>(last_seen);
            std::tm* tm = std::localtime(&time);
            std::ostringstream oss;
            oss << std::put_time(tm, "%Y-%m-%d %H:%M");
            return oss.str();
        }
        case UserStatus::RECENTLY:
            return "recently";
        case UserStatus::LAST_WEEK:
            return "within a week";
        case UserStatus::LAST_MONTH:
            return "within a month";
        case UserStatus::UNKNOWN:
        default:
            return "a long time ago";
    }
}

// Chat methods
std::string Chat::get_directory_name() const {
    if (type == ChatType::PRIVATE && !username.empty()) {
        return "@" + username;
    } else if ((type == ChatType::GROUP || type == ChatType::SUPERGROUP) && !username.empty()) {
        return "#" + username;
    } else if (type == ChatType::CHANNEL) {
        if (!username.empty()) {
            return "#" + username;
        }
        // Use numeric ID for private channels/groups
        return std::to_string(id);
    } else {
        // Fallback to numeric ID
        return std::to_string(id);
    }
}

// MediaInfo methods
std::string MediaInfo::get_extension() const {
    // Try to extract from filename first
    if (!filename.empty()) {
        auto pos = filename.find_last_of('.');
        if (pos != std::string::npos && pos < filename.length() - 1) {
            return filename.substr(pos);
        }
    }

    // Fallback to type-based extension
    switch (type) {
        case MediaType::PHOTO:
            return ".jpg";
        case MediaType::VIDEO:
            return ".mp4";
        case MediaType::AUDIO:
            return ".mp3";
        case MediaType::VOICE:
            return ".ogg";
        case MediaType::ANIMATION:
            return ".gif";
        case MediaType::STICKER:
            return ".webp";
        case MediaType::VIDEO_NOTE:
            return ".mp4";
        case MediaType::DOCUMENT:
        default:
            return "";
    }
}

// Message methods
std::string Message::format_for_display() const {
    std::ostringstream oss;

    // Format: [timestamp] Sender: text
    // For now, simplified format
    oss << "[" << timestamp << "] ";

    if (!text.empty()) {
        oss << text;
    }

    if (has_media()) {
        if (!text.empty()) {
            oss << " ";
        }
        oss << "[" << media_type_to_string(media->type) << ": " << media->filename << "]";
    }

    return oss.str();
}

// FileListItem methods
std::string FileListItem::get_size_string() const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(file_size);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream oss;
    oss.precision(2);
    oss << std::fixed << size << " " << units[unit_index];
    return oss.str();
}

// Utility functions
std::string chat_type_to_string(ChatType type) {
    switch (type) {
        case ChatType::PRIVATE:
            return "private";
        case ChatType::GROUP:
            return "group";
        case ChatType::SUPERGROUP:
            return "supergroup";
        case ChatType::CHANNEL:
            return "channel";
        default:
            return "unknown";
    }
}

std::string media_type_to_string(MediaType type) {
    switch (type) {
        case MediaType::PHOTO:
            return "photo";
        case MediaType::VIDEO:
            return "video";
        case MediaType::DOCUMENT:
            return "document";
        case MediaType::AUDIO:
            return "audio";
        case MediaType::VOICE:
            return "voice";
        case MediaType::ANIMATION:
            return "animation";
        case MediaType::STICKER:
            return "sticker";
        case MediaType::VIDEO_NOTE:
            return "video_note";
        default:
            return "unknown";
    }
}

MediaType detect_media_type(const std::string& filename, const std::string& mime_type) {
    // Convert to lowercase for comparison
    std::string lower_mime = mime_type;
    std::string lower_filename = filename;

    std::transform(lower_mime.begin(), lower_mime.end(), lower_mime.begin(), ::tolower);
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

    // Check MIME type first
    if (lower_mime.find("image") != std::string::npos) {
        if (lower_mime.find("gif") != std::string::npos) {
            return MediaType::ANIMATION;
        }
        return MediaType::PHOTO;
    } else if (lower_mime.find("video") != std::string::npos) {
        return MediaType::VIDEO;
    } else if (lower_mime.find("audio") != std::string::npos) {
        return MediaType::AUDIO;
    }

    // Check file extension as fallback
    auto ext_pos = lower_filename.find_last_of('.');
    if (ext_pos != std::string::npos) {
        std::string ext = lower_filename.substr(ext_pos);

        // Image extensions
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".webp") {
            return MediaType::PHOTO;
        } else if (ext == ".gif") {
            return MediaType::ANIMATION;
        }
        // Video extensions
        else if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || ext == ".mkv" || ext == ".webm") {
            return MediaType::VIDEO;
        }
        // Audio extensions
        else if (ext == ".mp3" || ext == ".ogg" || ext == ".wav" || ext == ".m4a" || ext == ".flac") {
            return MediaType::AUDIO;
        }
    }

    // Default to document
    return MediaType::DOCUMENT;
}

bool is_media_type(MediaType type) {
    return type == MediaType::PHOTO || type == MediaType::VIDEO || type == MediaType::ANIMATION;
}

bool is_document_type(MediaType type) {
    return type == MediaType::DOCUMENT || type == MediaType::AUDIO || type == MediaType::VOICE ||
           type == MediaType::STICKER || type == MediaType::VIDEO_NOTE;
}

}  // namespace tg
