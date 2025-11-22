#include "tg/cache.hpp"

#include "tg/exceptions.hpp"

#include <sqlite3.h>
#include <spdlog/spdlog.h>

namespace tg {

namespace {

// Helper to execute SQL with error handling
void exec_sql(sqlite3* db, const char* sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw DatabaseException("Failed to execute SQL: " + error);
    }
}

// Helper to convert ChatType enum to int
int chat_type_to_int(ChatType type) {
    return static_cast<int>(type);
}

// Helper to convert int to ChatType
ChatType int_to_chat_type(int value) {
    return static_cast<ChatType>(value);
}

// Helper to convert MediaType enum to int
int media_type_to_int(MediaType type) {
    return static_cast<int>(type);
}

// Helper to convert int to MediaType
MediaType int_to_media_type(int value) {
    return static_cast<MediaType>(value);
}

}  // namespace

CacheManager::CacheManager(const std::string& db_path) : db_(nullptr) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw DatabaseException("Failed to open database: " + std::string(sqlite3_errmsg(db_)));
    }

    spdlog::info("Opened cache database: {}", db_path);
    init_database();
}

CacheManager::~CacheManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void CacheManager::init_database() {
    // Enable WAL mode for better concurrency
    exec_sql(db_, "PRAGMA journal_mode=WAL;");
    exec_sql(db_, "PRAGMA synchronous=NORMAL;");
    exec_sql(db_, "PRAGMA foreign_keys=ON;");

    create_tables();
}

void CacheManager::create_tables() {
    const char* schema = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY,
            username TEXT,
            first_name TEXT,
            last_name TEXT,
            phone_number TEXT,
            is_contact INTEGER,
            last_message_id INTEGER,
            last_message_timestamp INTEGER,
            updated_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);

        CREATE TABLE IF NOT EXISTS chats (
            id INTEGER PRIMARY KEY,
            type INTEGER NOT NULL,
            title TEXT,
            username TEXT,
            last_message_id INTEGER,
            last_message_timestamp INTEGER,
            updated_at INTEGER DEFAULT (strftime('%s', 'now'))
        );

        CREATE INDEX IF NOT EXISTS idx_chats_username ON chats(username);
        CREATE INDEX IF NOT EXISTS idx_chats_type ON chats(type);

        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER NOT NULL,
            chat_id INTEGER NOT NULL,
            sender_id INTEGER,
            timestamp INTEGER NOT NULL,
            text TEXT,
            is_outgoing INTEGER,
            media_type INTEGER,
            media_file_id TEXT,
            media_filename TEXT,
            media_mime_type TEXT,
            media_file_size INTEGER,
            media_local_path TEXT,
            media_width INTEGER,
            media_height INTEGER,
            media_duration INTEGER,
            PRIMARY KEY (chat_id, id)
        );

        CREATE INDEX IF NOT EXISTS idx_messages_chat_timestamp ON messages(chat_id, timestamp DESC);
        CREATE INDEX IF NOT EXISTS idx_messages_media ON messages(chat_id, media_type) WHERE media_type IS NOT NULL;

        CREATE TABLE IF NOT EXISTS files (
            message_id INTEGER NOT NULL,
            chat_id INTEGER NOT NULL,
            filename TEXT NOT NULL,
            file_size INTEGER,
            timestamp INTEGER,
            type INTEGER,
            file_id TEXT,
            PRIMARY KEY (chat_id, message_id)
        );

        CREATE INDEX IF NOT EXISTS idx_files_chat_type ON files(chat_id, type);
        CREATE INDEX IF NOT EXISTS idx_files_timestamp ON files(chat_id, timestamp DESC);
    )";

    exec_sql(db_, schema);
    spdlog::debug("Cache database schema initialised");
}

void CacheManager::cache_user(const User& user) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO users
        (id, username, first_name, last_name, phone_number, is_contact,
         last_message_id, last_message_timestamp, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, strftime('%s', 'now'))
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, user.id);
    sqlite3_bind_text(stmt, 2, user.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, user.first_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, user.last_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, user.phone_number.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, user.is_contact ? 1 : 0);
    sqlite3_bind_int64(stmt, 7, user.last_message_id);
    sqlite3_bind_int64(stmt, 8, user.last_message_timestamp);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to cache user");
    }

    sqlite3_finalize(stmt);
}

std::optional<User> CacheManager::get_cached_user(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM users WHERE id = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        User user;
        user.id = sqlite3_column_int64(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.first_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.last_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.phone_number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user.is_contact = sqlite3_column_int(stmt, 5) != 0;
        user.last_message_id = sqlite3_column_int64(stmt, 6);
        user.last_message_timestamp = sqlite3_column_int64(stmt, 7);

        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<User> CacheManager::get_cached_user_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM users WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        User user;
        user.id = sqlite3_column_int64(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.first_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.last_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.phone_number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user.is_contact = sqlite3_column_int(stmt, 5) != 0;
        user.last_message_id = sqlite3_column_int64(stmt, 6);
        user.last_message_timestamp = sqlite3_column_int64(stmt, 7);

        sqlite3_finalize(stmt);
        return user;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<User> CacheManager::get_all_cached_users() {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM users ORDER BY username";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    std::vector<User> users;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        User user;
        user.id = sqlite3_column_int64(stmt, 0);
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        user.first_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        user.last_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        user.phone_number = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        user.is_contact = sqlite3_column_int(stmt, 5) != 0;
        user.last_message_id = sqlite3_column_int64(stmt, 6);
        user.last_message_timestamp = sqlite3_column_int64(stmt, 7);

        users.push_back(std::move(user));
    }

    sqlite3_finalize(stmt);
    return users;
}

void CacheManager::cache_chat(const Chat& chat) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO chats
        (id, type, title, username, last_message_id, last_message_timestamp, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, strftime('%s', 'now'))
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat.id);
    sqlite3_bind_int(stmt, 2, chat_type_to_int(chat.type));
    sqlite3_bind_text(stmt, 3, chat.title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, chat.username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, chat.last_message_id);
    sqlite3_bind_int64(stmt, 6, chat.last_message_timestamp);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to cache chat");
    }

    sqlite3_finalize(stmt);
}

std::optional<Chat> CacheManager::get_cached_chat(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM chats WHERE id = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Chat chat;
        chat.id = sqlite3_column_int64(stmt, 0);
        chat.type = int_to_chat_type(sqlite3_column_int(stmt, 1));
        chat.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        chat.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        chat.last_message_id = sqlite3_column_int64(stmt, 4);
        chat.last_message_timestamp = sqlite3_column_int64(stmt, 5);

        sqlite3_finalize(stmt);
        return chat;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::optional<Chat> CacheManager::get_cached_chat_by_username(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM chats WHERE username = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Chat chat;
        chat.id = sqlite3_column_int64(stmt, 0);
        chat.type = int_to_chat_type(sqlite3_column_int(stmt, 1));
        chat.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        chat.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        chat.last_message_id = sqlite3_column_int64(stmt, 4);
        chat.last_message_timestamp = sqlite3_column_int64(stmt, 5);

        sqlite3_finalize(stmt);
        return chat;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<Chat> CacheManager::get_all_cached_chats() {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM chats ORDER BY last_message_timestamp DESC";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    std::vector<Chat> chats;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Chat chat;
        chat.id = sqlite3_column_int64(stmt, 0);
        chat.type = int_to_chat_type(sqlite3_column_int(stmt, 1));
        chat.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        chat.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        chat.last_message_id = sqlite3_column_int64(stmt, 4);
        chat.last_message_timestamp = sqlite3_column_int64(stmt, 5);

        chats.push_back(std::move(chat));
    }

    sqlite3_finalize(stmt);
    return chats;
}

std::vector<Chat> CacheManager::get_cached_chats_by_type(ChatType type) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM chats WHERE type = ? ORDER BY last_message_timestamp DESC";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int(stmt, 1, chat_type_to_int(type));

    std::vector<Chat> chats;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Chat chat;
        chat.id = sqlite3_column_int64(stmt, 0);
        chat.type = int_to_chat_type(sqlite3_column_int(stmt, 1));
        chat.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        chat.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        chat.last_message_id = sqlite3_column_int64(stmt, 4);
        chat.last_message_timestamp = sqlite3_column_int64(stmt, 5);

        chats.push_back(std::move(chat));
    }

    sqlite3_finalize(stmt);
    return chats;
}

void CacheManager::cache_message(const Message& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO messages
        (id, chat_id, sender_id, timestamp, text, is_outgoing,
         media_type, media_file_id, media_filename, media_mime_type, media_file_size,
         media_local_path, media_width, media_height, media_duration)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, msg.id);
    sqlite3_bind_int64(stmt, 2, msg.chat_id);
    sqlite3_bind_int64(stmt, 3, msg.sender_id);
    sqlite3_bind_int64(stmt, 4, msg.timestamp);
    sqlite3_bind_text(stmt, 5, msg.text.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, msg.is_outgoing ? 1 : 0);

    if (msg.media) {
        sqlite3_bind_int(stmt, 7, media_type_to_int(msg.media->type));
        sqlite3_bind_text(stmt, 8, msg.media->file_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, msg.media->filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 10, msg.media->mime_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 11, msg.media->file_size);

        if (msg.media->local_path) {
            sqlite3_bind_text(stmt, 12, msg.media->local_path->c_str(), -1, SQLITE_TRANSIENT);
        } else {
            sqlite3_bind_null(stmt, 12);
        }

        if (msg.media->width) {
            sqlite3_bind_int(stmt, 13, *msg.media->width);
        } else {
            sqlite3_bind_null(stmt, 13);
        }

        if (msg.media->height) {
            sqlite3_bind_int(stmt, 14, *msg.media->height);
        } else {
            sqlite3_bind_null(stmt, 14);
        }

        if (msg.media->duration) {
            sqlite3_bind_int(stmt, 15, *msg.media->duration);
        } else {
            sqlite3_bind_null(stmt, 15);
        }
    } else {
        for (int i = 7; i <= 15; ++i) {
            sqlite3_bind_null(stmt, i);
        }
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to cache message");
    }

    sqlite3_finalize(stmt);
}

void CacheManager::cache_messages(const std::vector<Message>& messages) {
    for (const auto& msg : messages) {
        cache_message(msg);
    }
}

std::optional<Message> CacheManager::get_cached_message(int64_t chat_id, int64_t message_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM messages WHERE chat_id = ? AND id = ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_bind_int64(stmt, 2, message_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.chat_id = sqlite3_column_int64(stmt, 1);
        msg.sender_id = sqlite3_column_int64(stmt, 2);
        msg.timestamp = sqlite3_column_int64(stmt, 3);
        msg.text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.is_outgoing = sqlite3_column_int(stmt, 5) != 0;

        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            MediaInfo media;
            media.type = int_to_media_type(sqlite3_column_int(stmt, 6));
            media.file_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            media.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            media.mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            media.file_size = sqlite3_column_int64(stmt, 10);

            if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
                media.local_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
            }
            if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
                media.width = sqlite3_column_int(stmt, 12);
            }
            if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
                media.height = sqlite3_column_int(stmt, 13);
            }
            if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
                media.duration = sqlite3_column_int(stmt, 14);
            }

            msg.media = std::move(media);
        }

        sqlite3_finalize(stmt);
        return msg;
    }

    sqlite3_finalize(stmt);
    return std::nullopt;
}

std::vector<Message> CacheManager::get_cached_messages(int64_t chat_id, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT * FROM messages WHERE chat_id = ? ORDER BY timestamp DESC LIMIT ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_bind_int(stmt, 2, limit);

    std::vector<Message> messages;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg.id = sqlite3_column_int64(stmt, 0);
        msg.chat_id = sqlite3_column_int64(stmt, 1);
        msg.sender_id = sqlite3_column_int64(stmt, 2);
        msg.timestamp = sqlite3_column_int64(stmt, 3);
        msg.text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        msg.is_outgoing = sqlite3_column_int(stmt, 5) != 0;

        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            MediaInfo media;
            media.type = int_to_media_type(sqlite3_column_int(stmt, 6));
            media.file_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            media.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
            media.mime_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 9));
            media.file_size = sqlite3_column_int64(stmt, 10);

            if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) {
                media.local_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
            }
            if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
                media.width = sqlite3_column_int(stmt, 12);
            }
            if (sqlite3_column_type(stmt, 13) != SQLITE_NULL) {
                media.height = sqlite3_column_int(stmt, 13);
            }
            if (sqlite3_column_type(stmt, 14) != SQLITE_NULL) {
                media.duration = sqlite3_column_int(stmt, 14);
            }

            msg.media = std::move(media);
        }

        messages.push_back(std::move(msg));
    }

    sqlite3_finalize(stmt);
    return messages;
}

std::vector<Message> CacheManager::get_last_n_messages(int64_t chat_id, int n) {
    return get_cached_messages(chat_id, n);
}

void CacheManager::update_chat_status(int64_t chat_id, int64_t last_message_id, int64_t last_message_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        UPDATE chats
        SET last_message_id = ?, last_message_timestamp = ?, updated_at = strftime('%s', 'now')
        WHERE id = ?
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, last_message_id);
    sqlite3_bind_int64(stmt, 2, last_message_timestamp);
    sqlite3_bind_int64(stmt, 3, chat_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to update chat status");
    }

    sqlite3_finalize(stmt);
}

void CacheManager::cache_file_item(int64_t chat_id, const FileListItem& item) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = R"(
        INSERT OR REPLACE INTO files
        (message_id, chat_id, filename, file_size, timestamp, type, file_id)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, item.message_id);
    sqlite3_bind_int64(stmt, 2, chat_id);
    sqlite3_bind_text(stmt, 3, item.filename.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, item.file_size);
    sqlite3_bind_int64(stmt, 5, item.timestamp);
    sqlite3_bind_int(stmt, 6, media_type_to_int(item.type));
    sqlite3_bind_text(stmt, 7, item.file_id.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw DatabaseException("Failed to cache file item");
    }

    sqlite3_finalize(stmt);
}

void CacheManager::cache_file_list(int64_t chat_id, const std::vector<FileListItem>& files) {
    for (const auto& item : files) {
        cache_file_item(chat_id, item);
    }
}

std::vector<FileListItem> CacheManager::get_cached_file_list(int64_t chat_id, std::optional<MediaType> type) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string sql = "SELECT * FROM files WHERE chat_id = ?";
    if (type) {
        sql += " AND type = ?";
    }
    sql += " ORDER BY timestamp DESC";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat_id);
    if (type) {
        sqlite3_bind_int(stmt, 2, media_type_to_int(*type));
    }

    std::vector<FileListItem> items;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileListItem item;
        item.message_id = sqlite3_column_int64(stmt, 0);
        item.filename = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        item.file_size = sqlite3_column_int64(stmt, 3);
        item.timestamp = sqlite3_column_int64(stmt, 4);
        item.type = int_to_media_type(sqlite3_column_int(stmt, 5));
        item.file_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        items.push_back(std::move(item));
    }

    sqlite3_finalize(stmt);
    return items;
}

void CacheManager::invalidate_chat_messages(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM messages WHERE chat_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void CacheManager::invalidate_chat_files(int64_t chat_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM files WHERE chat_id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void CacheManager::invalidate_chat(int64_t chat_id) {
    invalidate_chat_messages(chat_id);
    invalidate_chat_files(chat_id);

    std::lock_guard<std::mutex> lock(mutex_);
    const char* sql = "DELETE FROM chats WHERE id = ?";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, chat_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void CacheManager::clear_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    exec_sql(db_, "DELETE FROM users");
    exec_sql(db_, "DELETE FROM chats");
    exec_sql(db_, "DELETE FROM messages");
    exec_sql(db_, "DELETE FROM files");
}

void CacheManager::vacuum() {
    std::lock_guard<std::mutex> lock(mutex_);
    exec_sql(db_, "VACUUM");
}

void CacheManager::cleanup_old_messages(int64_t older_than_timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "DELETE FROM messages WHERE timestamp < ?";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw DatabaseException("Failed to prepare statement");
    }

    sqlite3_bind_int64(stmt, 1, older_than_timestamp);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

}  // namespace tg
