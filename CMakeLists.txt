cmake_minimum_required(VERSION 3.20)

project(
    cpp_safe_concurrent_buffer
    VERSION 0.3.0
    LANGUAGES CXX
)

set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

add_library(
    cpp_safe_concurrent_buffer::concurrent_buffer
    ALIAS
    concurrent_buffer
)

target_compile_features(concurrent_buffer
    PUBLIC
        cxx_std_20
)

target_include_directories(concurrent_buffer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(concurrent_buffer
    PUBLIC
        Threads::Threads
)

enable_project_warnings(concurrent_buffer)
enable_project_sanitizers(concurrent_buffer)

install(
    TARGETS concurrent_buffer
    EXPORT cpp_safe_concurrent_bufferTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    FILES
        include/safe_concurrent_buffer.hpp
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT cpp_safe_concurrent_bufferTargets
    FILE cpp_safe_concurrent_bufferTargets.cmake
    NAMESPACE cpp_safe_concurrent_buffer::
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_safe_concurrent_buffer
)

configure_package_config_file(
    cmake/cpp_safe_concurrent_bufferConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/cpp_safe_concurrent_bufferConfig.cmake"
    INSTALL_DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/cpp_safe_concurrent_buffer"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cpp_safe_concurrent_bufferConfigVersion.cmake"
    VERSION
        ${PROJECT_VERSION}
    COMPATIBILITY
        SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cpp_safe_concurrent_bufferConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cpp_safe_concurrent_bufferConfigVersion.cmake"
    DESTINATION
        "${CMAKE_INSTALL_LIBDIR}/cmake/cpp_safe_concurrent_buffer"
)

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
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
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

    if(CMAKE_CONFIGURATION_TYPES)
        set(package_test_multi_config ON)
    else()
        set(package_test_multi_config OFF)
    endif()

    add_test(
        NAME installed_package_consumer
        COMMAND
            ${CMAKE_COMMAND}
            "-DPROJECT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DPROJECT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DPACKAGE_TEST_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/package-test/install"
            "-DPACKAGE_TEST_CONSUMER_BUILD=${CMAKE_CURRENT_BINARY_DIR}/package-test/consumer-build"
            "-DPACKAGE_TEST_CONFIGURATION=$<CONFIG>"
            "-DPACKAGE_TEST_GENERATOR=${CMAKE_GENERATOR}"
            "-DPACKAGE_TEST_MULTI_CONFIG=${package_test_multi_config}"
            "-DPACKAGE_TEST_ENABLE_ASAN_UBSAN=${ENABLE_ASAN_UBSAN}"
            "-DPACKAGE_TEST_ENABLE_TSAN=${ENABLE_TSAN}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            -P
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/package_consumer_test.cmake"
    )
    set_tests_properties(
        installed_package_consumer
        PROPERTIES
            TIMEOUT 180
    )
endif()
