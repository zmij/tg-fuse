#pragma once

#include "tg/types.hpp"
#include "types.hpp"

#include <array>
#include <chrono>
#include <string_view>
#include <unordered_map>

#include <ctime>

#include <fmt/chrono.h>
#include <fmt/format.h>

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

namespace detail {

extern const std::unordered_map<UserStatus, std::string_view> user_status_to_string_map;
extern const std::unordered_map<ChatType, std::string_view> chat_type_to_string_map;
extern const std::unordered_map<MediaType, std::string_view> media_type_to_string_map;

template <typename FormatContext>
auto format_display_name(const tg::User& user, FormatContext& ctx) -> decltype(ctx.out());
template <typename FormatContext>
auto format_timestamp(int64_t ts_sec, FormatContext& ctx) -> decltype(ctx.out());

}  // namespace detail

}  // namespace tg

// fmt formatters for tg types

template <>
struct fmt::formatter<tg::UserStatus> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(tg::UserStatus status, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", tg::detail::user_status_to_string_map.at(status));
    }
};

template <>
struct fmt::formatter<tg::MediaType> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(tg::MediaType type, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", tg::detail::media_type_to_string_map.at(type));
    }
};

template <>
struct fmt::formatter<tg::ChatType> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(tg::ChatType type, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "{}", tg::detail::chat_type_to_string_map.at(type));
    }
};

template <>
struct fmt::formatter<tg::MediaInfo> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const tg::MediaInfo& media, FormatContext& ctx) const -> decltype(ctx.out()) {
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

    template <typename FormatContext>
    auto format(const tg::User& user, FormatContext& ctx) const -> decltype(ctx.out()) {
        switch (format_spec) {
            case Format::DISPLAY_NAME: {
                tg::detail::format_display_name(user, ctx);
                if (!user.username.empty()) {
                    if (user.has_name()) {
                        return fmt::format_to(ctx.out(), " (@{})", user.username);
                    } else {
                        return fmt::format_to(ctx.out(), "@{}", user.username);
                    }
                }
                // Fallback to user ID
                return fmt::format_to(ctx.out(), "User {}", user.id);
            }
            case Format::USERNAME:
                if (!user.username.empty()) {
                    return fmt::format_to(ctx.out(), "@{}", user.username);
                }
                // Fallback to user ID
                return fmt::format_to(ctx.out(), "User {}", user.id);
            case Format::FULL_NAME:
                return tg::detail::format_display_name(user, ctx);
            case Format::IDENTIFIER:
                return fmt::format_to(ctx.out(), "{}", user.id);
        }
        return ctx.out();
    }
};

template <>
struct fmt::formatter<tg::Chat> : fmt::formatter<std::string_view> {
    template <typename FormatContext>
    auto format(const tg::Chat& chat, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (!chat.username.empty()) {
            return fmt::format_to(ctx.out(), "{} (@{})", chat.title, chat.username);
        }
        return fmt::format_to(ctx.out(), "{}", chat.title);
    }
};

template <>
struct fmt::formatter<tg::MessageInfo> : fmt::formatter<std::string_view> {
    enum class Format : char {
        FULL = 'f',
        SENDER = 's',
        TIMESTAMP = 't',
        MESSAGE = 'm',
    };

    Format format_spec{Format::FULL};

    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        auto it = ctx.begin();
        if (it != ctx.end() && (*it == 'f' || *it == 's' || *it == 't' || *it == 'm')) {
            format_spec = static_cast<Format>(*it);
            ++it;
        }
        return it;
    }

    template <typename FormatContext>
    auto format(const tg::MessageInfo& info, FormatContext& ctx) const -> decltype(ctx.out()) {
        if (format_spec == Format::FULL || format_spec == Format::SENDER) {
            if (format_spec == Format::FULL) {
                fmt::format_to(ctx.out(), "> **");
            }
            if (info.message.is_outgoing) {
                fmt::format_to(ctx.out(), "You");
            } else {
                fmt::format_to(ctx.out(), "{:d}", info.sender);
            }
            if (format_spec == Format::FULL) {
                fmt::format_to(ctx.out(), "** ");
            }
        }

        if (format_spec == Format::FULL || format_spec == Format::TIMESTAMP) {
            if (format_spec == Format::FULL) {
                fmt::format_to(ctx.out(), "[");
            }
            tg::detail::format_timestamp(info.message.timestamp, ctx);
            if (format_spec == Format::FULL) {
                fmt::format_to(ctx.out(), "]");
            }
        }

        if (format_spec == Format::FULL || format_spec == Format::MESSAGE) {
            if (format_spec == Format::FULL) {
                fmt::format_to(ctx.out(), ": ");
            }

            if (info.message.has_media()) {
                if (info.message.text.empty()) {
                    fmt::format_to(ctx.out(), "{}", info.message.media.value());
                } else {
                    fmt::format_to(ctx.out(), "{} ", info.message.media.value());
                }
            }

            if (!info.message.text.empty()) {
                fmt::format_to(ctx.out(), "{}", info.message.text);
            }
        }

        return ctx.out();
    }
};

namespace tg::detail {

constexpr std::array<std::string_view, 2> munute_forms = {"minute ago", "minutes ago"};
constexpr std::array<std::string_view, 2> hour_forms = {"hour ago", "hours ago"};

template <typename FormatContext>
auto format_display_name(const tg::User& user, FormatContext& ctx) -> decltype(ctx.out()) {
    if (!user.first_name.empty() && !user.last_name.empty()) {
        return fmt::format_to(ctx.out(), "{} {}", user.first_name, user.last_name);
    } else if (!user.first_name.empty()) {
        return fmt::format_to(ctx.out(), "{}", user.first_name);
    } else if (!user.last_name.empty()) {
        return fmt::format_to(ctx.out(), "{}", user.last_name);
    }
    return ctx.out();
}

template <typename FormatContext>
auto format_timestamp(int64_t ts_sec, FormatContext& ctx) -> decltype(ctx.out()) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::from_time_t(ts_sec);
    auto diff = now - timestamp;
    if (diff < std::chrono::minutes(1)) {
        return fmt::format_to(ctx.out(), "just_now");
    } else if (diff < std::chrono::hours(1)) {
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(diff).count();
        if (minutes == 1) {
            return fmt::format_to(ctx.out(), "{} {}", minutes, munute_forms[0]);
        } else {
            return fmt::format_to(ctx.out(), "{} {}", minutes, munute_forms[1]);
        }
    } else if (diff < std::chrono::hours(24)) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(diff).count();
        if (hours == 1) {
            return fmt::format_to(ctx.out(), "{} {}", hours, hour_forms[0]);
        } else {
            return fmt::format_to(ctx.out(), "{} {}", hours, hour_forms[1]);
        }
    }
    return fmt::format_to(ctx.out(), "{:%Y-%m-%d %H:%M}", timestamp);
}

}  // namespace tg::detail