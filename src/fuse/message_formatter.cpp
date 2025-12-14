#include "fuse/message_formatter.hpp"

namespace tgfuse {

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
