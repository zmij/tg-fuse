#pragma once

#include "tg/types.hpp"

#include <functional>
#include <string>
#include <vector>

namespace tgfuse {

/// Sender information for message formatting
struct SenderInfo {
    std::string display_name;
    std::string username;  // Without @ prefix
    bool is_outgoing{false};
};

/// Message formatter for the messages file
///
/// Keeps formatting logic separate for future enhancements.
/// Format: > **Display Name (@username)** *HH:MM* message text
class MessageFormatter {
public:
    /// Format a single message as markdown blockquote
    /// @param msg The message to format
    /// @param sender Sender information
    /// @return Formatted message string
    static std::string format_message(const tg::Message& msg, const SenderInfo& sender);

    /// Format multiple messages (oldest first for display)
    /// @param messages Messages to format (will be sorted oldest-first)
    /// @param get_sender Callback to get sender info for a sender_id
    /// @return Formatted messages string
    static std::string
    format_messages(const std::vector<tg::Message>& messages, const std::function<SenderInfo(int64_t)>& get_sender);

    /// Estimate size for a given message count (for file size reporting)
    /// @param message_count Number of messages
    /// @return Estimated byte size
    static std::size_t estimate_size(std::size_t message_count);

    /// Check if data appears to be valid text (not binary)
    /// @param data Data buffer
    /// @param size Size of data
    /// @return true if data is valid text
    static bool is_valid_text(const char* data, std::size_t size);

    /// Split large text into chunks suitable for Telegram messages
    /// @param text Text to split
    /// @param max_size Maximum size per chunk (default 4096)
    /// @return Vector of text chunks
    static std::vector<std::string> split_message(const std::string& text, std::size_t max_size = 4096);

    static constexpr std::size_t AVG_MESSAGE_SIZE = 150;  // Conservative estimate per message
    static constexpr std::size_t DEFAULT_FALLBACK_SIZE = 4096;
};

}  // namespace tgfuse
