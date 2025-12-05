# Test executable
add_executable(tg-fuse-tests
    tg/types_test.cpp
    tg/cache_test.cpp
    tg/async_test.cpp
    tg/formatters_test.cpp
    tg/bustache_format_test.cpp
)

# Set C++20 for tests (required for coroutines)
set_target_properties(tg-fuse-tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(tg-fuse-tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(tg-fuse-tests PRIVATE
    tglib
    GTest::gtest
    GTest::gtest_main
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    bustache
)

# Compiler flags
target_compile_options(tg-fuse-tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(tg-fuse-tests)
