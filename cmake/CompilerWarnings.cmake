include_guard(GLOBAL)

include(CheckCXXCompilerFlag)

function(enable_project_warnings target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Cannot enable warnings for unknown target: ${target_name}")
    endif()

    if(NOT ENABLE_STRICT_WARNINGS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(project_warning_flags
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wold-style-cast
            -Wcast-align
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
            -Wimplicit-fallthrough
        )

        foreach(warning_flag IN LISTS project_warning_flags)
            string(MAKE_C_IDENTIFIER
                "${CMAKE_CXX_COMPILER_ID}_${warning_flag}"
                warning_flag_identifier
            )
            set(support_variable
                "PROJECT_SUPPORTS_WARNING_${warning_flag_identifier}"
            )
            check_cxx_compiler_flag("${warning_flag}" "${support_variable}")
            if(${support_variable})
                target_compile_options("${target_name}" PRIVATE "${warning_flag}")
            endif()
        endforeach()

        if(ENABLE_WARNINGS_AS_ERRORS)
            target_compile_options("${target_name}" PRIVATE -Werror)
        endif()
    else()
        message(WARNING
            "Strict project warnings are not configured for "
            "${CMAKE_CXX_COMPILER_ID}; target ${target_name} is unchanged."
        )
    endif()
endfunction()
