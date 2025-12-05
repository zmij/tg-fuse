#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace tg {

/// Configuration for RateLimiter
struct RateLimiterConfig {
    std::size_t max_requests_per_second = 2;       // Conservative default for background tasks
    std::chrono::milliseconds min_interval{500};   // Minimum time between requests
    std::chrono::milliseconds burst_window{1000};  // Window for burst detection
};

/// Simple rate limiter to avoid Telegram API flood bans
///
/// Uses a token bucket algorithm with configurable rate.
/// Thread-safe and blocking when rate limit is exceeded.
class RateLimiter {
public:
    using Config = RateLimiterConfig;

    explicit RateLimiter(Config config = {});
    ~RateLimiter() = default;

    // Disable copy
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    /// Wait until we can make a request (blocks if rate limit exceeded)
    void acquire();

    /// Try to acquire without blocking
    /// @return true if acquired, false if would exceed rate limit
    bool try_acquire();

    /// Get current configuration
    [[nodiscard]] const Config& get_config() const { return config_; }

    /// Update configuration (thread-safe)
    void set_config(Config config);

private:
    Config config_;
    std::chrono::steady_clock::time_point last_request_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace tg
