#include "tg/formatters.hpp"

#include <chrono>
#include <string_view>
#include <unordered_map>

#include <fmt/chrono.h>

namespace tg {

namespace {

const std::unordered_map<UserStatus, std::string_view> user_status_to_string_map = {
    {UserStatus::ONLINE, "online"},
    {UserStatus::OFFLINE, "offline"},
    {UserStatus::RECENTLY, "recently"},
    {UserStatus::LAST_WEEK, "last week"},
    {UserStatus::LAST_MONTH, "last month"},
    {UserStatus::UNKNOWN, "unknown"},
};

const std::unordered_map<ChatType, std::string_view> chat_type_to_string_map = {
    {ChatType::PRIVATE, "private"},
    {ChatType::GROUP, "group"},
    {ChatType::SUPERGROUP, "supergroup"},
    {ChatType::CHANNEL, "channel"},
};

const std::unordered_map<MediaType, std::string_view> media_type_to_string_map = {
    {MediaType::PHOTO, "photo"},
    {MediaType::VIDEO, "video"},
    {MediaType::DOCUMENT, "document"},
    {MediaType::AUDIO, "audio"},
    {MediaType::VOICE, "voice"},
    {MediaType::ANIMATION, "animation"},
    {MediaType::STICKER, "sticker"},
};

auto format_display_name(const tg::User& user, fmt::format_context& ctx) -> decltype(ctx.out()) {
    if (!user.first_name.empty() && !user.last_name.empty()) {
        return fmt::format_to(ctx.out(), "{} {}", user.first_name, user.last_name);
    } else if (!user.first_name.empty()) {
        return fmt::format_to(ctx.out(), "{}", user.first_name);
    } else if (!user.last_name.empty()) {
        return fmt::format_to(ctx.out(), "{}", user.last_name);
    }
    return ctx.out();
}

constexpr std::array<std::string_view, 2> munute_forms = {"minute ago", "minutes ago"};
constexpr std::array<std::string_view, 2> hour_forms = {"hour ago", "hours ago"};

auto format_timestamp(int64_t ts_sec, fmt::format_context& ctx) -> decltype(ctx.out()) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::from_time_t(ts_sec);
    auto diff = now - timestamp;
    if (diff < std::chrono::minutes(1)) {
        return fmt::format_to(ctx.out(), "[just_now]");
    } else if (diff < std::chrono::hours(1)) {
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(diff).count();
        if (minutes == 1) {
            return fmt::format_to(ctx.out(), "[{} {}]:", minutes, munute_forms[0]);
        } else {
            return fmt::format_to(ctx.out(), "[{} {}]:", minutes, munute_forms[1]);
        }
    } else if (diff < std::chrono::hours(24)) {
        auto hours = std::chrono::duration_cast<std::chrono::hours>(diff).count();
        if (hours == 1) {
            return fmt::format_to(ctx.out(), "[{} {}]:", hours, hour_forms[0]);
        } else {
            return fmt::format_to(ctx.out(), "[{} {}]:", hours, hour_forms[1]);
        }
    }
    return fmt::format_to(ctx.out(), "[{}]:", timestamp);
}

}  // namespace

}  // namespace tg

auto fmt::formatter<tg::UserStatus>::format(tg::UserStatus status, fmt::format_context& ctx) const
    -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", tg::user_status_to_string_map.at(status));
}

auto fmt::formatter<tg::ChatType>::format(tg::ChatType type, fmt::format_context& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", tg::chat_type_to_string_map.at(type));
}

auto fmt::formatter<tg::MediaType>::format(tg::MediaType type, fmt::format_context& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", tg::media_type_to_string_map.at(type));
}

auto fmt::formatter<tg::User>::format(const tg::User& user, fmt::format_context& ctx) const -> decltype(ctx.out()) {
    switch (format_spec) {
        case Format::DISPLAY_NAME: {
            tg::format_display_name(user, ctx);
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
            return tg::format_display_name(user, ctx);
        case Format::IDENTIFIER:
            return fmt::format_to(ctx.out(), "{}", user.id);
    }
    return ctx.out();
}

auto fmt::formatter<tg::MessageInfo>::format(const tg::MessageInfo& info, fmt::format_context& ctx) const
    -> decltype(ctx.out()) {
    if (info.message.is_outgoing) {
        fmt::format_to(ctx.out(), "> **You** ");
    } else {
        fmt::format_to(ctx.out(), "> **{:d}** ", info.sender);
    }

    tg::format_timestamp(info.message.timestamp, ctx);

    if (info.message.has_media()) {
        fmt::format_to(ctx.out(), " {}", info.message.media.value());
    }

    if (!info.message.text.empty()) {
        fmt::format_to(ctx.out(), " {}", info.message.text);
    }

    return ctx.out();
}
