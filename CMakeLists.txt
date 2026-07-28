cmake_minimum_required(VERSION 3.20)

project(
    cpp_safe_concurrent_buffer
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(concurrent_buffer
    src/safe_concurrent_buffer.cpp
)

target_compile_features(concurrent_buffer
    PUBLIC
        cxx_std_20
)

target_include_directories(concurrent_buffer
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(concurrent_buffer
    PUBLIC
        Threads::Threads
)

add_executable(concurrent_buffer_demo
    src/main.cpp
)

target_link_libraries(concurrent_buffer_demo
    PRIVATE
        concurrent_buffer
)
