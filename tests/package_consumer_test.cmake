cmake_minimum_required(VERSION 3.20)

foreach(required_variable IN ITEMS
        PROJECT_SOURCE_DIR
        PROJECT_BINARY_DIR
        PACKAGE_TEST_PREFIX
        PACKAGE_TEST_CONSUMER_BUILD
        CMAKE_COMMAND
        CMAKE_CXX_COMPILER)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR
            "installed_package_consumer requires ${required_variable}")
    endif()
endforeach()

cmake_path(
    ABSOLUTE_PATH PROJECT_BINARY_DIR
    NORMALIZE
    OUTPUT_VARIABLE normalized_project_binary_dir
)
cmake_path(
    ABSOLUTE_PATH PACKAGE_TEST_PREFIX
    NORMALIZE
    OUTPUT_VARIABLE normalized_package_prefix
)
cmake_path(
    ABSOLUTE_PATH PACKAGE_TEST_CONSUMER_BUILD
    NORMALIZE
    OUTPUT_VARIABLE normalized_consumer_build
)

cmake_path(
    IS_PREFIX normalized_project_binary_dir
    "${normalized_package_prefix}"
    NORMALIZE
    prefix_is_inside_build
)
cmake_path(
    IS_PREFIX normalized_project_binary_dir
    "${normalized_consumer_build}"
    NORMALIZE
    consumer_build_is_inside_build
)

if(NOT prefix_is_inside_build OR
   normalized_package_prefix STREQUAL normalized_project_binary_dir)
    message(FATAL_ERROR
        "Package test prefix must be a dedicated directory inside the build tree")
endif()
if(NOT consumer_build_is_inside_build OR
   normalized_consumer_build STREQUAL normalized_project_binary_dir)
    message(FATAL_ERROR
        "Consumer build must be a dedicated directory inside the build tree")
endif()

function(run_checked step_name)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_stdout
        ERROR_VARIABLE command_stderr
    )
    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR
            "${step_name} failed with exit code ${command_result}\n"
            "stdout:\n${command_stdout}\n"
            "stderr:\n${command_stderr}")
    endif()
    message(STATUS
        "${step_name} succeeded\n"
        "stdout:\n${command_stdout}\n"
        "stderr:\n${command_stderr}")
endfunction()

file(REMOVE_RECURSE
    "${normalized_package_prefix}"
    "${normalized_consumer_build}"
)

set(install_command
    "${CMAKE_COMMAND}"
    --install
    "${normalized_project_binary_dir}"
    --prefix
    "${normalized_package_prefix}"
)
set(consumer_build_command
    "${CMAKE_COMMAND}"
    --build
    "${normalized_consumer_build}"
)

if(DEFINED PACKAGE_TEST_CONFIGURATION AND
   NOT PACKAGE_TEST_CONFIGURATION STREQUAL "")
    list(APPEND install_command
        --config
        "${PACKAGE_TEST_CONFIGURATION}"
    )
    list(APPEND consumer_build_command
        --config
        "${PACKAGE_TEST_CONFIGURATION}"
    )
endif()

run_checked("Package installation" ${install_command})

set(consumer_configure_command
    "${CMAKE_COMMAND}"
    -S
    "${PROJECT_SOURCE_DIR}/examples/installed_consumer"
    -B
    "${normalized_consumer_build}"
    "-DCMAKE_PREFIX_PATH=${normalized_package_prefix}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
)

if(DEFINED PACKAGE_TEST_GENERATOR AND
   NOT PACKAGE_TEST_GENERATOR STREQUAL "")
    list(APPEND consumer_configure_command
        -G
        "${PACKAGE_TEST_GENERATOR}"
    )
endif()

if(DEFINED PACKAGE_TEST_CONFIGURATION AND
   NOT PACKAGE_TEST_CONFIGURATION STREQUAL "" AND
   NOT PACKAGE_TEST_MULTI_CONFIG)
    list(APPEND consumer_configure_command
        "-DCMAKE_BUILD_TYPE=${PACKAGE_TEST_CONFIGURATION}"
    )
endif()

if(PACKAGE_TEST_ENABLE_ASAN_UBSAN)
    list(APPEND consumer_configure_command
        "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
    )
elseif(PACKAGE_TEST_ENABLE_TSAN)
    list(APPEND consumer_configure_command
        "-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"
    )
endif()

run_checked("Consumer configuration" ${consumer_configure_command})
run_checked("Consumer build" ${consumer_build_command})

if(PACKAGE_TEST_MULTI_CONFIG AND
   DEFINED PACKAGE_TEST_CONFIGURATION AND
   NOT PACKAGE_TEST_CONFIGURATION STREQUAL "")
    set(consumer_executable
        "${normalized_consumer_build}/${PACKAGE_TEST_CONFIGURATION}/installed_consumer")
else()
    set(consumer_executable
        "${normalized_consumer_build}/installed_consumer")
endif()

if(WIN32)
    string(APPEND consumer_executable ".exe")
endif()

if(NOT EXISTS "${consumer_executable}")
    message(FATAL_ERROR
        "Consumer executable was not created at ${consumer_executable}")
endif()

run_checked("Consumer execution" "${consumer_executable}")
