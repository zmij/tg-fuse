# TG Wrapper Design & Implementation Summary

## Overview

A modern C++20 wrapper around TDLib (Telegram Database Library) providing:
- Coroutine-based asynchronous API
- Persistent SQLite caching
- Type-safe data structures
- Exception-based error handling
- Per-directory file type behaviour for VFS integration

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
│   │  - send_file()                  │   │
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

**Enumerations:**
- `ChatType` - PRIVATE, GROUP, SUPERGROUP, CHANNEL
- `MediaType` - PHOTO, VIDEO, DOCUMENT, AUDIO, VOICE, ANIMATION, STICKER, VIDEO_NOTE
- `SendMode` - AUTO, MEDIA, DOCUMENT
- `AuthState` - WAIT_PHONE, WAIT_CODE, WAIT_PASSWORD, READY

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
       is_contact, last_message_id, last_message_timestamp, updated_at)

-- Chats table
chats (id, type, title, username, last_message_id,
       last_message_timestamp, updated_at)

-- Messages table
messages (id, chat_id, sender_id, timestamp, text, is_outgoing,
          media_type, media_file_id, media_filename, media_mime_type,
          media_file_size, media_local_path, media_width,
          media_height, media_duration)

-- Files table
files (message_id, chat_id, filename, file_size, timestamp,
       type, file_id)
```

**Indices:**
- `idx_users_username` - Fast username lookup
- `idx_chats_username` - Fast chat username lookup
- `idx_chats_type` - Filter by chat type
- `idx_messages_chat_timestamp` - Chronological message retrieval
- `idx_messages_media` - Media-only message filtering
- `idx_files_chat_type` - File type filtering
- `idx_files_timestamp` - Chronological file listing

**Features:**
- WAL mode for better concurrency
- Foreign key enforcement
- Thread-safe with mutex protection
- Automatic timestamp tracking
- Cache invalidation per chat
- Vacuum and cleanup utilities

**API:**
- User/Chat caching and retrieval
- Message bulk caching
- File metadata caching
- Username-based lookups
- Type-filtered queries
- Selective invalidation

### 5. Telegram Client (`include/tg/client.hpp`, `src/tg/client.cpp`)

**Configuration:**
```cpp
struct Config {
    int32_t api_id;
    std::string api_hash;
    std::string database_directory;
    std::string files_directory;
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
- `get_users()` - All user contacts
- `get_groups()` - All groups/supergroups
- `get_channels()` - All channels
- `get_all_chats()` - All chats combined

*Entity Lookup:*
- `resolve_username(username)` - Find by @username or #groupname
- `get_chat(chat_id)` - Get chat by ID
- `get_user(user_id)` - Get user by ID

*Messaging:*
- `send_text(chat_id, text)` - Send text message
- `get_messages(chat_id, limit)` - Get recent messages
- `get_last_n_messages(chat_id, n)` - Get last N messages

*File Operations:*
- `send_file(chat_id, path, mode)` - Upload and send file
- `list_media(chat_id)` - List photos/videos/animations
- `list_files(chat_id)` - List documents/audio
- `download_file(file_id, dest)` - Download file

*Status:*
- `get_chat_status(chat_id)` - Get last message info

*Cache Access:*
- `cache()` - Direct access to CacheManager

**Update Processing:**
- Background thread continuously polls TDLib
- Updates dispatched to handlers
- Authorization state updates tracked
- New chat/message updates logged
- Callback-based query responses

**Current Status:**
- ✅ Infrastructure complete
- ✅ Authentication flow implemented
- ⏳ Entity operations are stubs (need TDLib integration)
- ⏳ Messaging operations are stubs
- ⏳ File operations are stubs

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
/dev/tg/
├── @alice/              # Private chat with user "alice"
│   ├── messages         # Read/write text messages
│   ├── media/           # Photos, videos, animations
│   └── files/           # Documents, audio files
├── #mygroup/            # Group chat
│   ├── messages
│   ├── media/
│   └── files/
├── -1001234567890/      # Private channel/group (by ID)
│   ├── messages
│   ├── media/
│   └── files/
└── .meta/               # Control interface (future)
```

### FUSE Operation Mapping

**readdir("/"):**
```cpp
chats = co_await client.get_all_chats()
for chat in chats:
    add_entry(chat.get_directory_name())
```

**readdir("/@alice/"):**
```cpp
add_entry("messages")
add_entry("media")
add_entry("files")
```

**read("/@alice/messages"):**
```cpp
messages = co_await client.get_last_n_messages(chat_id, N)
return format_as_text(messages)  // Compatible with tail -n
```

**write("/@alice/messages"):**
```cpp
co_await client.send_text(chat_id, content)
```

**readdir("/@alice/media/"):**
```cpp
items = co_await client.list_media(chat_id)
for item in items:
    add_entry(generate_filename(item))
```

**write("/@alice/media/photo.jpg"):**
```cpp
co_await client.send_file(chat_id, source_path, SendMode::MEDIA)
```

**write("/@alice/files/document.pdf"):**
```cpp
co_await client.send_file(chat_id, source_path, SendMode::DOCUMENT)
```

### Per-Directory Behaviour

- **AUTO mode**: Detects file type by MIME and extension
  - Images (.jpg, .png, .gif) → MEDIA (compressed)
  - Videos (.mp4, .mov) → MEDIA (compressed)
  - Other → DOCUMENT (original)

- **/media directory**: Forces `SendMode::MEDIA`
  - All files sent as photos/videos
  - Telegram compression applied

- **/files directory**: Forces `SendMode::DOCUMENT`
  - All files sent as documents
  - Original quality preserved

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

// List chats
auto chats = co_await client.get_all_chats();

// Send message
auto chat = co_await client.resolve_username("alice");
co_await client.send_text(chat->id, "Hello!");

// Read messages
auto msgs = co_await client.get_last_n_messages(chat->id, 10);

// Send file
co_await client.send_file(chat->id, "/tmp/photo.jpg", tg::SendMode::AUTO);
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

2. **TelegramClient Foundation**
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

4. **Documentation**
   - Comprehensive usage guide
   - Design documentation
   - Code examples for all features
   - VFS integration patterns

### ⏳ Pending (Stubs in Place)

The following methods have signatures and stub implementations but need full TDLib integration:

1. **Entity Operations**
   - `get_users()` - Needs `td_api::getContacts`
   - `get_groups()` - Needs `td_api::getChats` with filtering
   - `get_channels()` - Needs `td_api::getChats` with filtering
   - `get_all_chats()` - Needs `td_api::getChats`
   - `resolve_username()` - Needs `td_api::searchPublicChat`
   - `get_chat()` - Needs `td_api::getChat`
   - `get_user()` - Needs `td_api::getUser`

2. **Messaging**
   - `send_text()` - Needs `td_api::sendMessage` with `inputMessageText`
   - `get_messages()` - Needs `td_api::getChatHistory`

3. **File Operations**
   - `send_file()` - Needs `td_api::sendMessage` with `inputMessageDocument`/`inputMessagePhoto`
   - `list_media()` - Needs `td_api::searchChatMessages` with media filter
   - `list_files()` - Needs `td_api::searchChatMessages` with document filter
   - `download_file()` - Needs `td_api::downloadFile`

4. **Status**
   - `get_chat_status()` - Needs `td_api::getChat` and extract last message info

### 🔜 Future Enhancements

1. **Testing**
   - Unit tests for data structures
   - Cache manager tests
   - TDLib mock for client testing
   - Integration tests

2. **Optimisations**
   - Batch message caching
   - Incremental updates
   - Lazy file list loading
   - Cache TTL and expiry policies

3. **Features**
   - Message editing/deletion
   - Reactions and stickers
   - Voice/video messages
   - Chat folders
   - User presence
   - Typing indicators

## Files Structure

```
tg-interface/
├── include/tg/
│   ├── types.hpp          # Data structures and enumerations
│   ├── exceptions.hpp     # Exception hierarchy
│   ├── async.hpp          # Coroutine infrastructure
│   ├── cache.hpp          # SQLite cache manager
│   └── client.hpp         # Main TelegramClient API
├── src/tg/
│   ├── types.cpp          # Data structure implementations
│   ├── cache.cpp          # Cache manager implementation
│   └── client.cpp         # TelegramClient implementation
├── tests/tg/
│   └── (future test files)
├── CMakeLists.txt         # Root build configuration
├── src/CMakeLists.txt     # tglib + tg-fuse targets
├── TG_WRAPPER_USAGE.md    # Usage guide and examples
└── TG_WRAPPER_DESIGN.md   # This file
```

## Next Steps

### Phase 1: Complete Core Operations (Entity Listing)

Implement TDLib integration for:
1. `getContacts` → `get_users()`
2. `getChats` → `get_all_chats()` with filtering
3. `searchPublicChat` → `resolve_username()`
4. `getChat` / `getUser` → lookup methods
5. Cache population and synchronisation

### Phase 2: Messaging

1. `sendMessage` with `inputMessageText` → `send_text()`
2. `getChatHistory` → `get_messages()`
3. Message parsing and caching
4. Format for VFS display

### Phase 3: File Operations

1. `sendMessage` with media inputs → `send_file()`
2. `searchChatMessages` with filters → `list_media()` / `list_files()`
3. `downloadFile` → `download_file()`
4. File type detection and mode handling
5. Progress tracking for large files

### Phase 4: Testing & Integration

1. Write comprehensive unit tests
2. Create TDLib mock for testing
3. Integration tests with real Telegram
4. FUSE handler implementation
5. End-to-end VFS testing

### Phase 5: Polish & Optimisation

1. Error handling refinement
2. Cache performance optimisation
3. Batch operations
4. Memory management
5. Documentation updates

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

## Conclusion

The TG wrapper provides a solid foundation for integrating Telegram into a FUSE filesystem:

- ✅ **Complete infrastructure** ready for TDLib integration
- ✅ **Modern C++20** with coroutines and strong typing
- ✅ **Persistent caching** for offline capabilities
- ✅ **Clean API** designed for VFS operations
- ⏳ **Stub implementations** awaiting TDLib calls

The next phase involves replacing stubs with actual TDLib API calls, which is straightforward given the infrastructure in place.
