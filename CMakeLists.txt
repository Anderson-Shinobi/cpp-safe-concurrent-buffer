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

option(BUILD_TESTING "Build the test suite" ON)
include(CTest)

if(BUILD_TESTING)
    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(concurrent_buffer_tests
        tests/smoke_test.cpp
    )

    target_link_libraries(concurrent_buffer_tests
        PRIVATE
            concurrent_buffer
            GTest::gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(
        concurrent_buffer_tests
        DISCOVERY_TIMEOUT 30
    )
endif()
