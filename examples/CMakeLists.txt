# Examples for TG wrapper

# Simple client example
add_executable(simple_client
    simple_client.cpp
)

set_target_properties(simple_client PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_include_directories(simple_client PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(simple_client PRIVATE
    tglib
    spdlog::spdlog
)

target_compile_options(simple_client PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
