#include "cache.hpp"
#include "config.hpp"
#include "login.hpp"
#include "users.hpp"

#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// Find the path to tg-fused executable
/// Looks in the same directory as the current executable
std::string find_daemon_path(const char* argv0) {
    std::string path(argv0);

    // Find last directory separator
    auto pos = path.rfind('/');
    if (pos != std::string::npos) {
        // Replace executable name with tg-fused
        return path.substr(0, pos + 1) + "tg-fused";
    }

    // No path, assume tg-fused is in PATH
    return "tg-fused";
}

/// Execute mount command by exec-ing into tg-fused
int exec_mount(
    const char* argv0,
    const std::string& mount_point,
    bool foreground,
    int verbosity,
    bool mock_mode,
    bool allow_other
) {
    std::string daemon_path = find_daemon_path(argv0);

    spdlog::debug("Executing daemon: {}", daemon_path);

    // Build arguments
    std::vector<std::string> args_storage;
    std::vector<char*> argv_new;

    args_storage.push_back(daemon_path);
    args_storage.push_back(mount_point);

    if (foreground) {
        args_storage.push_back("-f");
    }
    for (int i = 0; i < verbosity; ++i) {
        args_storage.push_back("-v");
    }
    if (mock_mode) {
        args_storage.push_back("--mock");
    }
    if (allow_other) {
        args_storage.push_back("--allow-other");
    }

    // Convert to char* array
    for (auto& arg : args_storage) {
        argv_new.push_back(arg.data());
    }
    argv_new.push_back(nullptr);

    // Log the command
    spdlog::info("Executing: {}", [&]() {
        std::string cmd;
        for (const auto& arg : args_storage) {
            if (!cmd.empty()) cmd += " ";
            cmd += arg;
        }
        return cmd;
    }());

    // Exec into daemon (replaces current process)
    execv(daemon_path.c_str(), argv_new.data());

    // If we get here, exec failed
    spdlog::error("Failed to execute {}: {}", daemon_path, strerror(errno));
    return 1;
}

}  // namespace

int main(int argc, char* argv[]) {
    CLI::App app{"tg-fuse - Telegram FUSE filesystem control"};
    app.require_subcommand(1);

    int verbosity = 0;
    app.add_flag("-v,--verbose", verbosity, "Increase verbosity (-v, -vv, -vvv)");

    // Mount subcommand
    auto* mount_cmd = app.add_subcommand("mount", "Mount the Telegram filesystem");

    std::string mount_point;
    bool foreground = false;
    bool mock_mode = false;
    bool allow_other = false;

    mount_cmd->add_option("mount_point", mount_point, "Mount point for the filesystem")->required();
    mount_cmd->add_flag("-f,--foreground", foreground, "Run in foreground (don't daemonise)");
    mount_cmd->add_flag("--mock", mock_mode, "Use mock data (no Telegram connection)");
    mount_cmd->add_flag("--allow-other", allow_other, "Allow other users to access the mount");

    // Login subcommand
    app.add_subcommand("login", "Authenticate with Telegram");

    // Logout subcommand
    app.add_subcommand("logout", "Log out from Telegram");

    // Status subcommand
    app.add_subcommand("status", "Show authentication status");

    // Users subcommand
    auto* users_cmd = app.add_subcommand("users", "Manage and list users");
    bool list_users = false;
    users_cmd->add_flag("--list,-l", list_users, "List all users from private chats");

    // Cache subcommand with nested subcommands
    auto* cache_cmd = app.add_subcommand("cache", "Manage caches");
    cache_cmd->require_subcommand(1);

    // cache clear-files [entity]
    auto* cache_clear_files_cmd = cache_cmd->add_subcommand("clear-files", "Clear file cache for a specific chat");
    std::string cache_entity_name;
    cache_clear_files_cmd->add_option("entity", cache_entity_name, "Username or display name of the chat");

    // cache clear-all-files
    cache_cmd->add_subcommand("clear-all-files", "Clear all file caches");

    // cache clear-all
    cache_cmd->add_subcommand("clear-all", "Clear all caches (messages, files, etc.)");

    // cache stats
    cache_cmd->add_subcommand("stats", "Show cache statistics");

    // Config subcommand with nested subcommands
    auto* config_cmd = app.add_subcommand("config", "Manage configuration");
    config_cmd->require_subcommand(1);

    // config set
    auto* config_set_cmd = config_cmd->add_subcommand("set", "Set API credentials");
    int32_t api_id = 0;
    std::string api_hash;
    config_set_cmd->add_option("--api-id", api_id, "Telegram API ID")->required();
    config_set_cmd->add_option("--api-hash", api_hash, "Telegram API hash")->required();

    CLI11_PARSE(app, argc, argv);

    // Configure logging based on verbosity
    if (verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
    } else if (verbosity == 1) {
        spdlog::set_level(spdlog::level::debug);
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    // Handle subcommands
    if (mount_cmd->parsed()) {
        return exec_mount(argv[0], mount_point, foreground, verbosity, mock_mode, allow_other);
    }

    if (app.got_subcommand("login")) {
        return tgfuse::ctl::exec_login();
    }

    if (app.got_subcommand("logout")) {
        return tgfuse::ctl::exec_logout();
    }

    if (app.got_subcommand("status")) {
        return tgfuse::ctl::exec_status();
    }

    if (config_set_cmd->parsed()) {
        return tgfuse::ctl::exec_config_set(api_id, api_hash);
    }

    if (users_cmd->parsed()) {
        if (list_users) {
            return tgfuse::ctl::exec_users_list();
        }
        // Default to list if no flag specified
        return tgfuse::ctl::exec_users_list();
    }

    if (cache_clear_files_cmd->parsed()) {
        if (cache_entity_name.empty()) {
            return tgfuse::ctl::exec_cache_clear_all_files();
        }
        return tgfuse::ctl::exec_cache_clear_files(cache_entity_name);
    }

    if (cache_cmd->got_subcommand("clear-all-files")) {
        return tgfuse::ctl::exec_cache_clear_all_files();
    }

    if (cache_cmd->got_subcommand("clear-all")) {
        return tgfuse::ctl::exec_cache_clear_all();
    }

    if (cache_cmd->got_subcommand("stats")) {
        return tgfuse::ctl::exec_cache_stats();
    }

    return 0;
}
