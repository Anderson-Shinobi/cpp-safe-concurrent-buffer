cmake_minimum_required(VERSION 3.20)

project(
    installed_consumer
    LANGUAGES CXX
)

find_package(
    cpp_safe_concurrent_buffer
    0.3
    REQUIRED
)

add_executable(
    installed_consumer
    main.cpp
)

target_link_libraries(
    installed_consumer
    PRIVATE
        cpp_safe_concurrent_buffer::concurrent_buffer
)
