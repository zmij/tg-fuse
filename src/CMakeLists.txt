# tg-fuse executable
add_executable(tg-fuse
    main.cpp
)

# Set C++ standard for this target (can be different from TDLib)
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
    fuse::fuse
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    CLI11::CLI11
    Td::TdStatic
)

# Compiler flags
target_compile_options(tg-fuse PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)
