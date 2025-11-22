/**
 * Simple example demonstrating TG wrapper usage
 *
 * This example shows how to:
 * 1. Configure and start the TelegramClient
 * 2. Authenticate with Telegram
 * 3. List all chats
 * 4. Send a message
 * 5. Send a file
 *
 * Usage:
 *   ./simple_client <api_id> <api_hash>
 */

#include "tg/client.hpp"
#include "tg/exceptions.hpp"

#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

// Helper coroutine to run authentication flow
tg::Task<void> authenticate(tg::TelegramClient& client) {
    auto state = co_await client.get_auth_state();

    if (state == tg::AuthState::READY) {
        spdlog::info("Already authenticated!");
        co_return;
    }

    if (state == tg::AuthState::WAIT_PHONE) {
        std::cout << "Enter your phone number (with country code, e.g., +1234567890): ";
        std::string phone;
        std::getline(std::cin, phone);

        co_await client.login(phone);

        // Wait a bit for the code to arrive
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state = co_await client.get_auth_state();
    }

    if (state == tg::AuthState::WAIT_CODE) {
        std::cout << "Enter the authentication code sent to your phone: ";
        std::string code;
        std::getline(std::cin, code);

        co_await client.submit_code(code);

        // Wait for auth state to update
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state = co_await client.get_auth_state();
    }

    if (state == tg::AuthState::WAIT_PASSWORD) {
        std::cout << "Enter your 2FA password: ";
        std::string password;
        std::getline(std::cin, password);

        co_await client.submit_password(password);
    }

    spdlog::info("Authentication complete!");
}

// Main workflow coroutine
tg::Task<void> run_example(tg::TelegramClient& client) {
    try {
        // Step 1: Authenticate
        spdlog::info("Starting authentication...");
        co_await authenticate(client);

        // Step 2: List all chats
        spdlog::info("Fetching all chats...");
        auto chats = co_await client.get_all_chats();

        spdlog::info("Found {} chats:", chats.size());
        for (size_t i = 0; i < std::min(chats.size(), size_t(10)); ++i) {
            const auto& chat = chats[i];
            spdlog::info("  [{}] {} - {} ({})", i, chat.get_directory_name(), chat.title,
                         tg::chat_type_to_string(chat.type));
        }

        if (chats.empty()) {
            spdlog::warn("No chats found. Make sure you have some conversations in Telegram.");
            co_return;
        }

        // Step 3: Interactive menu
        std::cout << "\nWhat would you like to do?\n";
        std::cout << "1. Send a text message\n";
        std::cout << "2. View recent messages from a chat\n";
        std::cout << "3. List media from a chat\n";
        std::cout << "4. Exit\n";
        std::cout << "Choice: ";

        int choice;
        std::cin >> choice;
        std::cin.ignore();  // Clear newline

        if (choice == 4) {
            co_return;
        }

        std::cout << "Enter chat number (0-" << chats.size() - 1 << "): ";
        size_t chat_idx;
        std::cin >> chat_idx;
        std::cin.ignore();

        if (chat_idx >= chats.size()) {
            spdlog::error("Invalid chat number");
            co_return;
        }

        const auto& selected_chat = chats[chat_idx];
        spdlog::info("Selected chat: {}", selected_chat.title);

        if (choice == 1) {
            // Send text message
            std::cout << "Enter message text: ";
            std::string text;
            std::getline(std::cin, text);

            spdlog::info("Sending message...");
            auto msg = co_await client.send_text(selected_chat.id, text);
            spdlog::info("Message sent! Message ID: {}", msg.id);

        } else if (choice == 2) {
            // View recent messages
            spdlog::info("Fetching last 10 messages...");
            auto messages = co_await client.get_last_n_messages(selected_chat.id, 10);

            spdlog::info("Last {} messages:", messages.size());
            for (const auto& msg : messages) {
                spdlog::info("  {}", msg.format_for_display());
            }

        } else if (choice == 3) {
            // List media
            spdlog::info("Fetching media files...");
            auto media = co_await client.list_media(selected_chat.id);

            spdlog::info("Found {} media items:", media.size());
            for (size_t i = 0; i < std::min(media.size(), size_t(20)); ++i) {
                const auto& item = media[i];
                spdlog::info("  {} - {} ({})", item.filename, item.get_size_string(),
                             tg::media_type_to_string(item.type));
            }
        }

        spdlog::info("Done!");

    } catch (const tg::TelegramException& e) {
        spdlog::error("Telegram error: {}", e.what());
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
    }
}

int main(int argc, char* argv[]) {
    // Setup logging
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <api_id> <api_hash>\n";
        std::cerr << "\nGet your API credentials from https://my.telegram.org\n";
        return 1;
    }

    try {
        int api_id = std::stoi(argv[1]);
        std::string api_hash = argv[2];

        // Configure client
        tg::TelegramClient::Config config;
        config.api_id = api_id;
        config.api_hash = api_hash;
        config.database_directory = "/tmp/tg-fuse-example";
        config.files_directory = "/tmp/tg-fuse-example/files";

        spdlog::info("Initializing TelegramClient...");
        spdlog::info("Database: {}", config.database_directory);

        tg::TelegramClient client(config);

        // Start client
        auto start_task = client.start();
        start_task.get_result();

        spdlog::info("Client started successfully!");

        // Run main workflow
        auto workflow_task = run_example(client);
        workflow_task.get_result();

        // Stop client
        spdlog::info("Stopping client...");
        auto stop_task = client.stop();
        stop_task.get_result();

        spdlog::info("Goodbye!");

    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
