#include "users.hpp"
#include "config.hpp"

#include "tg/client.hpp"
#include "tg/exceptions.hpp"
#include "tg/types.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace tgfuse::ctl {

namespace {

tg::TelegramClient::Config make_client_config(const Config& config) {
    auto data_dir = get_data_dir();

    tg::TelegramClient::Config client_config{};
    client_config.api_id = config.api_id;
    client_config.api_hash = config.api_hash;
    client_config.database_directory = (data_dir / "tdlib").string();
    client_config.cache_directory = (data_dir / "cache").string();
    client_config.files_directory = (data_dir / "files").string();
    client_config.logs_directory = (data_dir / "logs").string();

    std::filesystem::create_directories(client_config.database_directory);
    std::filesystem::create_directories(client_config.cache_directory);
    std::filesystem::create_directories(client_config.files_directory);

    return client_config;
}

}  // namespace

int exec_users_list() {
    auto config = load_config();
    if (!config) {
        std::cerr << "Error: Not configured. Run 'tg-fuse login' first.\n";
        return 1;
    }

    auto client_config = make_client_config(*config);

    // Redirect spdlog to a file
    auto log_dir = get_data_dir() / "logs";
    std::filesystem::create_directories(log_dir);
    auto log_path = log_dir / "tg-fuse.log";

    try {
        auto file_logger = spdlog::basic_logger_mt("users", log_path.string(), true);
        spdlog::set_default_logger(file_logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
    } catch (const spdlog::spdlog_ex& e) {
        std::cerr << "Warning: Failed to create log file: " << e.what() << "\n";
    }

    try {
        tg::TelegramClient client(client_config);

        client.start().get_result();

        // Wait for TDLib to initialise
        std::this_thread::sleep_for(std::chrono::seconds(2));

        auto state = client.get_auth_state().get_result();

        if (state != tg::AuthState::READY) {
            std::cerr << "Error: Not authenticated. Run 'tg-fuse login' first.\n";
            client.stop().get_result();
            return 1;
        }

        auto users = client.get_users().get_result();

        std::cout << "Found " << users.size() << " users:\n";

        for (const auto& user : users) {
            std::cout << user.id;
            if (!user.username.empty()) {
                std::cout << "\t@" << user.username;
            } else {
                std::cout << "\t";
            }
            std::cout << "\t" << user.display_name();
            if (user.is_contact) {
                std::cout << "\t[contact]";
            }
            std::cout << "\n";
        }

        client.stop().get_result();
        return 0;

    } catch (const tg::TelegramException& e) {
        std::cerr << "Telegram error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace tgfuse::ctl
