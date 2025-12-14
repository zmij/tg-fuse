# TG Wrapper Design & Implementation Summary

## Overview

A modern C++20 wrapper around TDLib (Telegram Database Library) providing:
- Coroutine-based asynchronous API
- Persistent SQLite caching
- Type-safe data structures
- Exception-based error handling
- Per-directory file type behaviour for VFS integration
- Upload deduplication via file hash caching

## Architecture

```
┌─────────────────────────────────────────┐
│         FUSE Filesystem Layer            │
│      (reads/writes/directory ops)        │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│       TelegramClient (client.hpp)        │
│   ┌─────────────────────────────────┐   │
│   │  Public Coroutine-based API     │   │
│   │  - get_all_chats()              │   │
│   │  - send_text()                  │   │
│   │  - send_file() / send_file_by_id│   │
│   │  - list_media() / list_files()  │   │
│   └─────────────────────────────────┘   │
└──────┬────────────────────────┬─────────┘
       │                        │
       ▼                        ▼
┌─────────────────┐    ┌─────────────────┐
│  CacheManager   │    │  TDLib Client   │
│  (cache.hpp)    │    │  (Impl class)   │
│                 │    │                 │
│  SQLite DB      │    │  Async Updates  │
│  - users        │    │  - Events       │
│  - chats        │    │  - Callbacks    │
│  - messages     │    │  - Queries      │
│  - files        │    │                 │
│  - upload_cache │    │                 │
└─────────────────┘    └─────────────────┘
       │                        │
       ▼                        ▼
┌─────────────────────────────────────────┐
│         Coroutine Infrastructure         │
│            Task<T> / TdPromise<T>        │
│         (async.hpp - C++20 coroutines)   │
└─────────────────────────────────────────┘
```

## Components Implemented

### 1. Data Structures (`include/tg/types.hpp`)

**Core Types:**
- `User` - Telegram user with username, names, contact status
- `Chat` - Represents DMs, groups, supergroups, channels
- `Message` - Text and media messages with timestamps
- `MediaInfo` - File metadata (type, size, dimensions, etc.)
- `FileListItem` - Simplified file listing for VFS
- `ChatStatus` - Last message ID and timestamp
- `ChatMessageStats` - Message statistics per chat

**Enumerations:**
- `ChatType` - PRIVATE, GROUP, SUPERGROUP, CHANNEL
- `MediaType` - PHOTO, VIDEO, DOCUMENT, AUDIO, VOICE, ANIMATION, STICKER, VIDEO_NOTE
- `SendMode` - AUTO, MEDIA, DOCUMENT
- `AuthState` - WAIT_PHONE, WAIT_CODE, WAIT_PASSWORD, READY
- `UserStatus` - ONLINE, OFFLINE, RECENTLY, LAST_WEEK, LAST_MONTH, LONG_AGO

**Helper Functions:**
- `chat_type_to_string()` - Convert enum to readable string
- `media_type_to_string()` - Convert enum to readable string
- `detect_media_type()` - Auto-detect from filename/MIME
- `is_media_type()` / `is_document_type()` - Type classification
- Member methods: `display_name()`, `get_identifier()`, `get_directory_name()`, etc.

### 2. Exception Hierarchy (`include/tg/exceptions.hpp`)

**Base:**
- `TelegramException` - All Telegram errors

**Categories:**
- **Authentication**: `InvalidPhoneException`, `InvalidCodeException`, `InvalidPasswordException`
- **Network**: `ConnectionException`, `TimeoutException`
- **Entity**: `ChatNotFoundException`, `UserNotFoundException`, `MessageNotFoundException`
- **File**: `FileNotFoundException`, `FileDownloadException`, `FileUploadException`
- **Operation**: `PermissionDeniedException`, `RateLimitException`
- **Cache**: `DatabaseException`
- **TDLib**: `TdLibException` (with error code)

### 3. Coroutine Infrastructure (`include/tg/async.hpp`)

**Task<T> Awaitable Type:**
- Promise type with proper suspend/resume semantics
- Move-only semantics (no copy)
- Support for both value and void returns
- Exception propagation through coroutines
- Manual resume capability via `get_result()`

**TdPromise<T> Bridge:**
- Converts TDLib callback-based API to coroutines
- Awaitable interface (`co_await promise`)
- `set_value()` / `set_exception()` for callback completion
- Handles both void and non-void return types

**Features:**
- Full C++20 coroutine support
- Lazy evaluation (suspended at start)
- Continuation chain support
- Thread-safe completion

### 4. Cache Manager (`include/tg/cache.hpp`, `src/tg/cache.cpp`)

**SQLite Schema:**

```sql
-- Users table
users (id, username, first_name, last_name, phone_number,
       is_contact, status, last_seen, updated_at)

-- Chats table
chats (id, type, title, username, updated_at)

-- Messages table
messages (id, chat_id, sender_id, timestamp, text, is_outgoing,
          media_type, media_file_id, media_filename, media_mime_type,
          media_file_size, media_local_path, media_width,
          media_height, media_duration)

-- Files table
files (message_id, chat_id, filename, file_size, timestamp,
       type, file_id)

-- Chat message statistics
chat_message_stats (chat_id, message_count, content_size,
                    last_message_time, last_fetch_time, oldest_message_time)

-- Upload deduplication cache
upload_cache (file_hash, file_size, remote_file_id, created_at)
```

**Indices:**
- `idx_users_username` - Fast username lookup
- `idx_chats_username` - Fast chat username lookup
- `idx_chats_type` - Filter by chat type
- `idx_messages_chat_timestamp` - Chronological message retrieval
- `idx_messages_media` - Media-only message filtering
- `idx_files_chat_type` - File type filtering
- `idx_files_timestamp` - Chronological file listing
- `idx_upload_cache_size` - File size lookup

**Features:**
- WAL mode for better concurrency
- Foreign key enforcement
- Thread-safe with mutex protection
- Automatic timestamp tracking
- Cache invalidation per chat
- Vacuum and cleanup utilities
- Upload deduplication cache

**API:**
- User/Chat caching and retrieval
- Message bulk caching
- File metadata caching
- Username-based lookups
- Type-filtered queries
- Selective invalidation
- Upload cache for file deduplication

### 5. Telegram Client (`include/tg/client.hpp`, `src/tg/client.cpp`)

**Configuration:**
```cpp
struct Config {
    int32_t api_id;
    std::string api_hash;
    std::string database_directory;
    std::string files_directory;
    std::string logs_directory;       // Optional TDLib log directory
    int32_t log_verbosity = 2;
    bool use_test_dc;
    bool use_file_database;
    bool use_chat_info_database;
    bool use_message_database;
    bool enable_storage_optimiser;
};
```

**Implementation Architecture:**

**Impl Class (Private):**
- Manages TDLib client lifecycle
- Dedicated update processing thread
- Callback registration and dispatch
- Authorization state machine
- Query ID management
- Thread-safe callback map
- Pending upload tracking for deduplication

**Public API (Coroutine-based):**

*Lifecycle:*
- `start()` - Initialise TDLib client
- `stop()` - Shutdown gracefully

*Authentication:*
- `get_auth_state()` - Check current state
- `login(phone)` - Send phone number
- `submit_code(code)` - Submit auth code
- `submit_password(password)` - Submit 2FA password
- `logout()` - End session

*Entity Listing:*
- `get_users()` - Users from private chats
- `get_groups()` - All groups/supergroups
- `get_channels()` - All channels
- `get_all_chats()` - All chats combined

*Entity Lookup:*
- `resolve_username(username)` - Find by @username
- `get_chat(chat_id)` - Get chat by ID
- `get_user(user_id)` - Get user by ID
- `get_me()` - Get current logged-in user

*Messaging:*
- `send_text(chat_id, text)` - Send text message
- `get_messages(chat_id, limit)` - Get recent messages
- `get_last_n_messages(chat_id, n)` - Get last N messages
- `get_messages_until(chat_id, min, max_age)` - Fetch until age threshold

*File Operations:*
- `send_file(chat_id, path, mode)` - Upload and send file
- `send_file(chat_id, path, mode, hash, size)` - Upload with caching
- `send_file_by_id(chat_id, remote_id, filename, mode)` - Send cached file
- `list_media(chat_id)` - List photos/videos/animations
- `list_files(chat_id)` - List documents/audio
- `download_file(file_id, dest)` - Download file

*Status:*
- `get_chat_status(chat_id)` - Get last message info
- `get_user_bio(user_id)` - Get user bio (lazy loaded)

*Cache Access:*
- `cache()` - Direct access to CacheManager

*Callbacks:*
- `set_message_callback(callback)` - Register for new messages

**Update Processing:**
- Background thread continuously polls TDLib
- Updates dispatched to handlers
- Authorization state updates tracked
- New chat/message updates logged
- Callback-based query responses
- `updateMessageSendSucceeded` - Caches remote file ID for deduplication
- `updateMessageSendFailed` - Logs upload failures

### 6. Build System

**CMake Structure:**

```cmake
# Root CMakeLists.txt
- SQLite3 via find_package
- TDLib via FetchContent (master branch)
- nlohmann/json, spdlog, CLI11, GoogleTest via FetchContent
- C++17 base (TDLib requirement)
- Per-target C++ standard override support

# src/CMakeLists.txt
- tglib static library (C++20, coroutines)
  - tg/types.cpp
  - tg/cache.cpp
  - tg/client.cpp
- tg-fuse executable (C++20)
  - Links: tglib, fuse, json, spdlog, CLI11
```

**Dependencies:**
- **System**: FUSE (macFUSE/libfuse3), SQLite3, C++20 compiler
- **Automatic**: TDLib, nlohmann/json, spdlog, CLI11, GoogleTest

**Compiler Flags:**
- `-Wall -Wextra -Wpedantic`
- Debug: `-g`
- Release: `-O3`

## VFS Integration Pattern

### Directory Structure

```
/mnt/tg/  (or /Volumes/tg on macOS)
├── users/
│   └── alice/              # Private chat with user "alice"
│       ├── .info           # User information (read-only)
│       ├── messages        # Read/write text messages
│       ├── media/          # Photos, videos, animations
│       │   └── 20241205-1430-photo.jpg
│       └── files/          # Documents, audio files
│           └── 20241205-1445-document.pdf
├── contacts/
│   └── alice -> ../users/alice   # Symlinks to contact users
├── groups/
│   └── mygroup/            # Group chat
│       ├── .info
│       ├── messages
│       ├── media/
│       └── files/
├── channels/
│   └── news/               # Channel
│       ├── .info
│       ├── messages
│       ├── media/
│       └── files/
├── @alice -> users/alice   # Symlink for quick access (contacts only)
└── self -> users/me        # Current user
```

### FUSE Operation Mapping

**readdir("/users/"):**
```cpp
users = co_await client.get_users()
for user in users:
    add_entry(user.get_identifier())
```

**readdir("/users/alice/"):**
```cpp
add_entry(".info")
add_entry("messages")
add_entry("media")
add_entry("files")
```

**read("/users/alice/messages"):**
```cpp
messages = co_await client.get_last_n_messages(chat_id, N)
return format_as_text(messages)  // Compatible with tail -n
```

**write("/users/alice/messages"):**
```cpp
co_await client.send_text(chat_id, content)
```

**readdir("/users/alice/media/"):**
```cpp
items = co_await client.list_media(chat_id)
for item in items:
    add_entry(item.filename)  // YYYYMMDD-HHMM-original.ext
```

**write("/users/alice/media/photo.jpg"):**
```cpp
co_await client.send_file(chat_id, source_path, SendMode::MEDIA)
```

**write("/users/alice/files/document.pdf"):**
```cpp
co_await client.send_file(chat_id, source_path, SendMode::DOCUMENT)
```

### Per-Directory Behaviour

- **AUTO mode** (copying to chat directory): Detects file type by MIME and extension
  - Images (.jpg, .png, .gif, .webp) → MEDIA (compressed)
  - Videos (.mp4, .mov, .mkv) → MEDIA (compressed)
  - Text files (.txt, .md) → Sent as text message
  - Other → DOCUMENT (original)

- **/media directory**: Forces `SendMode::MEDIA`
  - All files sent as photos/videos
  - Telegram compression applied

- **/files directory**: Forces `SendMode::DOCUMENT`
  - All files sent as documents
  - Original quality preserved

### Upload Deduplication

When sending a file:
1. Compute SHA256 hash of file content
2. Check `upload_cache` for existing remote file ID
3. If found: use `send_file_by_id()` with cached ID (avoids re-uploading)
4. If not found: upload via `send_file()`, cache result on success

## Usage Examples

### Basic Usage

```cpp
// Configure and start
tg::TelegramClient::Config config;
config.api_id = 12345;
config.api_hash = "hash";
config.database_directory = "/tmp/tg";

tg::TelegramClient client(config);
co_await client.start();

// Authenticate
co_await client.login("+1234567890");
co_await client.submit_code("12345");

// Get current user
auto me = co_await client.get_me();

// List chats
auto chats = co_await client.get_all_chats();

// Send message
auto chat = co_await client.resolve_username("alice");
co_await client.send_text(chat->id, "Hello!");

// Read messages
auto msgs = co_await client.get_last_n_messages(chat->id, 10);

// Send file
co_await client.send_file(chat->id, "/tmp/photo.jpg", tg::SendMode::AUTO);

// Send file with deduplication
auto msg = co_await client.send_file(chat->id, "/tmp/doc.pdf",
                                      tg::SendMode::DOCUMENT, file_hash, file_size);
```

### Error Handling

```cpp
try {
    auto chat = co_await client.resolve_username("invalid");
} catch (const tg::ChatNotFoundException& e) {
    spdlog::error("Chat not found: {}", e.what());
} catch (const tg::TelegramException& e) {
    spdlog::error("Error: {}", e.what());
}
```

### Cache Usage

```cpp
auto& cache = client.cache();

// Get cached data
auto user = cache.get_cached_user_by_username("alice");
auto chats = cache.get_all_cached_chats();
auto messages = cache.get_last_n_messages(chat_id, 50);

// Upload deduplication
auto remote_id = cache.get_cached_upload(file_hash);
cache.cache_upload(file_hash, file_size, remote_file_id);

// Invalidate
cache.invalidate_chat(chat_id);
cache.clear_all();
```

## Implementation Status

### ✅ Completed

1. **Core Infrastructure**
   - Data structures with helper methods
   - Exception hierarchy
   - Coroutine Task<T> and TdPromise<T>
   - SQLite cache with full schema
   - Build system integration

2. **TelegramClient**
   - Configuration and initialisation
   - TDLib client lifecycle management
   - Update processing thread
   - Callback registration system
   - Authorization state machine

3. **Authentication**
   - Phone number submission
   - Code verification
   - 2FA password
   - State tracking and synchronisation

4. **Entity Operations**
   - `get_users()` - Users from private chats
   - `get_groups()` - Groups and supergroups
   - `get_channels()` - Channels
   - `get_all_chats()` - All chats
   - `resolve_username()` - Username lookup
   - `get_chat()` / `get_user()` - ID lookup
   - `get_me()` - Current user

5. **Messaging**
   - `send_text()` - Send text messages
   - `get_messages()` - Fetch message history
   - `get_last_n_messages()` - Last N messages
   - `get_messages_until()` - Fetch until age threshold
   - Message formatting for display

6. **File Operations**
   - `send_file()` - Upload and send files
   - `send_file_by_id()` - Send cached files
   - `list_media()` - List photos/videos
   - `list_files()` - List documents
   - `download_file()` - Download files
   - Upload deduplication via hash caching

7. **FUSE Integration**
   - Directory structure (/users/, /groups/, /channels/, /contacts/)
   - Read/write messages
   - File uploads via copy
   - Symlinks for quick access
   - Per-directory send modes

8. **Documentation**
   - Comprehensive usage guide
   - Design documentation
   - Code examples for all features
   - VFS integration patterns

## Files Structure

```
tg-interface/
├── include/tg/
│   ├── types.hpp          # Data structures and enumerations
│   ├── exceptions.hpp     # Exception hierarchy
│   ├── async.hpp          # Coroutine infrastructure
│   ├── cache.hpp          # SQLite cache manager
│   └── client.hpp         # Main TelegramClient API
├── include/fuse/
│   ├── data_provider.hpp  # Abstract VFS provider interface
│   ├── telegram_provider.hpp  # Telegram VFS implementation
│   ├── operations.hpp     # FUSE operations wrapper
│   └── platform.hpp       # Platform abstraction
├── src/tg/
│   ├── types.cpp          # Data structure implementations
│   ├── cache.cpp          # Cache manager implementation
│   └── client.cpp         # TelegramClient implementation
├── src/fuse/
│   ├── telegram_provider.cpp  # VFS implementation
│   └── operations.cpp     # FUSE callbacks
├── src/ctl/              # CLI companion app
├── src/fused/            # FUSE daemon
├── CMakeLists.txt        # Root build configuration
├── src/CMakeLists.txt    # Library targets
├── TG_WRAPPER_README.md  # Quick reference
├── TG_WRAPPER_USAGE.md   # Usage guide and examples
└── TG_WRAPPER_DESIGN.md  # This file
```

## Design Decisions

### Why Coroutines?

- **Clean async code**: Sequential style for async operations
- **TDLib integration**: Natural bridge from callbacks to coroutines
- **C++20 standard**: Modern, standardised approach
- **Composability**: Easy to chain async operations

### Why SQLite Cache?

- **Persistence**: Survives application restarts
- **Performance**: Fast indexed queries
- **Reliability**: ACID guarantees
- **Simplicity**: Single-file database
- **Thread-safe**: Built-in concurrency support

### Why Exception-Based Error Handling?

- **FUSE compatibility**: Easy conversion to errno codes
- **Clarity**: Explicit error types
- **Stack unwinding**: Automatic cleanup
- **Hierarchical**: Catch at appropriate level

### Why Per-Directory Behaviour?

- **User control**: Explicit media vs document choice
- **Intuitiveness**: Directory name indicates behaviour
- **Flexibility**: AUTO mode for convenience
- **VFS pattern**: Matches filesystem metaphor

### Why Upload Deduplication?

- **Efficiency**: Avoids re-uploading identical files to Telegram servers
- **Multi-chat sends**: Same file can be sent to multiple chats quickly
- **Hash-based**: SHA256 ensures content identity
- **Async-friendly**: Cache populated on upload success

## Conclusion

The TG wrapper provides a complete foundation for integrating Telegram into a FUSE filesystem:

- ✅ **Complete infrastructure** with full TDLib integration
- ✅ **Modern C++20** with coroutines and strong typing
- ✅ **Persistent caching** for offline capabilities
- ✅ **Clean API** designed for VFS operations
- ✅ **Upload deduplication** for efficient file sending
- ✅ **Full FUSE integration** with read/write support
