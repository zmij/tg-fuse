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

# tg-fuse executable
add_executable(tg-fuse
    main.cpp
)

# Set C++ standard for this target
set_target_properties(tg-fuse PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Include directories
target_include_directories(tg-fuse PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link libraries using modern CMake targets
target_link_libraries(tg-fuse PRIVATE
    tglib
    fuse::fuse
    nlohmann_json::nlohmann_json
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
