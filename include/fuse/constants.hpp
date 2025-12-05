#pragma once

#include <string_view>

namespace tgfuse {

// Directory names for the virtual filesystem
inline constexpr std::string_view kUsersDir = "users";
inline constexpr std::string_view kContactsDir = "contacts";
inline constexpr std::string_view kGroupsDir = "groups";
inline constexpr std::string_view kChannelsDir = "channels";
inline constexpr std::string_view kInfoFile = ".info";
inline constexpr std::string_view kMessagesFile = "messages";
inline constexpr std::string_view kSelfSymlink = "self";

}  // namespace tgfuse
