#include "tg/formatters.hpp"

#include <chrono>
#include <string_view>
#include <unordered_map>

#include <fmt/chrono.h>

namespace tg {

namespace detail {

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

}  // namespace detail

}  // namespace tg
