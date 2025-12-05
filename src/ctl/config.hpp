#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace tgfuse::ctl {

/// Application configuration (API credentials)
struct Config {
    int32_t api_id = 0;
    std::string api_hash;

    bool is_valid() const { return api_id != 0 && !api_hash.empty(); }
};

/// Get XDG config directory (~/.config/tg-fuse)
std::filesystem::path get_config_dir();

/// Get XDG data directory (~/.local/share/tg-fuse)
std::filesystem::path get_data_dir();

/// Get config file path (~/.config/tg-fuse/config.json)
std::filesystem::path get_config_path();

/// Load configuration from disk
/// Returns std::nullopt if config file doesn't exist or is invalid
std::optional<Config> load_config();

/// Save configuration to disk
/// Creates directories if needed
void save_config(const Config& config);

/// Open URL in system default browser
/// Returns true if browser was launched successfully
bool open_browser(const std::string& url);

/// Execute config set command
int exec_config_set(int32_t api_id, const std::string& api_hash);

/// Configure spdlog to write to file instead of stderr
/// Call this early in CLI commands that use TelegramClient
void setup_file_logging();

}  // namespace tgfuse::ctl
