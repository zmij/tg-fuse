#pragma once

#include "fuse/messages_cache.hpp"
#include "tg/cache.hpp"
#include "tg/client.hpp"
#include "tg/rate_limiter.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>

namespace tgfuse {

/// Priority for prefetch queue
enum class PrefetchPriority { HIGH = 0, NORMAL = 1, LOW = 2 };

/// Configuration for BackgroundPrefetcher
struct BackgroundPrefetcherConfig {
    std::chrono::milliseconds rate_limit_interval{500};  // Min time between API requests
    std::chrono::seconds prefetch_interval{300};         // Check for new chats every 5 min
    std::chrono::seconds max_history_age{172800};        // 48h max message age
    std::size_t min_messages{10};                        // Min messages to fetch per chat
    bool exclude_archived{true};                         // Skip archived chats
};

/// Background worker that prefetches messages for chats
///
/// Respects Telegram API rate limits to avoid flood bans.
/// Prioritises fetching: contacts → other users → groups → channels
/// Stores fetched messages in SQLite, updates chat stats.
class BackgroundPrefetcher {
public:
    using Priority = PrefetchPriority;
    using Config = BackgroundPrefetcherConfig;

    BackgroundPrefetcher(
        tg::TelegramClient& client,
        FormattedMessagesCache& cache,
        tg::CacheManager& db_cache,
        Config config = {}
    );

    ~BackgroundPrefetcher();

    // Disable copy
    BackgroundPrefetcher(const BackgroundPrefetcher&) = delete;
    BackgroundPrefetcher& operator=(const BackgroundPrefetcher&) = delete;

    /// Start background prefetch thread
    void start();

    /// Stop and join thread
    void stop();

    /// Queue a specific chat for priority fetch
    /// @param chat_id The chat to fetch
    /// @param priority Fetch priority (HIGH for on-demand, NORMAL for background)
    void queue_chat(int64_t chat_id, Priority priority = Priority::NORMAL);

    /// Check if prefetcher is running
    [[nodiscard]] bool is_running() const { return running_.load(); }

    /// Set callback for user resolution (needed for formatting)
    using UserResolver = std::function<const tg::User&(int64_t)>;
    using ChatResolver = std::function<const tg::Chat&(int64_t)>;

    void set_resolvers(UserResolver user_resolver, ChatResolver chat_resolver);

private:
    /// Main prefetch loop (runs in background thread)
    void prefetch_loop();

    /// Fetch messages for a single chat
    void fetch_chat_messages(int64_t chat_id);

    /// Get ordered list of chats to fetch
    /// Order: contacts → users → groups → channels, each sorted by last_message_time DESC
    std::vector<int64_t> get_chats_to_fetch();

    /// Check if a chat needs fetching
    bool needs_fetch(int64_t chat_id);

    tg::TelegramClient& client_;
    FormattedMessagesCache& cache_;
    tg::CacheManager& db_cache_;
    Config config_;
    tg::RateLimiter rate_limiter_;

    UserResolver user_resolver_;
    ChatResolver chat_resolver_;

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::condition_variable cv_;
    std::mutex mutex_;

    // Priority queue: (priority, -last_message_time, chat_id)
    // Using negative time so higher (more recent) times come first
    using QueueEntry = std::tuple<Priority, int64_t, int64_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<>> queue_;
};

}  // namespace tgfuse
