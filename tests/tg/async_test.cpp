#include "tg/async.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace tg {
namespace {

// Simple coroutine that returns a value
Task<int> simple_coroutine() { co_return 42; }

// Coroutine that co_awaits another
Task<int> nested_coroutine() {
    auto result = co_await simple_coroutine();
    co_return result * 2;
}

// Coroutine with void return
Task<void> void_coroutine() { co_return; }

// Coroutine that throws
Task<int> throwing_coroutine() {
    throw std::runtime_error("Test error");
    co_return 0;
}

// Test basic Task functionality
TEST(AsyncTest, SimpleCoroutineReturnsValue) {
    auto task = simple_coroutine();
    auto result = task.get_result();
    EXPECT_EQ(result, 42);
}

TEST(AsyncTest, NestedCoroutine) {
    auto task = nested_coroutine();
    auto result = task.get_result();
    EXPECT_EQ(result, 84);
}

TEST(AsyncTest, VoidCoroutine) {
    auto task = void_coroutine();
    EXPECT_NO_THROW(task.get_result());
}

TEST(AsyncTest, ExceptionPropagation) {
    auto task = throwing_coroutine();
    EXPECT_THROW(task.get_result(), std::runtime_error);
}

// Test TdPromise
TEST(AsyncTest, TdPromiseSetValue) {
    TdPromise<int> promise;

    std::thread setter([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        promise.set_value(123);
    });

    auto awaiter_task = [](TdPromise<int>& p) -> Task<int> {
        auto result = co_await p;
        co_return result;
    }(promise);

    setter.join();

    auto result = awaiter_task.get_result();
    EXPECT_EQ(result, 123);
}

TEST(AsyncTest, TdPromiseSetException) {
    TdPromise<int> promise;

    std::thread setter([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        promise.set_exception(std::make_exception_ptr(std::runtime_error("Async error")));
    });

    auto awaiter_task = [](TdPromise<int>& p) -> Task<int> {
        auto result = co_await p;
        co_return result;
    }(promise);

    setter.join();

    EXPECT_THROW(awaiter_task.get_result(), std::runtime_error);
}

TEST(AsyncTest, TdPromiseVoid) {
    TdPromise<void> promise;

    std::thread setter([&promise]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        promise.set_value();
    });

    auto awaiter_task = [](TdPromise<void>& p) -> Task<void> {
        co_await p;
        co_return;
    }(promise);

    setter.join();

    EXPECT_NO_THROW(awaiter_task.get_result());
}

// Test chaining multiple coroutines
Task<int> chain_step1() { co_return 10; }

Task<int> chain_step2(int value) {
    auto result = co_await chain_step1();
    co_return value + result;
}

Task<int> chain_step3() {
    auto step2_result = co_await chain_step2(20);
    co_return step2_result * 2;
}

TEST(AsyncTest, CoroutineChaining) {
    auto task = chain_step3();
    auto result = task.get_result();
    EXPECT_EQ(result, 60);  // (20 + 10) * 2
}

// Test multiple co_await in sequence
Task<int> multiple_awaits() {
    auto r1 = co_await simple_coroutine();
    auto r2 = co_await simple_coroutine();
    auto r3 = co_await simple_coroutine();
    co_return r1 + r2 + r3;
}

TEST(AsyncTest, MultipleAwaits) {
    auto task = multiple_awaits();
    auto result = task.get_result();
    EXPECT_EQ(result, 126);  // 42 + 42 + 42
}

// Test error handling in chains
Task<int> chain_with_error() {
    try {
        auto result = co_await throwing_coroutine();
        co_return result;
    } catch (const std::exception&) {
        co_return -1;  // Error caught, return sentinel
    }
}

TEST(AsyncTest, ExceptionHandlingInChain) {
    auto task = chain_with_error();
    auto result = task.get_result();
    EXPECT_EQ(result, -1);
}

// Test Task move semantics
TEST(AsyncTest, TaskMoveSemantics) {
    auto task1 = simple_coroutine();
    Task<int> task2 = std::move(task1);

    auto result = task2.get_result();
    EXPECT_EQ(result, 42);
}

// Stress test with many coroutines (TDLib pattern)
Task<int> stress_coroutine(int value) { co_return value * 2; }

TEST(AsyncTest, StressTestManyCoroutines) {
    const int num_coroutines = 1000;
    std::vector<Task<int>> tasks;

    for (int i = 0; i < num_coroutines; ++i) {
        tasks.push_back(stress_coroutine(i));
    }

    for (int i = 0; i < num_coroutines; ++i) {
        auto result = tasks[i].get_result();
        EXPECT_EQ(result, i * 2);
    }
}

// Test concurrent promise resolution
TEST(AsyncTest, ConcurrentPromiseResolution) {
    const int num_promises = 100;
    std::vector<TdPromise<int>> promises(num_promises);
    std::vector<std::thread> threads;

    // Start threads that will resolve promises
    for (int i = 0; i < num_promises; ++i) {
        threads.emplace_back([&promises, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            promises[i].set_value(i);
        });
    }

    // Create coroutines that await the promises
    std::vector<Task<int>> tasks;
    for (int i = 0; i < num_promises; ++i) {
        tasks.push_back([](TdPromise<int>& p) -> Task<int> {
            auto result = co_await p;
            co_return result;
        }(promises[i]));
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify results
    for (int i = 0; i < num_promises; ++i) {
        auto result = tasks[i].get_result();
        EXPECT_EQ(result, i);
    }
}

// Test that Task can be resumed manually
TEST(AsyncTest, ManualResume) {
    auto task = simple_coroutine();

    // Resume manually
    task.resume();

    EXPECT_TRUE(task.done());

    auto result = task.get_result();
    EXPECT_EQ(result, 42);
}

// Test lazy evaluation
Task<int> lazy_coroutine() {
    // This should not execute until awaited or resumed
    co_return 100;
}

TEST(AsyncTest, LazyEvaluation) {
    auto task = lazy_coroutine();

    // Task is created but not executed yet
    EXPECT_FALSE(task.done());

    // Execute by calling get_result
    auto result = task.get_result();

    EXPECT_TRUE(task.done());
    EXPECT_EQ(result, 100);
}

}  // namespace
}  // namespace tg
