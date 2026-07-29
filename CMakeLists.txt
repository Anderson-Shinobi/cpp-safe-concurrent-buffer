cmake_minimum_required(VERSION 3.20)

project(
    cpp_safe_concurrent_buffer
    VERSION 0.2.0
    LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

option(
    ENABLE_STRICT_WARNINGS
    "Enable strict compiler warnings"
    ON
)
option(
    ENABLE_WARNINGS_AS_ERRORS
    "Treat project warnings as errors"
    ON
)
option(
    ENABLE_ASAN_UBSAN
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)
option(
    ENABLE_TSAN
    "Enable ThreadSanitizer"
    OFF
)
option(
    BUILD_BENCHMARKS
    "Build the lightweight benchmark executable"
    OFF
)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

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

enable_project_warnings(concurrent_buffer)
enable_project_sanitizers(concurrent_buffer)

add_executable(concurrent_buffer_demo
    src/main.cpp
)

target_link_libraries(concurrent_buffer_demo
    PRIVATE
        concurrent_buffer
)

enable_project_warnings(concurrent_buffer_demo)
enable_project_sanitizers(concurrent_buffer_demo)

if(BUILD_BENCHMARKS)
    add_executable(concurrent_buffer_benchmark
        benchmarks/buffer_benchmark.cpp
    )

    target_link_libraries(concurrent_buffer_benchmark
        PRIVATE
            concurrent_buffer
    )

    enable_project_warnings(concurrent_buffer_benchmark)
    enable_project_sanitizers(concurrent_buffer_benchmark)
endif()

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

    enable_project_warnings(concurrent_buffer_tests)
    enable_project_sanitizers(concurrent_buffer_tests)

    include(GoogleTest)
    gtest_discover_tests(
        concurrent_buffer_tests
        DISCOVERY_TIMEOUT 30
        DISCOVERY_MODE PRE_TEST
    )
endif()
