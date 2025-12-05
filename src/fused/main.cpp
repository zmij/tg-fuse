#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>

#include "fuse/mock_provider.hpp"
#include "fuse/vfs.hpp"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

namespace {

namespace fs = std::filesystem;

/// Get the data directory for tg-fuse (~/.local/share/tg-fuse)
fs::path get_data_directory() {
    // Check XDG_DATA_HOME first
    if (const char* xdg_data = std::getenv("XDG_DATA_HOME"); xdg_data && *xdg_data) {
        return fs::path(xdg_data) / "tg-fuse";
    }

    // Fall back to ~/.local/share/tg-fuse
    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".local" / "share" / "tg-fuse";
    }

    // Last resort: use passwd entry
    if (struct passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) {
        return fs::path(pw->pw_dir) / ".local" / "share" / "tg-fuse";
    }

    // Fallback to /tmp
    return fs::path("/tmp") / "tg-fuse";
}

/// Convert verbosity level to spdlog level
/// 0 = info, 1 = debug, 2+ = trace
spdlog::level::level_enum verbosity_to_level(int verbosity) {
    if (verbosity >= 2) return spdlog::level::trace;
    if (verbosity == 1) return spdlog::level::debug;
    return spdlog::level::info;
}

/// Set up logging with file and optional stderr output
/// @param foreground Whether running in foreground (logs to stderr)
/// @param verbosity Verbosity level (0=info, 1=debug, 2+=trace)
/// @return true on success
bool setup_logging(bool foreground, int verbosity) {
    try {
        fs::path data_dir = get_data_directory();
        fs::path log_dir = data_dir / "logs";

        // Create log directory if it doesn't exist
        std::error_code ec;
        fs::create_directories(log_dir, ec);
        if (ec) {
            std::cerr << "Failed to create log directory: " << log_dir << " - " << ec.message() << "\n";
            return false;
        }

        fs::path log_file = log_dir / "tg-fused.log";
        auto log_level = verbosity_to_level(verbosity);

        // Create sinks
        std::vector<spdlog::sink_ptr> sinks;

        if (foreground) {
            // Foreground mode: log to stderr only
            auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            stderr_sink->set_level(spdlog::level::trace);
            sinks.push_back(stderr_sink);
        } else {
            // Daemon mode: log to file only
            auto file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file.string(), 5 * 1024 * 1024, 3);
            file_sink->set_level(spdlog::level::trace);
            sinks.push_back(file_sink);
        }

        // Create and register the logger
        auto logger = std::make_shared<spdlog::logger>("tg-fused", sinks.begin(), sinks.end());
        logger->set_level(log_level);
        logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to set up logging: " << e.what() << "\n";
        return false;
    }
}

/// Daemonise the process
/// @return 0 on success (in child), -1 on error, exits parent
int daemonise() {
    // First fork
    pid_t pid = fork();
    if (pid < 0) {
        spdlog::error("First fork failed: {}", strerror(errno));
        return -1;
    }
    if (pid > 0) {
        // Parent exits
        _exit(0);
    }

    // Create new session
    if (setsid() < 0) {
        spdlog::error("setsid failed: {}", strerror(errno));
        return -1;
    }

    // Second fork (prevent acquiring terminal)
    pid = fork();
    if (pid < 0) {
        spdlog::error("Second fork failed: {}", strerror(errno));
        return -1;
    }
    if (pid > 0) {
        // Parent exits
        _exit(0);
    }

    // Change working directory
    if (chdir("/") < 0) {
        spdlog::warn("Could not change to root directory");
    }

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    CLI::App app{"tg-fused - Telegram FUSE daemon"};

    std::string mount_point;
    bool foreground = false;
    int verbosity = 0;
    bool mock_mode = false;
    bool allow_other = false;

    app.add_option("mount_point", mount_point, "Mount point for the filesystem")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_flag("-f,--foreground", foreground, "Run in foreground (don't daemonise)");
    app.add_flag("-v,--verbose", verbosity, "Increase verbosity (-v, -vv, -vvv)");
    app.add_flag("--mock", mock_mode, "Use mock data (no Telegram connection)");
    app.add_flag("--allow-other", allow_other, "Allow other users to access the mount");

    CLI11_PARSE(app, argc, argv);

    // Set up logging (stderr in foreground mode, file only in daemon mode)
    if (!setup_logging(foreground, verbosity)) {
        std::cerr << "Failed to initialise logging\n";
        return 1;
    }

    spdlog::info("tg-fused starting...");
    if (!foreground) {
        spdlog::info("Log file: {}", (get_data_directory() / "logs" / "tg-fused.log").string());
    }
    spdlog::debug("Mount point: {}", mount_point);
    spdlog::debug("Foreground: {}", foreground);
    spdlog::debug("Verbosity: {}", verbosity);
    spdlog::debug("Mock mode: {}", mock_mode);

    // Create data provider
    std::shared_ptr<tgfuse::DataProvider> provider;
    if (mock_mode) {
        spdlog::info("Running in mock mode");
        provider = std::make_shared<tgfuse::MockDataProvider>();
    } else {
        // Future: Create TelegramDataProvider
        spdlog::error("Real Telegram mode not yet implemented, use --mock");
        return 1;
    }

    // Daemonise unless foreground mode
    if (!foreground) {
        spdlog::info("Daemonising...");
        spdlog::default_logger()->flush();
        if (daemonise() < 0) {
            return 1;
        }
    }

    // Create and mount VFS
    tgfuse::VirtualFilesystem vfs(provider);

    tgfuse::VfsConfig config;
    config.mount_point = mount_point;
    config.foreground = true;       // Always true after daemonisation or if -f specified
    config.debug = verbosity >= 2;  // FUSE debug only at -vv or higher
    config.allow_other = allow_other;

    spdlog::info("Mounting filesystem at: {}", mount_point);

    int result = vfs.mount(config);

    spdlog::info("tg-fused exiting with code: {}", result);
    return result;
}
