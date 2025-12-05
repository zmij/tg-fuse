# TG wrapper library
add_library(tglib STATIC
    tg/types.cpp
    tg/cache.cpp
    tg/client.cpp
)

# Set C++20 for the wrapper library (uses coroutines)
set_target_properties(tglib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories for the library
target_include_directories(tglib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Link dependencies for the library
target_link_libraries(tglib PUBLIC
    Td::TdStatic
    SQLite::SQLite3
    spdlog::spdlog
)

# Compiler flags for the library
target_compile_options(tglib PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# FUSE library
add_library(fuselib STATIC
    fuse/platform.cpp
    fuse/operations.cpp
    fuse/vfs.cpp
    fuse/mock_provider.cpp
)

# Set C++20 for the FUSE library
set_target_properties(fuselib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories for the library
target_include_directories(fuselib PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

# Link dependencies for the library
target_link_libraries(fuselib PUBLIC
    fuse::fuse
    spdlog::spdlog
)

# Compiler flags for the library
target_compile_options(fuselib PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# tg-fused daemon executable
add_executable(tg-fused
    fused/main.cpp
)

# Set C++ standard and output directory
set_target_properties(tg-fused PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Include directories
target_include_directories(tg-fused PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(tg-fused PRIVATE
    fuselib
    spdlog::spdlog
    CLI11::CLI11
)

# Compiler flags
target_compile_options(tg-fused PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# tg-fuse CLI companion app
add_executable(tg-fuse
    ctl/main.cpp
)

# Set C++ standard and output directory
set_target_properties(tg-fuse PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Include directories
target_include_directories(tg-fuse PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(tg-fuse PRIVATE
    spdlog::spdlog
    CLI11::CLI11
)

# Compiler flags
target_compile_options(tg-fuse PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)
