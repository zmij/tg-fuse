#include "login.hpp"
#include "config.hpp"

#include "tg/client.hpp"
#include "tg/exceptions.hpp"
#include "tg/types.hpp"

#include <spdlog/spdlog.h>

#include <termios.h>
#include <unistd.h>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace tgfuse::ctl {

namespace {

/// Read a line from stdin with optional echo disabled (for passwords)
std::string read_line(const std::string& prompt, bool hide_input = false) {
    std::cout << prompt << std::flush;

    termios oldt{};
    if (hide_input) {
        tcgetattr(STDIN_FILENO, &oldt);
        termios newt = oldt;
        newt.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }

    std::string line;
    std::getline(std::cin, line);

    if (hide_input) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        std::cout << std::endl;
    }

    return line;
}

/// Wait for auth state to change, showing progress dots
tg::AuthState wait_for_state_change(tg::TelegramClient& client, tg::AuthState current_state) {
    // Check immediately first
    auto state_task = client.get_auth_state();
    auto new_state = state_task.get_result();
    if (new_state != current_state) {
        return new_state;
    }

    // Then poll with dots
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::cout << "." << std::flush;

        state_task = client.get_auth_state();
        new_state = state_task.get_result();

        if (new_state != current_state) {
            std::cout << "\n";
            return new_state;
        }
    }
}

/// Create TelegramClient config from our config
tg::TelegramClient::Config make_client_config(const Config& config) {
    auto data_dir = get_data_dir();

    tg::TelegramClient::Config client_config{};
    client_config.api_id = config.api_id;
    client_config.api_hash = config.api_hash;
    client_config.database_directory = (data_dir / "tdlib").string();
    client_config.cache_directory = (data_dir / "cache").string();
    client_config.files_directory = (data_dir / "files").string();
    client_config.logs_directory = (data_dir / "logs").string();

    // Ensure directories exist
    std::filesystem::create_directories(client_config.database_directory);
    std::filesystem::create_directories(client_config.cache_directory);
    std::filesystem::create_directories(client_config.files_directory);

    return client_config;
}

}  // namespace

int exec_login() {
    setup_file_logging();

    auto config = load_config();
    if (!config) {
        std::cerr << "Error: API credentials not configured.\n";
        std::cerr << "Run 'tg-fuse config set --api-id=XXX --api-hash=YYY' first.\n";
        std::cerr << "Get credentials at: https://my.telegram.org/apps\n";
        return 1;
    }

    auto client_config = make_client_config(*config);

    try {
        tg::TelegramClient client(client_config);

        // Start the client
        spdlog::debug("Starting Telegram client...");
        auto start_task = client.start();
        start_task.get_result();

        // Give TDLib a moment to initialise and determine auth state
        std::cout << "Connecting..." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "\n";

        auto state_task = client.get_auth_state();
        auto state = state_task.get_result();

        if (state == tg::AuthState::READY) {
            std::cout << "Already authenticated.\n";
            auto stop_task = client.stop();
            stop_task.get_result();
            return 0;
        }

        // Authentication loop
        while (state != tg::AuthState::READY) {
            switch (state) {
                case tg::AuthState::WAIT_PHONE: {
                    std::string phone = read_line("Enter phone number (e.g. +1234567890): ");
                    if (phone.empty()) {
                        std::cerr << "Phone number cannot be empty.\n";
                        continue;
                    }
                    auto login_task = client.login(phone);
                    login_task.get_result();
                    std::cout << "Sending" << std::flush;
                    state = wait_for_state_change(client, state);
                    break;
                }

                case tg::AuthState::WAIT_CODE: {
                    std::string code = read_line("Enter verification code: ");
                    if (code.empty()) {
                        std::cerr << "Code cannot be empty.\n";
                        continue;
                    }
                    auto code_task = client.submit_code(code);
                    code_task.get_result();
                    std::cout << "Verifying" << std::flush;
                    state = wait_for_state_change(client, state);
                    break;
                }

                case tg::AuthState::WAIT_PASSWORD: {
                    std::string password = read_line("Enter 2FA password: ", true);
                    if (password.empty()) {
                        std::cerr << "Password cannot be empty.\n";
                        continue;
                    }
                    auto password_task = client.submit_password(password);
                    password_task.get_result();
                    std::cout << "Verifying" << std::flush;
                    state = wait_for_state_change(client, state);
                    break;
                }

                case tg::AuthState::READY:
                    break;
            }
        }

        std::cout << "\nSuccessfully authenticated with Telegram!\n";
        std::cout << "You can now mount the filesystem with: tg-fuse mount <mount_point>\n";

        // Stop the client gracefully
        auto stop_task = client.stop();
        stop_task.get_result();

        return 0;

    } catch (const tg::AuthenticationException& e) {
        std::cerr << "Authentication error: " << e.what() << "\n";
        return 1;
    } catch (const tg::TelegramException& e) {
        std::cerr << "Telegram error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int exec_logout() {
    setup_file_logging();

    auto config = load_config();
    if (!config) {
        std::cerr << "Not configured. Run 'tg-fuse login' first.\n";
        return 1;
    }

    auto data_dir = get_data_dir();
    if (!std::filesystem::exists(data_dir)) {
        std::cout << "Not logged in.\n";
        return 0;
    }

    auto client_config = make_client_config(*config);

    try {
        tg::TelegramClient client(client_config);

        auto start_task = client.start();
        start_task.get_result();

        auto state_task = client.get_auth_state();
        auto state = state_task.get_result();

        if (state != tg::AuthState::READY) {
            std::cout << "Not logged in.\n";
            return 0;
        }

        std::cout << "Logging out...\n";
        auto logout_task = client.logout();
        logout_task.get_result();

        std::cout << "Successfully logged out.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int exec_status() {
    setup_file_logging();

    auto config = load_config();
    if (!config) {
        std::cout << "Status: Not configured\n";
        std::cout << "Run 'tg-fuse login' to authenticate.\n";
        return 0;
    }

    auto data_dir = get_data_dir();
    if (!std::filesystem::exists(data_dir / "tdlib")) {
        std::cout << "Status: Not authenticated\n";
        std::cout << "Run 'tg-fuse login' to authenticate.\n";
        return 0;
    }

    auto client_config = make_client_config(*config);

    try {
        tg::TelegramClient client(client_config);

        auto start_task = client.start();
        start_task.get_result();

        auto state_task = client.get_auth_state();
        auto state = state_task.get_result();

        switch (state) {
            case tg::AuthState::READY:
                std::cout << "Status: Authenticated\n";
                break;
            case tg::AuthState::WAIT_PHONE:
                std::cout << "Status: Not authenticated\n";
                break;
            case tg::AuthState::WAIT_CODE:
                std::cout << "Status: Pending (waiting for verification code)\n";
                break;
            case tg::AuthState::WAIT_PASSWORD:
                std::cout << "Status: Pending (waiting for 2FA password)\n";
                break;
        }

        auto stop_task = client.stop();
        stop_task.get_result();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace tgfuse::ctl
