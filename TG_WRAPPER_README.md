# TG Wrapper Library

A modern C++20 wrapper around TDLib providing a coroutine-based interface to Telegram with persistent caching.

## Features

- **C++20 Coroutines** - Clean, sequential async code
- **SQLite Caching** - Persistent offline access to chats, messages, and uploads
- **Type-Safe** - Strong typing for all Telegram entities
- **Per-Directory Behaviour** - Flexible file sending modes (media vs documents)
- **Exception-Based** - Clear error handling with hierarchical exceptions
- **Async by Default** - All operations return awaitable `Task<T>`
- **Upload Deduplication** - File hash caching avoids re-uploading to Telegram servers

## Quick Example

```cpp
#include "tg/client.hpp"

Task<void> send_hello(tg::TelegramClient& client) {
    // Find user by username
    auto chat = co_await client.resolve_username("alice");

    if (chat) {
        // Send a message
        co_await client.send_text(chat->id, "Hello from C++!");

        // Send a photo
        co_await client.send_file(chat->id, "/tmp/photo.jpg", tg::SendMode::MEDIA);
    }
}
```

## Architecture

```
TelegramClient (client.hpp)
    ├── Coroutine-based API
    ├── TDLib Integration
    └── CacheManager (cache.hpp)
            └── SQLite Database

Powered by:
    ├── Task<T> coroutines (async.hpp)
    ├── Type-safe structures (types.hpp)
    └── Exception hierarchy (exceptions.hpp)
```

## Core Components

### 1. TelegramClient

Main interface to Telegram:

```cpp
tg::TelegramClient::Config config;
config.api_id = 12345;
config.api_hash = "your_hash";
config.database_directory = "/tmp/tg-data";

tg::TelegramClient client(config);
co_await client.start();
```

**Authentication:**
```cpp
co_await client.login("+1234567890");
co_await client.submit_code("12345");
co_await client.submit_password("password");  // If 2FA enabled
```

**Operations:**
- `get_users()` - List users from private chats
- `get_groups()` - List groups and supergroups
- `get_channels()` - List channels
- `get_all_chats()` - All chats combined
- `resolve_username(username)` - Find chat by @username
- `get_chat(chat_id)` - Get chat by ID
- `get_user(user_id)` - Get user by ID
- `get_me()` - Get current logged-in user
- `send_text(chat_id, text)` - Send message
- `send_file(chat_id, path, mode)` - Upload file
- `get_messages(chat_id, limit)` - Get messages
- `get_last_n_messages(chat_id, n)` - Last N messages
- `get_messages_until(chat_id, min, max_age)` - Fetch until age threshold
- `list_media(chat_id)` - List photos/videos
- `list_files(chat_id)` - List documents
- `download_file(file_id)` - Download file

### 2. Data Structures

**User:**
```cpp
struct User {
    int64_t id;
    std::string username;      // Without @
    std::string first_name;
    std::string last_name;
    bool is_contact;
    UserStatus status;
    int64_t last_seen;

    std::string display_name() const;
    std::string get_identifier() const;  // @username or name
};
```

**Chat:**
```cpp
struct Chat {
    int64_t id;
    ChatType type;             // PRIVATE, GROUP, SUPERGROUP, CHANNEL
    std::string title;
    std::string username;

    std::string get_directory_name() const;  // username or sanitised title
    bool is_private() const;
    bool is_group() const;
};
```

**Message:**
```cpp
struct Message {
    int64_t id;
    int64_t chat_id;
    int64_t timestamp;
    std::string text;
    std::optional<MediaInfo> media;

    bool has_media() const;
    std::string format_for_display() const;
};
```

### 3. Coroutines

All async operations return `Task<T>`:

```cpp
Task<std::vector<Chat>> get_all_chats();
Task<Message> send_text(int64_t chat_id, const std::string& text);
Task<void> login(const std::string& phone);
```

Use with `co_await`:

```cpp
Task<void> example() {
    auto chats = co_await client.get_all_chats();

    for (const auto& chat : chats) {
        auto messages = co_await client.get_messages(chat.id, 10);
        // Process messages...
    }
}
```

### 4. Caching

SQLite-backed persistent cache:

```cpp
auto& cache = client.cache();

// Query cache
auto user = cache.get_cached_user_by_username("alice");
auto chats = cache.get_all_cached_chats();
auto messages = cache.get_cached_messages(chat_id, 100);

// Upload deduplication
auto remote_id = cache.get_cached_upload(file_hash);
cache.cache_upload(file_hash, file_size, remote_file_id);

// Manage cache
cache.invalidate_chat(chat_id);
cache.clear_all();
cache.vacuum();
```

**Cache Schema:**
- `users` - User contacts with metadata
- `chats` - All chats with last message info
- `messages` - Message history with media
- `files` - File metadata for quick listing
- `chat_message_stats` - Message statistics per chat
- `upload_cache` - File hash to remote ID mapping for deduplication

### 5. Error Handling

Exception hierarchy:

```cpp
try {
    auto chat = co_await client.resolve_username("invalid");

} catch (const tg::ChatNotFoundException& e) {
    // Specific error

} catch (const tg::NetworkException& e) {
    // Network issues

} catch (const tg::AuthenticationException& e) {
    // Auth problems

} catch (const tg::TelegramException& e) {
    // All Telegram errors
}
```

**Exception Types:**
- `AuthenticationException` - Login/auth errors
- `NetworkException` - Connectivity issues
- `ChatNotFoundException` - Entity not found
- `FileException` - File operation failures
- `DatabaseException` - Cache errors
- `TdLibException` - Low-level TDLib errors

## VFS Integration

Designed for FUSE filesystem integration:

### Directory Structure

```
/mnt/tg/
├── users/
│   └── alice/              # Private chat
│       ├── .info           # User information
│       ├── messages        # Read/write messages
│       ├── files/          # Documents
│       └── media/          # Photos/videos
├── contacts/
│   └── alice -> ../users/alice
├── groups/
│   └── mygroup/            # Group chat
│       ├── .info
│       ├── messages
│       ├── files/
│       └── media/
├── channels/
│   └── news/               # Channel
│       ├── .info
│       ├── messages
│       ├── files/
│       └── media/
├── @alice -> users/alice   # Quick access symlinks
└── self -> users/me        # Current user
```

### File Sending Modes

**AUTO Mode** (default):
```cpp
co_await client.send_file(chat_id, "photo.jpg", SendMode::AUTO);
// Detects type: .jpg -> sent as compressed photo
```

**MEDIA Mode** (photos/videos):
```cpp
co_await client.send_file(chat_id, "video.mp4", SendMode::MEDIA);
// Always compressed, good for media
```

**DOCUMENT Mode** (files):
```cpp
co_await client.send_file(chat_id, "photo.jpg", SendMode::DOCUMENT);
// Original quality, no compression
```

**Per-Directory Behaviour:**
- Copying to `/media/` -> SendMode::MEDIA
- Copying to `/files/` -> SendMode::DOCUMENT
- Copying to chat directory -> SendMode::AUTO (auto-detect)

## Building

The wrapper is built as a static library `tglib`:

```bash
# Using Makefile
make build-debug
make build-release

# Using CMake
mkdir build && cd build
cmake ..
make
```

**Dependencies:**
- C++20 compiler
- CMake 3.20+
- SQLite3 (system library)
- TDLib (auto-fetched)
- spdlog (auto-fetched)

**CMake Integration:**

```cmake
# Link against tglib
target_link_libraries(your_target PRIVATE tglib)

# Include directories
target_include_directories(your_target PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

## API Reference

### TelegramClient Methods

**Lifecycle:**
- `Task<void> start()` - Initialise and start client
- `Task<void> stop()` - Gracefully shutdown

**Authentication:**
- `Task<AuthState> get_auth_state()` - Check auth status
- `Task<void> login(phone)` - Begin login with phone
- `Task<void> submit_code(code)` - Submit verification code
- `Task<void> submit_password(password)` - Submit 2FA password
- `Task<void> logout()` - End session

**Entities:**
- `Task<vector<User>> get_users()` - Users from private chats
- `Task<vector<Chat>> get_groups()` - All groups
- `Task<vector<Chat>> get_channels()` - All channels
- `Task<vector<Chat>> get_all_chats()` - Everything
- `Task<optional<Chat>> resolve_username(username)` - Lookup by @username
- `Task<optional<Chat>> get_chat(chat_id)` - Get chat by ID
- `Task<optional<User>> get_user(user_id)` - Get user by ID
- `Task<User> get_me()` - Get current user

**Messaging:**
- `Task<Message> send_text(chat_id, text)` - Send message
- `Task<vector<Message>> get_messages(chat_id, limit)` - Get messages
- `Task<vector<Message>> get_last_n_messages(chat_id, n)` - Last N messages
- `Task<vector<Message>> get_messages_until(chat_id, min, max_age)` - Fetch by age

**Files:**
- `Task<Message> send_file(chat_id, path, mode)` - Upload file
- `Task<Message> send_file(chat_id, path, mode, hash, size)` - Upload with caching
- `Task<Message> send_file_by_id(chat_id, remote_id, filename, mode)` - Send cached file
- `Task<vector<FileListItem>> list_media(chat_id)` - List media
- `Task<vector<FileListItem>> list_files(chat_id)` - List documents
- `Task<string> download_file(file_id, dest)` - Download file

**Status:**
- `Task<ChatStatus> get_chat_status(chat_id)` - Last message info
- `Task<string> get_user_bio(user_id)` - Get user bio

**Callbacks:**
- `void set_message_callback(callback)` - Register for new messages

**Cache:**
- `CacheManager& cache()` - Access cache manager

### CacheManager Methods

**Users:**
- `cache_user(user)` - Store user
- `get_cached_user(id)` - Get by ID
- `get_cached_user_by_username(username)` - Get by username
- `get_all_cached_users()` - All users

**Chats:**
- `cache_chat(chat)` - Store chat
- `get_cached_chat(id)` - Get by ID
- `get_cached_chat_by_username(username)` - Get by username
- `get_all_cached_chats()` - All chats
- `get_cached_chats_by_type(type)` - Filter by type

**Messages:**
- `cache_message(msg)` - Store message
- `cache_messages(messages)` - Bulk store
- `get_cached_message(chat_id, message_id)` - Get specific
- `get_cached_messages(chat_id, limit)` - Get recent
- `get_last_n_messages(chat_id, n)` - Last N
- `get_messages_for_display(chat_id, max_age)` - For formatted output

**Files:**
- `cache_file_item(chat_id, item)` - Store file metadata
- `cache_file_list(chat_id, items)` - Bulk store
- `get_cached_file_list(chat_id, type)` - Get files

**Upload Cache:**
- `get_cached_upload(file_hash)` - Get cached remote file ID
- `cache_upload(hash, size, remote_id)` - Cache upload result
- `invalidate_upload(hash)` - Remove cached upload

**Maintenance:**
- `invalidate_chat_messages(chat_id)` - Clear messages
- `invalidate_chat_files(chat_id)` - Clear files
- `invalidate_chat(chat_id)` - Clear everything
- `clear_all()` - Wipe cache
- `vacuum()` - Optimise database
- `cleanup_old_messages(timestamp)` - Remove old
- `evict_old_messages(chat_id, timestamp)` - Per-chat cleanup

## Documentation

- **[TG_WRAPPER_USAGE.md](TG_WRAPPER_USAGE.md)** - Detailed usage guide with examples
- **[TG_WRAPPER_DESIGN.md](TG_WRAPPER_DESIGN.md)** - Architecture and design decisions
- **[README.md](README.md)** - Main project documentation

## License

Same as parent project (see [LICENSE](LICENSE))

## Contributing

This is part of the tg-fuse project. See main [README.md](README.md) for contribution guidelines.
