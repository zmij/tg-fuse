# tg-fuse executable
add_executable(tg-fuse
    main.cpp
)

# Include directories
target_include_directories(tg-fuse PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${FUSE_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(tg-fuse PRIVATE
    ${FUSE_LIBRARIES}
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
