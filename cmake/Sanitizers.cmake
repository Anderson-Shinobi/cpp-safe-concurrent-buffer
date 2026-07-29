include_guard(GLOBAL)

if(ENABLE_ASAN_UBSAN AND ENABLE_TSAN)
    message(FATAL_ERROR
        "ASan/UBSan and ThreadSanitizer cannot be enabled together."
    )
endif()

function(enable_project_sanitizers target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR
            "Cannot enable sanitizers for unknown target: ${target_name}"
        )
    endif()

    if(NOT ENABLE_ASAN_UBSAN AND NOT ENABLE_TSAN)
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "Sanitizers are supported only with GCC or Clang in this project."
        )
    endif()

    if(ENABLE_ASAN_UBSAN)
        set(sanitizer_flags
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
        )
    elseif(ENABLE_TSAN)
        set(sanitizer_flags
            -fsanitize=thread
            -fno-omit-frame-pointer
        )
    endif()

    target_compile_options("${target_name}" PRIVATE ${sanitizer_flags})
    target_link_options("${target_name}" PRIVATE ${sanitizer_flags})
endfunction()
