# Test executable
add_executable(tg-fuse-tests
    example_test.cpp
)

# Include directories
target_include_directories(tg-fuse-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(tg-fuse-tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(tg-fuse-tests)
