#include "fuse/background_prefetcher.hpp"

#include "tg/formatters.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>

#include <algorithm>

namespace tgfuse {

BackgroundPrefetcher::BackgroundPrefetcher(
    tg::TelegramClient& client,
    FormattedMessagesCache& cache,
    tg::CacheManager& db_cache,
    Config config
)
    : client_(client),
      cache_(cache),
      db_cache_(db_cache),
      config_(std::move(config)),
      rate_limiter_(
          tg::RateLimiter::Config{
              .max_requests_per_second = 2,
              .min_interval = config_.rate_limit_interval,
          }
      ) {}

BackgroundPrefetcher::~BackgroundPrefetcher() { stop(); }

void BackgroundPrefetcher::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }

    spdlog::info("BackgroundPrefetcher: starting");
    worker_ = std::thread([this]() { prefetch_loop(); });
}

void BackgroundPrefetcher::stop() {
    if (!running_.exchange(false)) {
        return;  // Already stopped
    }

    spdlog::info("BackgroundPrefetcher: stopping");
    cv_.notify_all();

    if (worker_.joinable()) {
        worker_.join();
    }
}

void BackgroundPrefetcher::queue_chat(int64_t chat_id, Priority priority) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.emplace(priority, -std::time(nullptr), chat_id);
    cv_.notify_one();
    spdlog::debug("BackgroundPrefetcher: queued chat {} with priority {}", chat_id, static_cast<int>(priority));
}

void BackgroundPrefetcher::set_resolvers(UserResolver user_resolver, ChatResolver chat_resolver) {
    std::lock_guard<std::mutex> lock(mutex_);
    user_resolver_ = std::move(user_resolver);
    chat_resolver_ = std::move(chat_resolver);
}

void BackgroundPrefetcher::prefetch_loop() {
    spdlog::debug("BackgroundPrefetcher: loop started");

    while (running_.load()) {
        int64_t chat_id = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            // Wait for work or timeout
            if (queue_.empty()) {
                cv_.wait_for(lock, config_.prefetch_interval, [this]() { return !running_.load() || !queue_.empty(); });

                if (!running_.load()) {
                    break;
                }

                // If still empty after timeout, scan for chats to prefetch
                if (queue_.empty()) {
                    auto chats = get_chats_to_fetch();
                    for (auto id : chats) {
                        queue_.emplace(Priority::LOW, -std::time(nullptr), id);
                    }
                    spdlog::debug("BackgroundPrefetcher: queued {} chats for prefetch", chats.size());
                }
            }

            if (!queue_.empty()) {
                auto [priority, neg_time, id] = queue_.top();
                queue_.pop();
                chat_id = id;
            }
        }

        if (chat_id != 0 && needs_fetch(chat_id)) {
            rate_limiter_.acquire();
            fetch_chat_messages(chat_id);
        }
    }

    spdlog::debug("BackgroundPrefetcher: loop stopped");
}

void BackgroundPrefetcher::fetch_chat_messages(int64_t chat_id) {
    spdlog::debug("BackgroundPrefetcher: fetching messages for chat {}", chat_id);

    try {
        // Fetch messages from Telegram API
        auto max_age = std::chrono::duration_cast<std::chrono::seconds>(config_.max_history_age);
        auto task = client_.get_messages_until(chat_id, config_.min_messages, max_age);
        auto messages = task.get_result();

        if (messages.empty()) {
            spdlog::debug("BackgroundPrefetcher: no messages for chat {}", chat_id);
            return;
        }

        // Store messages in SQLite
        for (const auto& msg : messages) {
            db_cache_.cache_message(msg);
        }

        // Sort by timestamp for stats calculation
        std::sort(messages.begin(), messages.end(), [](const tg::Message& a, const tg::Message& b) {
            return a.timestamp < b.timestamp;
        });

        // Format messages for cache (if we have resolvers)
        std::string content;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (user_resolver_ && chat_resolver_) {
                std::vector<tg::MessageInfo> infos;
                infos.reserve(messages.size());
                for (const auto& msg : messages) {
                    infos.push_back({msg, user_resolver_(msg.sender_id), chat_resolver_(chat_id)});
                }
                content = fmt::format("{}\n", fmt::join(infos, "\n"));
            }
        }

        // Update chat message stats
        tg::ChatMessageStats stats;
        stats.chat_id = chat_id;
        stats.message_count = messages.size();
        stats.content_size = content.size();
        stats.last_message_time = messages.back().timestamp;
        stats.oldest_message_time = messages.front().timestamp;
        stats.last_fetch_time = std::time(nullptr);
        db_cache_.update_chat_message_stats(stats);

        // Store formatted content in TLRU cache
        if (!content.empty()) {
            cache_.store(chat_id, std::move(content), messages.size(), messages.back().id);
        }

        // Evict old messages from SQLite
        auto cutoff = std::chrono::system_clock::now() - config_.max_history_age;
        auto cutoff_ts = std::chrono::duration_cast<std::chrono::seconds>(cutoff.time_since_epoch()).count();
        db_cache_.evict_old_messages(chat_id, cutoff_ts);

        spdlog::debug(
            "BackgroundPrefetcher: fetched {} messages for chat {}, {} bytes",
            messages.size(),
            chat_id,
            stats.content_size
        );

    } catch (const std::exception& e) {
        spdlog::warn("BackgroundPrefetcher: failed to fetch chat {}: {}", chat_id, e.what());
    }
}

std::vector<int64_t> BackgroundPrefetcher::get_chats_to_fetch() {
    std::vector<int64_t> result;

    try {
        // Get all users (contacts first, then non-contacts)
        auto users = db_cache_.get_all_cached_users();
        std::vector<tg::User> contacts;
        std::vector<tg::User> non_contacts;

        for (const auto& user : users) {
            if (user.is_contact) {
                contacts.push_back(user);
            } else {
                non_contacts.push_back(user);
            }
        }

        // Sort by last_message_timestamp DESC
        auto sort_by_time = [](const tg::User& a, const tg::User& b) {
            return a.last_message_timestamp > b.last_message_timestamp;
        };
        std::sort(contacts.begin(), contacts.end(), sort_by_time);
        std::sort(non_contacts.begin(), non_contacts.end(), sort_by_time);

        // Add contacts first
        for (const auto& user : contacts) {
            result.push_back(user.id);
        }

        // Then non-contacts
        for (const auto& user : non_contacts) {
            result.push_back(user.id);
        }

        // Get groups and channels
        auto groups = db_cache_.get_cached_chats_by_type(tg::ChatType::GROUP);
        auto channels = db_cache_.get_cached_chats_by_type(tg::ChatType::CHANNEL);

        // Already sorted by last_message_timestamp DESC from get_cached_chats_by_type
        for (const auto& chat : groups) {
            result.push_back(chat.id);
        }
        for (const auto& chat : channels) {
            result.push_back(chat.id);
        }

    } catch (const std::exception& e) {
        spdlog::warn("BackgroundPrefetcher: failed to get chats to fetch: {}", e.what());
    }

    return result;
}

bool BackgroundPrefetcher::needs_fetch(int64_t chat_id) {
    auto stats = db_cache_.get_chat_message_stats(chat_id);
    if (!stats) {
        return true;  // Never fetched
    }

    // Check if we have enough messages
    if (stats->message_count < config_.min_messages) {
        return true;
    }

    // Check if cache is stale (not fetched recently)
    auto now = std::time(nullptr);
    auto age = now - stats->last_fetch_time;
    if (age > static_cast<int64_t>(config_.prefetch_interval.count())) {
        return true;
    }

    return false;
}

}  // namespace tgfuse
