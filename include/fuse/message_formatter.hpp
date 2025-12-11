#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace tgfuse {

/// Message formatting utilities
///
/// Text validation and splitting for Telegram message sending.
/// Message formatting is done via fmt::format with tg::MessageInfo.
class MessageFormatter {
public:
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
