#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tg {

// Telegram file size limits
inline constexpr int64_t kMaxFileSizeRegular = 2LL * 1024 * 1024 * 1024;  // 2 GB
inline constexpr int64_t kMaxFileSizePremium = 4LL * 1024 * 1024 * 1024;  // 4 GB

// Forward declarations
struct User;
struct Chat;
struct Message;
struct MediaInfo;
struct FileListItem;

// Enumerations
enum class ChatType {
    PRIVATE,     // Direct message with a user
    GROUP,       // Basic group
    SUPERGROUP,  // Supergroup
    CHANNEL      // Channel (broadcast)
};

enum class MediaType { PHOTO, VIDEO, DOCUMENT, AUDIO, VOICE, ANIMATION, STICKER, VIDEO_NOTE };

enum class SendMode {
    AUTO,     // Auto-detect based on file type
    MEDIA,    // Send as media (compressed photos/videos)
    DOCUMENT  // Send as document (original file)
};

enum class AuthState {
    WAIT_PHONE,     // Waiting for phone number
    WAIT_CODE,      // Waiting for authentication code
    WAIT_PASSWORD,  // Waiting for 2FA password
    READY           // Authenticated and ready
};

enum class UserStatus {
    UNKNOWN,    // Default - never seen or hidden
    ONLINE,     // Currently online
    OFFLINE,    // Offline with known last seen timestamp
    RECENTLY,   // Within last 2-3 days
    LAST_WEEK,  // Within last week
    LAST_MONTH  // Within last month
};

// Data structures
struct User {
    int64_t id;
    std::string username;  // Without @ prefix
    std::string first_name;
    std::string last_name;
    std::string phone_number;
    std::string bio;  // User bio/description
    bool is_contact{false};
    UserStatus status{UserStatus::UNKNOWN};
    int64_t last_seen{0};  // Unix timestamp if status is OFFLINE
    int64_t last_message_id{0};
    int64_t last_message_timestamp{0};

    // Helper to check if user has a display name
    bool has_name() const { return !first_name.empty() || !last_name.empty(); }

    // Helper to get display name
    std::string display_name() const;

    // Helper to get @username or fallback to name
    std::string get_identifier() const;

    // Helper to get human-readable last seen string
    std::string get_last_seen_string() const;
};

struct Chat {
    int64_t id;
    ChatType type;
    std::string title;
    std::string username;  // For public groups/channels (without @ or #)
    int64_t last_message_id;
    int64_t last_message_timestamp;
    bool can_send_messages{true};  // Whether current user can send messages to this chat

    // Helper to get directory name for VFS
    std::string get_directory_name() const;

    // Helper to check if this is a private chat
    bool is_private() const { return type == ChatType::PRIVATE; }

    // Helper to check if this is a group or supergroup
    bool is_group() const { return type == ChatType::GROUP || type == ChatType::SUPERGROUP; }

    // Helper to check if this is a channel
    bool is_channel() const { return type == ChatType::CHANNEL; }
};

struct MediaInfo {
    MediaType type;
    std::string file_id;
    std::string filename;
    std::string mime_type;
    int64_t file_size;
    std::optional<std::string> local_path;  // Path if file is downloaded
    std::optional<int32_t> width;           // For photos/videos
    std::optional<int32_t> height;          // For photos/videos
    std::optional<int32_t> duration;        // For videos/audio

    // Helper to get file extension
    std::string get_extension() const;
};

struct Message {
    int64_t id;
    int64_t chat_id;
    int64_t sender_id;
    int64_t timestamp;  // Unix timestamp
    std::string text;
    std::optional<MediaInfo> media;
    bool is_outgoing;

    // Helper to check if message has media
    bool has_media() const { return media.has_value(); }

    // Helper to format message for display
    std::string format_for_display() const;
};

struct MessageInfo {
    const Message& message;
    const User& sender;
    const Chat& chat;
};

struct FileListItem {
    int64_t message_id;
    std::string filename;
    int64_t file_size;
    int64_t timestamp;
    MediaType type;
    std::string file_id;

    // Helper to get human-readable size
    std::string get_size_string() const;
};

struct ChatStatus {
    int64_t last_message_id;
    int64_t last_message_timestamp;
};

// Utility functions
std::string chat_type_to_string(ChatType type);
std::string media_type_to_string(MediaType type);
MediaType detect_media_type(const std::string& filename, const std::string& mime_type);
bool is_media_type(MediaType type);     // true for PHOTO, VIDEO, ANIMATION
bool is_document_type(MediaType type);  // true for DOCUMENT, AUDIO, etc.

}  // namespace tg
