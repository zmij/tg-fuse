cmake_minimum_required(VERSION 3.20)
project(tg-fuse VERSION 0.1.0 LANGUAGES CXX)

# Require C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Include FetchContent for dependency management
include(FetchContent)

# Platform detection
if(APPLE)
    set(FUSE_INCLUDE_DIR "/usr/local/include/osxfuse")
    set(FUSE_LIBRARY_NAME "osxfuse")
    add_compile_definitions(FUSE_USE_VERSION=29)
elseif(UNIX)
    set(FUSE_INCLUDE_DIR "/usr/include/fuse3")
    set(FUSE_LIBRARY_NAME "fuse3")
    add_compile_definitions(FUSE_USE_VERSION=35)
endif()

# Find FUSE library
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE REQUIRED ${FUSE_LIBRARY_NAME})

# Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)

# Fetch CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.1
)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# Fetch TDLib
FetchContent_Declare(
    tdlib
    GIT_REPOSITORY https://github.com/tdlib/td.git
    GIT_TAG v1.8.29
)

# Make dependencies available
FetchContent_MakeAvailable(nlohmann_json spdlog cli11 googletest tdlib)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)

# Install targets
install(TARGETS tg-fuse
    RUNTIME DESTINATION bin
)
