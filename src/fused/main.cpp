#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include "fuse/mock_provider.hpp"
#include "fuse/telegram_provider.hpp"
#include "fuse/vfs.hpp"
#include "tg/client.hpp"
#include "tg/types.hpp"

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

namespace {

namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// Configuration structures
//------------------------------------------------------------------------------

/// Daemon configuration from command line
struct DaemonConfig {
    std::string mount_point;
    bool foreground{false};
    int verbosity{0};
    bool mock_mode{false};
    bool allow_other{false};
    bool flush_logs{false};  // Flush logs on every message (useful for debugging)
};

/// API configuration from config file
struct ApiConfig {
    int32_t api_id{0};
    std::string api_hash;

    bool is_valid() const { return api_id > 0 && !api_hash.empty(); }
};

//------------------------------------------------------------------------------
// Path helpers
//------------------------------------------------------------------------------

/// Get the data directory for tg-fuse (~/.local/share/tg-fuse)
fs::path get_data_directory() {
    if (const char* xdg_data = std::getenv("XDG_DATA_HOME"); xdg_data && *xdg_data) {
        return fs::path(xdg_data) / "tg-fuse";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".local" / "share" / "tg-fuse";
    }
    if (struct passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) {
        return fs::path(pw->pw_dir) / ".local" / "share" / "tg-fuse";
    }
    return fs::path("/tmp") / "tg-fuse";
}

/// Get the config directory for tg-fuse (~/.config/tg-fuse)
fs::path get_config_directory() {
    if (const char* xdg_config = std::getenv("XDG_CONFIG_HOME"); xdg_config && *xdg_config) {
        return fs::path(xdg_config) / "tg-fuse";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return fs::path(home) / ".config" / "tg-fuse";
    }
    if (struct passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) {
        return fs::path(pw->pw_dir) / ".config" / "tg-fuse";
    }
    return ".config/tg-fuse";
}

//------------------------------------------------------------------------------
// Configuration loading
//------------------------------------------------------------------------------

/// Load API configuration from config file
std::optional<ApiConfig> load_api_config() {
    auto config_path = get_config_directory() / "config.json";

    if (!fs::exists(config_path)) {
        return std::nullopt;
    }

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json j;
        file >> j;

        ApiConfig config;
        config.api_id = j.value("api_id", 0);
        config.api_hash = j.value("api_hash", "");

        if (!config.is_valid()) {
            return std::nullopt;
        }

        return config;
    } catch (...) {
        return std::nullopt;
    }
}

/// Create TelegramClient configuration
tg::TelegramClient::Config make_client_config(const ApiConfig& api_config) {
    auto data_dir = get_data_directory();

    tg::TelegramClient::Config config;
    config.api_id = api_config.api_id;
    config.api_hash = api_config.api_hash;
    config.database_directory = (data_dir / "tdlib").string();
    config.files_directory = (data_dir / "files").string();
    config.logs_directory = (data_dir / "logs").string();

    fs::create_directories(config.database_directory);
    fs::create_directories(config.files_directory);

    return config;
}

//------------------------------------------------------------------------------
// Logging
//------------------------------------------------------------------------------

/// Convert verbosity level to spdlog level
spdlog::level::level_enum verbosity_to_level(int verbosity) {
    if (verbosity >= 2) return spdlog::level::trace;
    if (verbosity == 1) return spdlog::level::debug;
    return spdlog::level::info;
}

/// Set up logging with file and optional stderr output
bool setup_logging(const DaemonConfig& config) {
    try {
        fs::path log_dir = get_data_directory() / "logs";
        std::error_code ec;
        fs::create_directories(log_dir, ec);
        if (ec) {
            return false;
        }

        fs::path log_file = log_dir / "tg-fused.log";
        auto log_level = verbosity_to_level(config.verbosity);

        std::vector<spdlog::sink_ptr> sinks;

        if (config.foreground) {
            auto stderr_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            stderr_sink->set_level(spdlog::level::trace);
            sinks.push_back(stderr_sink);
        } else {
            auto file_sink =
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file.string(), 5 * 1024 * 1024, 3);
            file_sink->set_level(spdlog::level::trace);
            sinks.push_back(file_sink);
        }

        auto logger = std::make_shared<spdlog::logger>("tg-fused", sinks.begin(), sinks.end());
        logger->set_level(log_level);

        // Set flush level based on config
        if (config.flush_logs) {
            logger->flush_on(spdlog::level::trace);  // Flush every message
        } else {
            logger->flush_on(spdlog::level::warn);
        }

        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        return true;
    } catch (...) {
        return false;
    }
}

//------------------------------------------------------------------------------
// Daemonisation
//------------------------------------------------------------------------------

/// Daemonise the process
/// @return 0 on success (in child), -1 on error, parent exits
int daemonise() {
    // First fork
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid > 0) {
        _exit(0);
    }

    // Create new session
    if (setsid() < 0) {
        return -1;
    }

    // Second fork (prevent acquiring terminal)
    pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid > 0) {
        _exit(0);
    }

    // Change working directory
    chdir("/");

    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return 0;
}

//------------------------------------------------------------------------------
// Daemon context - holds all runtime state
//------------------------------------------------------------------------------

struct DaemonContext {
    std::unique_ptr<tg::TelegramClient> telegram_client;
    std::shared_ptr<tgfuse::DataProvider> provider;
};

//------------------------------------------------------------------------------
// Initialisation - called AFTER daemonising
//------------------------------------------------------------------------------

/// Initialise the daemon (logging, Telegram client, data provider)
/// @return Daemon context on success, nullopt on failure
std::optional<DaemonContext> initialise(const DaemonConfig& config) {
    // Set up logging first
    if (!setup_logging(config)) {
        return std::nullopt;
    }

    spdlog::info("tg-fused starting...");
    if (!config.foreground) {
        spdlog::info("Log file: {}", (get_data_directory() / "logs" / "tg-fused.log").string());
    }
    spdlog::debug("Mount point: {}", config.mount_point);
    spdlog::debug("Foreground: {}", config.foreground);
    spdlog::debug("Verbosity: {}", config.verbosity);
    spdlog::debug("Mock mode: {}", config.mock_mode);

    DaemonContext ctx;

    if (config.mock_mode) {
        spdlog::info("Running in mock mode");
        ctx.provider = std::make_shared<tgfuse::MockDataProvider>();
        return ctx;
    }

    // Load API configuration
    auto api_config = load_api_config();
    if (!api_config) {
        spdlog::error("Not configured. Run 'tg-fuse login' first.");
        return std::nullopt;
    }

    // Create and start Telegram client
    auto client_config = make_client_config(*api_config);
    ctx.telegram_client = std::make_unique<tg::TelegramClient>(client_config);

    spdlog::info("Starting Telegram client...");
    ctx.telegram_client->start().get_result();

    // Give TDLib a moment to initialise
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Check authentication state
    auto auth_state = ctx.telegram_client->get_auth_state().get_result();
    if (auth_state != tg::AuthState::READY) {
        spdlog::error("Not authenticated. Run 'tg-fuse login' first.");
        ctx.telegram_client->stop().get_result();
        return std::nullopt;
    }

    spdlog::info("Authenticated with Telegram");

    // Create TelegramDataProvider
    ctx.provider = std::make_shared<tgfuse::TelegramDataProvider>(*ctx.telegram_client);

    return ctx;
}

//------------------------------------------------------------------------------
// Main work loop - called AFTER initialisation
//------------------------------------------------------------------------------

/// Run the FUSE filesystem
/// @return Exit code
int run(const DaemonConfig& config, DaemonContext& ctx) {
    // Set mount point for absolute symlink paths
    ctx.provider->set_mount_point(config.mount_point);

    tgfuse::VirtualFilesystem vfs(ctx.provider);

    tgfuse::VfsConfig vfs_config;
    vfs_config.mount_point = config.mount_point;
    vfs_config.foreground = true;  // Always true after daemonisation
    vfs_config.debug = config.verbosity >= 2;
    vfs_config.allow_other = config.allow_other;

    spdlog::info("Mounting filesystem at: {}", config.mount_point);

    int result = vfs.mount(vfs_config);

    // Cleanup Telegram client
    if (ctx.telegram_client) {
        spdlog::info("Stopping Telegram client...");
        ctx.telegram_client->stop().get_result();
    }

    spdlog::info("tg-fused exiting with code: {}", result);
    return result;
}

}  // namespace

//------------------------------------------------------------------------------
// Main entry point
//------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    CLI::App app{"tg-fused - Telegram FUSE daemon"};

    DaemonConfig config;

    app.add_option("mount_point", config.mount_point, "Mount point for the filesystem")
        ->required()
        ->check(CLI::ExistingDirectory);

    app.add_flag("-f,--foreground", config.foreground, "Run in foreground (don't daemonise)");
    app.add_flag("-v,--verbose", config.verbosity, "Increase verbosity (-v, -vv, -vvv)");
    app.add_flag("--flush-logs", config.flush_logs, "Flush logs immediately (useful for debugging)");
    app.add_flag("--mock", config.mock_mode, "Use mock data (no Telegram connection)");
    app.add_flag("--allow-other", config.allow_other, "Allow other users to access the mount");

    CLI11_PARSE(app, argc, argv);

    // Pre-flight checks BEFORE daemonising (so errors go to stderr)
    if (!config.mock_mode) {
        if (!load_api_config()) {
            std::cerr << "Error: Not configured. Run 'tg-fuse login' first.\n";
            return 1;
        }
    }

    // Daemonise BEFORE creating any threads or connections
    if (!config.foreground) {
        if (daemonise() < 0) {
            std::cerr << "Failed to daemonise\n";
            return 1;
        }
    }

    // All initialisation happens AFTER daemonising
    auto ctx = initialise(config);
    if (!ctx) {
        return 1;
    }

    // Run the FUSE filesystem
    return run(config, *ctx);
}
