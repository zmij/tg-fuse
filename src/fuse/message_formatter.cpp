#include "fuse/message_formatter.hpp"

#include <fmt/format.h>
#include <algorithm>
#include <ctime>

namespace tgfuse {

namespace {

/// Format media type as a display string
std::string_view format_media_tag(const tg::MediaInfo& media) {
    switch (media.type) {
        case tg::MediaType::PHOTO:
            return "[photo]";
        case tg::MediaType::VIDEO:
            return "[video]";
        case tg::MediaType::VOICE:
            return "[voice message]";
        case tg::MediaType::ANIMATION:
            return "[animation]";
        case tg::MediaType::STICKER:
            return "[sticker]";
        case tg::MediaType::VIDEO_NOTE:
            return "[video note]";
        default:
            return "";  // DOCUMENT and AUDIO need filename, handled separately
    }
}

/// Format media with filename for document/audio types
std::string format_media_with_filename(const tg::MediaInfo& media) {
    switch (media.type) {
        case tg::MediaType::DOCUMENT:
            return fmt::format("[document: {}]", media.filename);
        case tg::MediaType::AUDIO:
            return fmt::format("[audio: {}]", media.filename);
        default:
            return std::string(format_media_tag(media));
    }
}

/// Replace newlines with blockquote continuations
std::string escape_newlines_for_blockquote(std::string_view text) {
    std::string result;
    result.reserve(text.size() + text.size() / 20);  // Estimate extra space for "> "

    for (char c : text) {
        if (c == '\n') {
            result += "\n> ";
        } else {
            result += c;
        }
    }
    return result;
}

}  // namespace

std::string MessageFormatter::format_message(const tg::Message& msg, const SenderInfo& sender) {
    // Format timestamp as HH:MM
    std::time_t time = static_cast<std::time_t>(msg.timestamp);
    std::tm* tm = std::localtime(&time);

    // Build sender string
    std::string sender_str;
    if (sender.is_outgoing) {
        sender_str = "You";
    } else if (!sender.username.empty()) {
        sender_str = fmt::format("{} (@{})", sender.display_name, sender.username);
    } else {
        sender_str = sender.display_name;
    }

    // Build content string
    std::string content;
    if (msg.has_media() && msg.text.empty()) {
        // Media-only message
        content = format_media_with_filename(msg.media.value());
    } else {
        // Text message (possibly with media)
        content = escape_newlines_for_blockquote(msg.text);

        if (msg.has_media()) {
            content += ' ';
            content += format_media_with_filename(msg.media.value());
        }
    }

    return fmt::format("> **{}** *{:02d}:{:02d}* {}\n\n", sender_str, tm->tm_hour, tm->tm_min, content);
}

std::string MessageFormatter::format_messages(
    const std::vector<tg::Message>& messages,
    const std::function<SenderInfo(int64_t)>& get_sender
) {
    if (messages.empty()) {
        return "";
    }

    // Sort messages by timestamp (oldest first for display)
    std::vector<tg::Message> sorted = messages;
    std::sort(sorted.begin(), sorted.end(), [](const tg::Message& a, const tg::Message& b) {
        return a.timestamp < b.timestamp;
    });

    // Remove duplicates by message ID (keep first occurrence after sort)
    auto last = std::unique(sorted.begin(), sorted.end(), [](const tg::Message& a, const tg::Message& b) {
        return a.id == b.id;
    });
    sorted.erase(last, sorted.end());

    // Pre-calculate total size estimate for reservation
    std::string result;
    result.reserve(sorted.size() * AVG_MESSAGE_SIZE);

    for (const auto& msg : sorted) {
        SenderInfo sender = get_sender(msg.sender_id);
        sender.is_outgoing = msg.is_outgoing;
        result += format_message(msg, sender);
    }

    return result;
}

std::size_t MessageFormatter::estimate_size(std::size_t message_count) {
    if (message_count == 0) {
        return DEFAULT_FALLBACK_SIZE;
    }
    return message_count * AVG_MESSAGE_SIZE;
}

bool MessageFormatter::is_valid_text(const char* data, std::size_t size) {
    if (size == 0) {
        return true;
    }

    std::size_t non_printable = 0;
    for (std::size_t i = 0; i < size; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);

        // Null byte = definitely binary
        if (c == 0) {
            return false;
        }

        // Control characters (except tab, newline, carriage return)
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            ++non_printable;
        }
    }

    // Reject if more than 5% non-printable
    // For small strings (< 20 bytes), allow up to 1 non-printable
    std::size_t threshold = size < 20 ? 1 : size / 20;
    return non_printable <= threshold;
}

std::vector<std::string> MessageFormatter::split_message(const std::string& text, std::size_t max_size) {
    std::vector<std::string> chunks;

    if (text.empty()) {
        return chunks;
    }

    if (text.size() <= max_size) {
        chunks.push_back(text);
        return chunks;
    }

    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t remaining = text.size() - pos;

        if (remaining <= max_size) {
            // Last chunk
            chunks.push_back(text.substr(pos));
            break;
        }

        // Find last whitespace before max_size
        std::size_t chunk_end = pos + max_size;
        std::size_t split_pos = chunk_end;

        // Search backwards for whitespace
        for (std::size_t i = chunk_end; i > pos; --i) {
            if (text[i] == ' ' || text[i] == '\n' || text[i] == '\t') {
                split_pos = i;
                break;
            }
        }

        // If no whitespace found, force split at max_size
        if (split_pos == chunk_end) {
            split_pos = chunk_end;
        }

        chunks.push_back(text.substr(pos, split_pos - pos));
        pos = split_pos;

        // Skip the whitespace character
        if (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\t')) {
            ++pos;
        }
    }

    return chunks;
}

}  // namespace tgfuse
