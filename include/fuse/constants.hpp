#pragma once

#include <string_view>

namespace tgfuse {

// Directory names for the virtual filesystem
inline constexpr std::string_view kUsersDir = "users";
inline constexpr std::string_view kContactsDir = "contacts";
inline constexpr std::string_view kInfoFile = ".info";

}  // namespace tgfuse
