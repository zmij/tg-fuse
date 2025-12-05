#pragma once

#include "tg/types.hpp"
#include "types.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <ctime>

namespace tg {

// Helper to format Unix timestamp as HH:MM
inline std::string format_time(int64_t timestamp) {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::localtime(&time);
    return fmt::format("{:02d}:{:02d}", tm->tm_hour, tm->tm_min);
}

// Helper to format Unix timestamp as YYYY-MM-DD HH:MM
inline std::string format_datetime(int64_t timestamp) {
    std::time_t time = static_cast<std::time_t>(timestamp);
    std::tm* tm = std::localtime(&time);
    return fmt::format(
        "{:04d}-{:02d}-{:02d} {:02d}:{:02d}", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min
    );
}

}  // namespace tg

// fmt formatters for tg types

template <>
struct fmt::formatter<tg::UserStatus> : fmt::formatter<std::string_view> {
    auto format(tg::UserStatus status, fmt::format_context& ctx) const -> decltype(ctx.out());
};

template <>
struct fmt::formatter<tg::MediaType> : fmt::formatter<std::string_view> {
    auto format(tg::MediaType type, fmt::format_context& ctx) const -> decltype(ctx.out());
};

template <>
struct fmt::formatter<tg::ChatType> : fmt::formatter<std::string_view> {
    auto format(tg::ChatType type, fmt::format_context& ctx) const -> decltype(ctx.out());
};

template <>
struct fmt::formatter<tg::MediaInfo> : fmt::formatter<std::string_view> {
    auto format(const tg::MediaInfo& media, fmt::format_context& ctx) const {
        if (media.filename.empty()) {
            return fmt::format_to(ctx.out(), "[{}]", media.type);
        }
        return fmt::format_to(ctx.out(), "[{}: {}]", media.type, media.filename);
    }
};

template <>
struct fmt::formatter<tg::User> : fmt::formatter<std::string_view> {
    enum class Format : char {
        DISPLAY_NAME = 'd',
        USERNAME = 'u',
        FULL_NAME = 'f',
        IDENTIFIER = 'i',
    };

    Format format_spec{Format::DISPLAY_NAME};

    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin();
        if (it != ctx.end() && (*it == 'd' || *it == 'u' || *it == 'f' || *it == 'i')) {
            format_spec = static_cast<Format>(*it);
            ++it;
        }
        return it;
    }

    auto format(const tg::User& user, fmt::format_context& ctx) const -> decltype(ctx.out());
};

template <>
struct fmt::formatter<tg::Chat> : fmt::formatter<std::string_view> {
    auto format(const tg::Chat& chat, fmt::format_context& ctx) const {
        if (!chat.username.empty()) {
            return fmt::format_to(ctx.out(), "{} (@{})", chat.title, chat.username);
        }
        return fmt::format_to(ctx.out(), "{}", chat.title);
    }
};

template <>
struct fmt::formatter<tg::MessageInfo> : fmt::formatter<std::string_view> {
    auto format(const tg::MessageInfo& info, fmt::format_context& ctx) const -> decltype(ctx.out());
};
