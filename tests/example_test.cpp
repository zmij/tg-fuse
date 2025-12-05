#include <gtest/gtest.h>

// Example test demonstrating basic GoogleTest functionality
TEST(ExampleTest, BasicAssertion) { EXPECT_EQ(1 + 1, 2); }

TEST(ExampleTest, StringComparison) {
    std::string hello = "Hello";
    EXPECT_EQ(hello, "Hello");
    EXPECT_NE(hello, "World");
}

// TODO: Add actual tg-fuse unit tests
