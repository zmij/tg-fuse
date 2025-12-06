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
inline constexpr std::string_view kFilesDir = "files";
inline constexpr std::string_view kMediaDir = "media";
inline constexpr std::string_view kSelfSymlink = "self";
inline constexpr std::string_view kUploadsDir = ".uploads";
inline constexpr std::string_view kTxtFile = "txt";
inline constexpr std::string_view kTextDir = "text";

// txt file buffer limits (for rate limit protection)
// Telegram message limit is 4096 bytes, so ~10 messages worth = 40KB
inline constexpr std::size_t kTxtMaxBufferSize = 40 * 1024;  // 40KB max buffer

}  // namespace tgfuse
