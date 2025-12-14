cmake_minimum_required(VERSION 3.20)
project(tg-fuse VERSION 0.1.0 LANGUAGES CXX)

# Default to C++17 for dependencies (TDLib requirement)
# Individual targets can override this
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies source dir to avoid (re-)download sources into build dir
set(DEPS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/deps" CACHE PATH "Directory for dependency sources")

# Include FetchContent for dependency management
include(FetchContent)

# Platform detection and FUSE library setup
if(APPLE)
    # macOS with macFUSE/osxfuse
    set(FUSE_INCLUDE_DIR "/usr/local/include")
    set(FUSE_LIBRARIES "/usr/local/lib/libfuse.dylib")
    set(FUSE_VERSION 29)

    # Create imported target for FUSE
    add_library(fuse::fuse SHARED IMPORTED)
    set_target_properties(fuse::fuse PROPERTIES
        IMPORTED_LOCATION "${FUSE_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${FUSE_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS "FUSE_USE_VERSION=${FUSE_VERSION};_FILE_OFFSET_BITS=64"
    )
    message(STATUS "macOS FUSE library found: ${FUSE_LIBRARIES}")

elseif(UNIX)
    # Linux with libfuse3
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FUSE REQUIRED IMPORTED_TARGET fuse3)
    set(FUSE_VERSION 35)

    # Create alias for consistency
    add_library(fuse::fuse ALIAS PkgConfig::FUSE)
    # Add compile definitions to the imported target
    set_property(TARGET PkgConfig::FUSE APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS "FUSE_USE_VERSION=${FUSE_VERSION};_FILE_OFFSET_BITS=64"
    )
    message(STATUS "Linux FUSE3 library found")
endif()

# Disable testing globally before fetching dependencies
set(BUILD_TESTING OFF CACHE INTERNAL "Disable testing for dependencies")

# Disable tests and benchmarks for third-party libraries
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_TESTING OFF CACHE INTERNAL "")
set(SPDLOG_BUILD_BENCH OFF CACHE INTERNAL "")
set(SPDLOG_FMT_EXTERNAL ON CACHE INTERNAL "")  # Use external fmt library
set(CLI11_BUILD_TESTS OFF CACHE INTERNAL "")
set(CLI11_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(BUILD_GMOCK OFF CACHE INTERNAL "")
set(INSTALL_GTEST OFF CACHE INTERNAL "")

# Disable TDLib extras
set(TD_ENABLE_TESTS OFF CACHE INTERNAL "")
set(TD_ENABLE_BENCHMARKS OFF CACHE INTERNAL "")
set(TD_ENABLE_JNI OFF CACHE INTERNAL "")
set(TD_ENABLE_DOTNET OFF CACHE INTERNAL "")

# Fetch nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    SOURCE_DIR ${DEPS_SOURCE_DIR}/nlohmann_json
)

# Fetch spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
    SOURCE_DIR ${DEPS_SOURCE_DIR}/spdlog
)

# Fetch CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.1
    SOURCE_DIR ${DEPS_SOURCE_DIR}/cli11
)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    SOURCE_DIR ${DEPS_SOURCE_DIR}/googletest
)

# Fetch TDLib
FetchContent_Declare(
    tdlib
    GIT_REPOSITORY https://github.com/tdlib/td.git
    GIT_TAG master
    GIT_PROGRESS TRUE
    SOURCE_DIR ${DEPS_SOURCE_DIR}/tdlib
)

# Fetch fmt library
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
    SOURCE_DIR ${DEPS_SOURCE_DIR}/fmt
    OVERRIDE_FIND_PACKAGE # bustache tries to find it via find_package
)

# Fetch bustache (mustache templating)
set(BUSTACHE_ENABLE_TESTING OFF CACHE INTERNAL "")
set(BUSTACHE_USE_FMT ON CACHE INTERNAL "")
FetchContent_Declare(
    bustache
    GIT_REPOSITORY https://github.com/jamboree/bustache.git
    GIT_TAG master
    SOURCE_DIR ${DEPS_SOURCE_DIR}/bustache
)

# Find SQLite3 (system library)
find_package(SQLite3 REQUIRED)

# Make dependencies available (fmt before spdlog and bustache since they use it)
FetchContent_MakeAvailable(nlohmann_json fmt spdlog cli11 googletest tdlib bustache)

# Enable testing for our project only (after dependencies are loaded)
enable_testing()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(tests)
add_subdirectory(examples)

# Install targets
install(TARGETS tg-fuse
    RUNTIME DESTINATION bin
)
