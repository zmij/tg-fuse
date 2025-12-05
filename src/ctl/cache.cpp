#include "cache.hpp"
#include "config.hpp"

#include "tg/cache.hpp"
#include "tg/types.hpp"

#include <filesystem>
#include <iostream>

namespace tgfuse::ctl {

namespace {

/// Get the path to the cache database
std::filesystem::path get_cache_db_path() { return get_data_dir() / "tdlib" / "cache.db"; }

/// Find chat ID by entity name (username or display name) using cached data
/// Returns 0 if not found
int64_t find_chat_id_from_cache(tg::CacheManager& cache, const std::string& entity_name) {
    // Strip leading @ if present
    std::string name = entity_name;
    if (!name.empty() && name[0] == '@') {
        name = name.substr(1);
    }

    // First, try to find as a user by username
    auto user = cache.get_cached_user_by_username(name);
    if (user) {
        return user->id;
    }

    // Search all users by display name
    auto users = cache.get_all_cached_users();
    for (const auto& u : users) {
        if (u.username == name || u.display_name() == entity_name) {
            return u.id;
        }
    }

    // Try to find as a chat (group/channel) by username
    auto chat = cache.get_cached_chat_by_username(name);
    if (chat) {
        return chat->id;
    }

    // Search all chats by title
    auto chats = cache.get_all_cached_chats();
    for (const auto& c : chats) {
        if (c.username == name || c.title == entity_name) {
            return c.id;
        }
    }

    return 0;
}

}  // namespace

int exec_cache_clear_files(const std::string& entity_name) {
    auto config = load_config();
    if (!config) {
        std::cerr << "Error: Not configured. Run 'tg-fuse login' first.\n";
        return 1;
    }

    auto db_path = get_cache_db_path();

    if (!std::filesystem::exists(db_path)) {
        std::cout << "No cache database found.\n";
        return 0;
    }

    try {
        tg::CacheManager cache(db_path.string());

        int64_t chat_id = find_chat_id_from_cache(cache, entity_name);
        if (chat_id == 0) {
            std::cerr << "Error: Entity '" << entity_name << "' not found in cache.\n"
                      << "Hint: The entity must have been accessed at least once while the filesystem was mounted.\n";
            return 1;
        }

        cache.invalidate_chat_files(chat_id);
        std::cout << "File cache cleared for '" << entity_name << "' (chat_id: " << chat_id << ")\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int exec_cache_clear_all_files() {
    auto config = load_config();
    if (!config) {
        std::cerr << "Error: Not configured. Run 'tg-fuse login' first.\n";
        return 1;
    }

    auto db_path = get_cache_db_path();

    if (!std::filesystem::exists(db_path)) {
        std::cout << "No cache database found.\n";
        return 0;
    }

    try {
        tg::CacheManager cache(db_path.string());

        // Get all chats and clear their file caches
        auto chats = cache.get_all_cached_chats();
        auto users = cache.get_all_cached_users();

        int cleared = 0;
        for (const auto& chat : chats) {
            cache.invalidate_chat_files(chat.id);
            cleared++;
        }
        for (const auto& user : users) {
            cache.invalidate_chat_files(user.id);
            cleared++;
        }

        std::cout << "Cleared file cache for " << cleared << " chats.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int exec_cache_clear_all() {
    auto config = load_config();
    if (!config) {
        std::cerr << "Error: Not configured. Run 'tg-fuse login' first.\n";
        return 1;
    }

    auto db_path = get_cache_db_path();

    if (!std::filesystem::exists(db_path)) {
        std::cout << "No cache database found.\n";
        return 0;
    }

    try {
        tg::CacheManager cache(db_path.string());
        cache.clear_all();

        std::cout << "All caches cleared.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

int exec_cache_stats() {
    auto config = load_config();
    if (!config) {
        std::cerr << "Error: Not configured. Run 'tg-fuse login' first.\n";
        return 1;
    }

    auto db_path = get_cache_db_path();

    if (!std::filesystem::exists(db_path)) {
        std::cout << "No cache database found.\n";
        return 0;
    }

    try {
        tg::CacheManager cache(db_path.string());

        auto users = cache.get_all_cached_users();
        auto chats = cache.get_all_cached_chats();
        auto stats = cache.get_all_chat_message_stats();

        std::cout << "Cache statistics:\n";
        std::cout << "  Cached users: " << users.size() << "\n";
        std::cout << "  Cached chats: " << chats.size() << "\n";
        std::cout << "  Chats with message stats: " << stats.size() << "\n";

        std::size_t total_messages = 0;
        std::size_t total_content_size = 0;
        for (const auto& s : stats) {
            total_messages += s.message_count;
            total_content_size += s.content_size;
        }
        std::cout << "  Total cached messages: " << total_messages << "\n";
        std::cout << "  Total content size: " << (total_content_size / 1024) << " KB\n";

        // Show database file size
        auto file_size = std::filesystem::file_size(db_path);
        std::cout << "  Database file size: " << (file_size / 1024) << " KB\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace tgfuse::ctl
