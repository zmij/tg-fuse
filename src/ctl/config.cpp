#include "config.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace tgfuse::ctl {

namespace {

std::filesystem::path get_xdg_config_home() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        return xdg;
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".config";
    }
    return ".config";
}

std::filesystem::path get_xdg_data_home() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] != '\0') {
        return xdg;
    }
    const char* home = std::getenv("HOME");
    if (home) {
        return std::filesystem::path(home) / ".local" / "share";
    }
    return ".local/share";
}

}  // namespace

std::filesystem::path get_config_dir() { return get_xdg_config_home() / "tg-fuse"; }

std::filesystem::path get_data_dir() { return get_xdg_data_home() / "tg-fuse"; }

std::filesystem::path get_config_path() { return get_config_dir() / "config.json"; }

std::optional<Config> load_config() {
    auto path = get_config_path();

    if (!std::filesystem::exists(path)) {
        spdlog::debug("Config file not found: {}", path.string());
        return std::nullopt;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            spdlog::warn("Failed to open config file: {}", path.string());
            return std::nullopt;
        }

        nlohmann::json j;
        file >> j;

        Config config;
        config.api_id = j.value("api_id", 0);
        config.api_hash = j.value("api_hash", "");

        if (!config.is_valid()) {
            spdlog::warn("Config file has invalid or missing credentials");
            return std::nullopt;
        }

        spdlog::debug("Loaded config from {}", path.string());
        return config;

    } catch (const nlohmann::json::exception& e) {
        spdlog::warn("Failed to parse config file: {}", e.what());
        return std::nullopt;
    } catch (const std::exception& e) {
        spdlog::warn("Failed to read config file: {}", e.what());
        return std::nullopt;
    }
}

void save_config(const Config& config) {
    auto dir = get_config_dir();
    auto path = get_config_path();

    // Create directory if needed
    std::filesystem::create_directories(dir);

    nlohmann::json j;
    j["api_id"] = config.api_id;
    j["api_hash"] = config.api_hash;

    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create config file: " + path.string());
    }

    file << j.dump(2) << std::endl;
    spdlog::info("Configuration saved to {}", path.string());
}

bool open_browser(const std::string& url) {
#ifdef __APPLE__
    std::string cmd = "open '" + url + "' 2>/dev/null";
#else
    std::string cmd = "xdg-open '" + url + "' 2>/dev/null";
#endif

    int result = std::system(cmd.c_str());
    return result == 0;
}

int exec_config_set(int32_t api_id, const std::string& api_hash) {
    if (api_id <= 0) {
        std::cerr << "Error: API ID must be a positive number.\n";
        return 1;
    }
    if (api_hash.empty()) {
        std::cerr << "Error: API hash cannot be empty.\n";
        return 1;
    }

    Config config;
    config.api_id = api_id;
    config.api_hash = api_hash;

    try {
        save_config(config);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

void setup_file_logging() {
    auto logs_dir = get_data_dir() / "logs";
    std::filesystem::create_directories(logs_dir);

    auto log_path = (logs_dir / "tg-fuse.log").string();

    try {
        auto file_logger = spdlog::basic_logger_mt("file_logger", log_path, true);
        spdlog::set_default_logger(file_logger);
    } catch (const spdlog::spdlog_ex& e) {
        // If we can't set up file logging, just continue with console
        std::cerr << "Warning: Could not set up file logging: " << e.what() << "\n";
    }
}

}  // namespace tgfuse::ctl
