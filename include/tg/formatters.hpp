#pragma once

#include "tg/types.hpp"

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
    auto format(tg::UserStatus status, fmt::format_context& ctx) const {
        std::string_view name;
        switch (status) {
            case tg::UserStatus::ONLINE:
                name = "online";
                break;
            case tg::UserStatus::OFFLINE:
                name = "offline";
                break;
            case tg::UserStatus::RECENTLY:
                name = "recently";
                break;
            case tg::UserStatus::LAST_WEEK:
                name = "last week";
                break;
            case tg::UserStatus::LAST_MONTH:
                name = "last month";
                break;
            case tg::UserStatus::UNKNOWN:
            default:
                name = "unknown";
                break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<tg::ChatType> : fmt::formatter<std::string_view> {
    auto format(tg::ChatType type, fmt::format_context& ctx) const {
        std::string_view name;
        switch (type) {
            case tg::ChatType::PRIVATE:
                name = "private";
                break;
            case tg::ChatType::GROUP:
                name = "group";
                break;
            case tg::ChatType::SUPERGROUP:
                name = "supergroup";
                break;
            case tg::ChatType::CHANNEL:
                name = "channel";
                break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<tg::MediaType> : fmt::formatter<std::string_view> {
    auto format(tg::MediaType type, fmt::format_context& ctx) const {
        std::string_view name;
        switch (type) {
            case tg::MediaType::PHOTO:
                name = "photo";
                break;
            case tg::MediaType::VIDEO:
                name = "video";
                break;
            case tg::MediaType::DOCUMENT:
                name = "document";
                break;
            case tg::MediaType::AUDIO:
                name = "audio";
                break;
            case tg::MediaType::VOICE:
                name = "voice message";
                break;
            case tg::MediaType::ANIMATION:
                name = "animation";
                break;
            case tg::MediaType::STICKER:
                name = "sticker";
                break;
            case tg::MediaType::VIDEO_NOTE:
                name = "video note";
                break;
        }
        return fmt::formatter<std::string_view>::format(name, ctx);
    }
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
    auto format(const tg::User& user, fmt::format_context& ctx) const {
        if (!user.username.empty()) {
            return fmt::format_to(ctx.out(), "{} (@{})", user.display_name(), user.username);
        }
        return fmt::format_to(ctx.out(), "{}", user.display_name());
    }
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
