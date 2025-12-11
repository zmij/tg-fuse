#include "tg/rate_limiter.hpp"

#include <spdlog/spdlog.h>

namespace tg {

RateLimiter::RateLimiter(Config config)
    : config_(std::move(config)), last_request_(std::chrono::steady_clock::now() - config_.min_interval) {}

void RateLimiter::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_request_;

    if (elapsed < config_.min_interval) {
        auto wait_time = config_.min_interval - elapsed;
        spdlog::debug(
            "RateLimiter: waiting {}ms before next request",
            std::chrono::duration_cast<std::chrono::milliseconds>(wait_time).count()
        );
        cv_.wait_for(lock, wait_time);
    }

    last_request_ = std::chrono::steady_clock::now();
}

bool RateLimiter::try_acquire() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_request_;

    if (elapsed < config_.min_interval) {
        return false;
    }

    last_request_ = now;
    return true;
}

void RateLimiter::set_config(Config config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = std::move(config);
}

}  // namespace tg
