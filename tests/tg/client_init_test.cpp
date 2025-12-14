#include "tg/async.hpp"
#include "tg/exceptions.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace tg {
namespace {

// Mock initialization synchronizer that mirrors TelegramClient::Impl's init logic
// This allows testing the synchronization mechanism without actual TDLib dependencies
class MockInitSynchronizer {
public:
    MockInitSynchronizer() : init_completed_(false) {}

    void prepare() {
        init_promise_ = std::promise<void>();
        init_future_ = init_promise_.get_future();
        init_completed_ = false;
    }

    void wait_initialized(int timeout_ms = 5000) {
        if (init_completed_) {
            return;
        }

        if (init_future_.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
            throw TimeoutException("Initialization timeout");
        }
        init_future_.get();
    }

    void complete_initialization() {
        if (!init_completed_.exchange(true)) {
            try {
                init_promise_.set_value();
            } catch (const std::future_error&) {
                // Promise already satisfied - ignore
            }
        }
    }

    bool is_completed() const { return init_completed_; }

private:
    std::promise<void> init_promise_;
    std::future<void> init_future_;
    std::atomic<bool> init_completed_;
};

// Test that wait_initialized blocks until complete_initialization is called
TEST(ClientInitTest, WaitBlocksUntilComplete) {
    MockInitSynchronizer sync;
    sync.prepare();

    std::atomic<bool> wait_finished{false};

    // Start a thread that waits for initialization
    std::thread waiter([&sync, &wait_finished]() {
        sync.wait_initialized();
        wait_finished = true;
    });

    // Give the waiter time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(wait_finished);

    // Complete initialization from another thread (simulating TDLib update thread)
    sync.complete_initialization();

    waiter.join();
    EXPECT_TRUE(wait_finished);
    EXPECT_TRUE(sync.is_completed());
}

// Test that wait_initialized returns immediately if already completed
TEST(ClientInitTest, WaitReturnsImmediatelyIfAlreadyComplete) {
    MockInitSynchronizer sync;
    sync.prepare();

    // Complete before waiting
    sync.complete_initialization();

    auto start = std::chrono::steady_clock::now();
    sync.wait_initialized();
    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should return almost immediately (less than 10ms)
    EXPECT_LT(elapsed, std::chrono::milliseconds(10));
}

// Test timeout when initialization never completes
TEST(ClientInitTest, TimeoutWhenNeverCompletes) {
    MockInitSynchronizer sync;
    sync.prepare();

    EXPECT_THROW(sync.wait_initialized(100), TimeoutException);
}

// Test that multiple calls to complete_initialization are safe
TEST(ClientInitTest, MultipleCompleteCallsAreSafe) {
    MockInitSynchronizer sync;
    sync.prepare();

    // Call complete multiple times from different threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&sync]() { sync.complete_initialization(); });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(sync.is_completed());
    EXPECT_NO_THROW(sync.wait_initialized());
}

// Test wait and complete from different threads (the actual TelegramClient pattern)
// Note: std::future is not thread-safe for concurrent wait_for() calls,
// so only one thread should wait while another completes - exactly as in production
TEST(ClientInitTest, WaitAndCompleteFromDifferentThreads) {
    MockInitSynchronizer sync;
    sync.prepare();

    std::atomic<bool> wait_started{false};
    std::atomic<bool> wait_finished{false};

    // Single waiter thread (like the main thread calling start())
    std::thread waiter([&sync, &wait_started, &wait_finished]() {
        wait_started = true;
        sync.wait_initialized();
        wait_finished = true;
    });

    // Wait for waiter to start
    while (!wait_started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(wait_finished);

    // Complete from another thread (like the TDLib update thread)
    std::thread completer([&sync]() { sync.complete_initialization(); });

    completer.join();
    waiter.join();

    EXPECT_TRUE(wait_finished);
}

// Test fast completion (before wait starts)
TEST(ClientInitTest, FastCompletion) {
    MockInitSynchronizer sync;
    sync.prepare();

    // Complete immediately
    sync.complete_initialization();

    // Wait should return immediately
    EXPECT_NO_THROW(sync.wait_initialized());
}

// Test that prepare() can be called again after completion (for restart scenarios)
TEST(ClientInitTest, CanReprepareAfterCompletion) {
    MockInitSynchronizer sync;

    // First cycle
    sync.prepare();
    sync.complete_initialization();
    sync.wait_initialized();
    EXPECT_TRUE(sync.is_completed());

    // Second cycle (simulating client restart)
    sync.prepare();
    EXPECT_FALSE(sync.is_completed());

    std::thread completer([&sync]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        sync.complete_initialization();
    });

    sync.wait_initialized();
    completer.join();

    EXPECT_TRUE(sync.is_completed());
}

// Stress test with rapid prepare/complete cycles
TEST(ClientInitTest, StressTestRapidCycles) {
    MockInitSynchronizer sync;

    for (int cycle = 0; cycle < 100; ++cycle) {
        sync.prepare();

        std::thread completer([&sync]() { sync.complete_initialization(); });

        sync.wait_initialized();
        completer.join();

        EXPECT_TRUE(sync.is_completed());
    }
}

// Test simulating actual TDLib authorization state flow
TEST(ClientInitTest, SimulateTdLibAuthFlow) {
    MockInitSynchronizer sync;
    sync.prepare();

    // Simulate TDLib update thread processing authorization states
    std::thread tdlib_thread([&sync]() {
        // Simulate: authorizationStateWaitTdlibParameters (no signal)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Simulate: setTdlibParameters response received
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Simulate: authorizationStateWaitPhoneNumber or authorizationStateReady
        // This is when we signal initialization complete
        sync.complete_initialization();
    });

    // Main thread waits for initialization
    auto start = std::chrono::steady_clock::now();
    sync.wait_initialized();
    auto elapsed = std::chrono::steady_clock::now() - start;

    tdlib_thread.join();

    // Should complete in roughly 20ms (the simulated TDLib delay)
    EXPECT_GE(elapsed, std::chrono::milliseconds(15));
    EXPECT_LT(elapsed, std::chrono::milliseconds(500));
}

// Test using Task<void> pattern (as used in TelegramClient::start())
TEST(ClientInitTest, TaskPatternIntegration) {
    MockInitSynchronizer sync;

    // This simulates TelegramClient::start()
    auto start_task = [&sync]() -> Task<void> {
        sync.prepare();

        // Simulate spawning update thread (in real code this happens before wait)
        std::thread update_thread([&sync]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            sync.complete_initialization();
        });
        update_thread.detach();

        // Wait for initialization
        sync.wait_initialized();
        co_return;
    };

    auto task = start_task();
    EXPECT_NO_THROW(task.get_result());
    EXPECT_TRUE(sync.is_completed());
}

// Test exception propagation through the initialization mechanism
class MockInitSynchronizerWithException {
public:
    void prepare() {
        init_promise_ = std::promise<void>();
        init_future_ = init_promise_.get_future();
        init_completed_ = false;
    }

    void wait_initialized(int timeout_ms = 5000) {
        if (init_completed_) {
            if (exception_) {
                std::rethrow_exception(exception_);
            }
            return;
        }

        if (init_future_.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
            throw TimeoutException("Initialization timeout");
        }
        init_future_.get();  // This will rethrow if set_exception was called
    }

    void complete_with_error(std::exception_ptr ex) {
        if (!init_completed_.exchange(true)) {
            exception_ = ex;
            try {
                init_promise_.set_exception(ex);
            } catch (const std::future_error&) {
                // Promise already satisfied - ignore
            }
        }
    }

private:
    std::promise<void> init_promise_;
    std::future<void> init_future_;
    std::atomic<bool> init_completed_{false};
    std::exception_ptr exception_;
};

TEST(ClientInitTest, ExceptionPropagation) {
    MockInitSynchronizerWithException sync;
    sync.prepare();

    std::thread error_thread([&sync]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        sync.complete_with_error(std::make_exception_ptr(std::runtime_error("TDLib init failed")));
    });

    EXPECT_THROW(sync.wait_initialized(), std::runtime_error);
    error_thread.join();
}

}  // namespace
}  // namespace tg
