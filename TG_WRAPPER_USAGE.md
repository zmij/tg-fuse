# TG Wrapper Library Usage Guide

This document demonstrates how to use the TG wrapper library to interact with Telegram through TDLib.

## Overview

The TG wrapper provides a modern C++20 coroutine-based interface to TDLib with:
- **Persistent caching** using SQLite for offline access
- **Coroutine-based async API** for clean, sequential code
- **Type-safe structures** for users, chats, messages, and files
- **Exception-based error handling**
- **Upload deduplication** via file hash caching

## Quick Start

### 1. Initialise the Client

```cpp
#include "tg/client.hpp"
#include <spdlog/spdlog.h>

int main() {
    // Configure the client
    tg::TelegramClient::Config config;
    config.api_id = 12345;  // Get from https://my.telegram.org
    config.api_hash = "your_api_hash";
    config.database_directory = "/tmp/tg-data";
    config.files_directory = "/tmp/tg-files";
    config.logs_directory = "/tmp/tg-logs";  // Optional TDLib log directory

    // Create the client
    tg::TelegramClient client(config);

    // Start the client
    auto start_task = client.start();
    start_task.get_result();  // Wait for start to complete

    return 0;
}
```

### 2. Authentication Flow

```cpp
Task<void> authenticate(tg::TelegramClient& client) {
    // Check current auth state
    auto state = co_await client.get_auth_state();

    if (state == tg::AuthState::WAIT_PHONE) {
        spdlog::info("Enter phone number:");
        std::string phone;
        std::cin >> phone;

        co_await client.login(phone);
        state = co_await client.get_auth_state();
    }

    if (state == tg::AuthState::WAIT_CODE) {
        spdlog::info("Enter authentication code:");
        std::string code;
        std::cin >> code;

        co_await client.submit_code(code);
        state = co_await client.get_auth_state();
    }

    if (state == tg::AuthState::WAIT_PASSWORD) {
        spdlog::info("Enter 2FA password:");
        std::string password;
        std::cin >> password;

        co_await client.submit_password(password);
    }

    spdlog::info("Authentication complete!");
}
```

### 3. Listing Chats

```cpp
Task<void> list_all_chats(tg::TelegramClient& client) {
    // Get all chats
    auto chats = co_await client.get_all_chats();

    spdlog::info("Found {} chats:", chats.size());

    for (const auto& chat : chats) {
        spdlog::info("  {} - {} ({})",
            chat.get_directory_name(),
            chat.title,
            tg::chat_type_to_string(chat.type));
    }
}
```

### 4. Sending Messages

```cpp
Task<void> send_message_example(tg::TelegramClient& client) {
    // Resolve username to get chat
    auto chat = co_await client.resolve_username("alice");

    if (!chat) {
        throw tg::ChatNotFoundException("alice");
    }

    // Send text message
    auto message = co_await client.send_text(chat->id, "Hello from tg-fuse!");

    spdlog::info("Message sent with ID: {}", message.id);
}
```

### 5. Reading Messages

```cpp
Task<void> read_messages_example(tg::TelegramClient& client, int64_t chat_id) {
    // Get last 10 messages
    auto messages = co_await client.get_last_n_messages(chat_id, 10);

    spdlog::info("Last {} messages:", messages.size());

    for (const auto& msg : messages) {
        spdlog::info("{}", msg.format_for_display());
    }

    // Fetch messages until at least 50, or 7 days old
    auto more_messages = co_await client.get_messages_until(
        chat_id, 50, std::chrono::seconds(7 * 24 * 3600));
}
```

### 6. Sending Files

```cpp
Task<void> send_file_example(tg::TelegramClient& client, int64_t chat_id) {
    // Auto-detect mode based on file type
    auto msg = co_await client.send_file(chat_id, "/path/to/photo.jpg",
                                          tg::SendMode::AUTO);

    spdlog::info("File sent as message {}", msg.id);

    // Force send as document (original quality)
    auto doc_msg = co_await client.send_file(chat_id, "/path/to/photo.jpg",
                                              tg::SendMode::DOCUMENT);

    spdlog::info("File sent as document {}", doc_msg.id);

    // Send with hash for upload deduplication
    std::string file_hash = "sha256_hash_of_file";
    int64_t file_size = 12345;
    auto cached_msg = co_await client.send_file(chat_id, "/path/to/file.pdf",
                                                 tg::SendMode::DOCUMENT,
                                                 file_hash, file_size);
}
```

### 7. Reusing Uploads (Deduplication)

```cpp
Task<void> send_cached_file_example(tg::TelegramClient& client, int64_t chat_id) {
    // Check cache for existing upload
    std::string file_hash = compute_file_hash("/path/to/file.pdf");
    auto cached_id = client.cache().get_cached_upload(file_hash);

    if (cached_id) {
        // Reuse existing upload - avoids re-uploading to Telegram servers
        auto msg = co_await client.send_file_by_id(
            chat_id, *cached_id, "file.pdf", tg::SendMode::DOCUMENT);
        spdlog::info("Sent cached file as message {}", msg.id);
    } else {
        // Upload new file
        auto msg = co_await client.send_file(chat_id, "/path/to/file.pdf",
                                              tg::SendMode::DOCUMENT,
                                              file_hash, file_size);
        // Cache will be populated automatically on upload success
    }
}
```

### 8. Listing Files/Media

```cpp
Task<void> list_media_example(tg::TelegramClient& client, int64_t chat_id) {
    // List all media (photos, videos, animations)
    auto media = co_await client.list_media(chat_id);

    spdlog::info("Found {} media items:", media.size());

    for (const auto& item : media) {
        spdlog::info("  {} - {} ({})",
            item.filename,
            item.get_size_string(),
            tg::media_type_to_string(item.type));
    }

    // List all documents
    auto files = co_await client.list_files(chat_id);

    spdlog::info("Found {} document files:", files.size());

    for (const auto& item : files) {
        spdlog::info("  {} - {}", item.filename, item.get_size_string());
    }
}
```

### 9. Downloading Files

```cpp
Task<void> download_file_example(tg::TelegramClient& client,
                                  const std::string& file_id) {
    // Download to temporary location
    auto local_path = co_await client.download_file(file_id);

    spdlog::info("File downloaded to: {}", local_path);

    // Download to specific location
    auto specific_path = co_await client.download_file(file_id,
                                                        "/tmp/myfile.jpg");

    spdlog::info("File saved as: {}", specific_path);
}
```

### 10. Using the Cache

```cpp
void cache_example(tg::TelegramClient& client) {
    auto& cache = client.cache();

    // Get cached chat by username
    auto chat = cache.get_cached_chat_by_username("alice");

    if (chat) {
        spdlog::info("Found cached chat: {}", chat->title);
    }

    // Get all cached users
    auto users = cache.get_all_cached_users();

    spdlog::info("Cached {} users", users.size());

    // Upload deduplication cache
    auto remote_id = cache.get_cached_upload("file_hash_here");
    cache.cache_upload("file_hash", 12345, "remote_file_id");

    // Invalidate cache for a specific chat
    cache.invalidate_chat_messages(123456789);

    // Clear entire cache
    cache.clear_all();
}
```

### 11. Error Handling

```cpp
Task<void> error_handling_example(tg::TelegramClient& client) {
    try {
        auto chat = co_await client.resolve_username("nonexistent_user");

        if (!chat) {
            throw tg::ChatNotFoundException("nonexistent_user");
        }

    } catch (const tg::ChatNotFoundException& e) {
        spdlog::error("Chat not found: {}", e.what());

    } catch (const tg::NetworkException& e) {
        spdlog::error("Network error: {}", e.what());

    } catch (const tg::AuthenticationException& e) {
        spdlog::error("Authentication error: {}", e.what());

    } catch (const tg::TelegramException& e) {
        spdlog::error("Telegram error: {}", e.what());
    }
}
```

### 12. Message Callbacks

```cpp
void callback_example(tg::TelegramClient& client) {
    // Register callback for new messages
    client.set_message_callback([](const tg::Message& msg) {
        spdlog::info("New message in chat {}: {}", msg.chat_id, msg.text);
    });
}
```

## VFS Integration Example

Here's how the wrapper is used in the FUSE filesystem:

```cpp
// In FUSE readdir handler for "/users/"
Task<void> vfs_list_users(tg::TelegramClient& client, std::vector<std::string>& entries) {
    auto users = co_await client.get_users();

    for (const auto& user : users) {
        // Add directory name to FUSE listing
        entries.push_back(user.get_identifier());
    }
}

// In FUSE readdir handler for "/users/alice/"
void vfs_list_chat_dirs(std::vector<std::string>& entries) {
    entries.push_back(".info");
    entries.push_back("messages");
    entries.push_back("media");
    entries.push_back("files");
}

// In FUSE read handler for "/users/alice/messages"
Task<std::string> vfs_read_messages(tg::TelegramClient& client,
                                     int64_t chat_id,
                                     int count) {
    auto messages = co_await client.get_last_n_messages(chat_id, count);

    std::ostringstream oss;
    for (const auto& msg : messages) {
        oss << msg.format_for_display() << "\n";
    }

    co_return oss.str();
}

// In FUSE write handler for "/users/alice/messages"
Task<void> vfs_write_message(tg::TelegramClient& client,
                              int64_t chat_id,
                              const std::string& text) {
    auto msg = co_await client.send_text(chat_id, text);

    spdlog::info("Message sent: {}", msg.id);
}

// In FUSE readdir handler for "/users/alice/media"
Task<void> vfs_list_media(tg::TelegramClient& client,
                          int64_t chat_id,
                          std::vector<std::string>& entries) {
    auto items = co_await client.list_media(chat_id);

    for (const auto& item : items) {
        // Filename includes timestamp prefix: YYYYMMDD-HHMM-original.ext
        entries.push_back(item.filename);
    }
}

// In FUSE write handler for copying file to "/users/alice/media/newfile.jpg"
Task<void> vfs_send_file(tg::TelegramClient& client,
                         int64_t chat_id,
                         const std::string& source_path,
                         bool is_media_dir) {
    auto mode = is_media_dir ? tg::SendMode::MEDIA : tg::SendMode::DOCUMENT;

    auto msg = co_await client.send_file(chat_id, source_path, mode);

    spdlog::info("File sent as message {}", msg.id);
}
```

## Data Structures

### User
```cpp
struct User {
    int64_t id;
    std::string username;         // Without @ prefix
    std::string first_name;
    std::string last_name;
    std::string phone_number;
    bool is_contact;
    UserStatus status;
    int64_t last_seen;

    std::string display_name() const;      // "John Doe"
    std::string get_identifier() const;    // "@johndoe" or name
};
```

### Chat
```cpp
struct Chat {
    int64_t id;
    ChatType type;                // PRIVATE, GROUP, SUPERGROUP, CHANNEL
    std::string title;
    std::string username;         // For public groups/channels

    std::string get_directory_name() const;  // username or sanitised title
    bool is_private() const;
    bool is_group() const;
    bool is_channel() const;
};
```

### Message
```cpp
struct Message {
    int64_t id;
    int64_t chat_id;
    int64_t sender_id;
    int64_t timestamp;
    std::string text;
    std::optional<MediaInfo> media;
    bool is_outgoing;

    bool has_media() const;
    std::string format_for_display() const;
};
```

### MediaInfo
```cpp
struct MediaInfo {
    MediaType type;               // PHOTO, VIDEO, DOCUMENT, etc.
    std::string file_id;
    std::string filename;
    std::string mime_type;
    int64_t file_size;
    std::optional<std::string> local_path;
    std::optional<int32_t> width;
    std::optional<int32_t> height;
    std::optional<int32_t> duration;

    std::string get_extension() const;
};
```

### FileListItem
```cpp
struct FileListItem {
    int64_t message_id;
    std::string filename;         // With YYYYMMDD-HHMM- timestamp prefix
    int64_t file_size;
    int64_t timestamp;
    MediaType type;
    std::string file_id;

    std::string get_size_string() const;  // "1.2 MB"
};
```

## Exception Hierarchy

All exceptions inherit from `tg::TelegramException`:

- `AuthenticationException` - Authentication errors
  - `InvalidPhoneException`
  - `InvalidCodeException`
  - `InvalidPasswordException`
- `NetworkException` - Network/connectivity issues
  - `ConnectionException`
  - `TimeoutException`
- `EntityException` - Entity lookup failures
  - `ChatNotFoundException`
  - `UserNotFoundException`
  - `MessageNotFoundException`
- `FileException` - File operation errors
  - `FileNotFoundException`
  - `FileDownloadException`
  - `FileUploadException`
- `OperationException` - General operation failures
  - `PermissionDeniedException`
  - `RateLimitException`
- `CacheException` - Cache/database errors
  - `DatabaseException`
- `TdLibException` - Low-level TDLib errors

## Complete Example

```cpp
#include "tg/client.hpp"
#include <spdlog/spdlog.h>
#include <iostream>

Task<void> example_workflow(tg::TelegramClient& client) {
    try {
        // 1. Authenticate
        spdlog::info("Starting authentication...");
        auto state = co_await client.get_auth_state();

        if (state != tg::AuthState::READY) {
            // Handle authentication (simplified)
            spdlog::info("Authentication required");
        }

        // 2. Get current user
        auto me = co_await client.get_me();
        spdlog::info("Logged in as: {}", me.display_name());

        // 3. List all chats
        auto chats = co_await client.get_all_chats();
        spdlog::info("Found {} chats", chats.size());

        // 4. Find a specific user
        auto alice_chat = co_await client.resolve_username("alice");

        if (alice_chat) {
            // 5. Send a message
            auto msg = co_await client.send_text(alice_chat->id,
                                                  "Hello from C++20!");

            // 6. Read recent messages
            auto messages = co_await client.get_last_n_messages(alice_chat->id, 5);

            for (const auto& m : messages) {
                spdlog::info("{}", m.format_for_display());
            }

            // 7. Send a file
            auto file_msg = co_await client.send_file(alice_chat->id,
                                                       "/tmp/photo.jpg",
                                                       tg::SendMode::MEDIA);

            spdlog::info("Photo sent: {}", file_msg.id);

            // 8. List media
            auto media = co_await client.list_media(alice_chat->id);

            spdlog::info("Found {} media items", media.size());
        }

    } catch (const tg::TelegramException& e) {
        spdlog::error("Error: {}", e.what());
    }
}

int main() {
    // Setup logging
    spdlog::set_level(spdlog::level::info);

    // Configure client
    tg::TelegramClient::Config config;
    config.api_id = 12345;
    config.api_hash = "your_api_hash";
    config.database_directory = "/tmp/tg-data";
    config.files_directory = "/tmp/tg-files";

    // Create and start client
    tg::TelegramClient client(config);

    auto start_task = client.start();
    start_task.get_result();

    // Run workflow
    auto workflow_task = example_workflow(client);
    workflow_task.get_result();

    // Stop client
    auto stop_task = client.stop();
    stop_task.get_result();

    return 0;
}
```

## Notes

- **Thread Safety**: The cache is thread-safe with internal locking
- **Coroutines**: All async methods return `Task<T>` that can be `co_await`ed
- **Persistence**: Cache survives application restarts
- **TDLib Updates**: The client automatically processes TDLib updates in background
- **Upload Deduplication**: File hashes are cached to avoid re-uploading identical files
- **Async Uploads**: File uploads happen asynchronously; `cp` completes when data is written to temp file
