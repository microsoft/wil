
add_executable(witest.asan)

target_precompile_headers(witest.asan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

target_compile_options(witest.asan PRIVATE -fsanitize=address)

target_compile_features(witest.asan PRIVATE cxx_std_23)

target_compile_definitions(witest.asan PRIVATE
    -DCATCH_CONFIG_NO_WINDOWS_SEH # ASAN relies on first chance AVs
    -DWITEST_ADDRESS_SANITIZER # To conditionally enable/disable code
    )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(witest.asan PRIVATE
        # See below; not compatible with exceptions
        -DCATCH_CONFIG_DISABLE_EXCEPTIONS
        )

    # Clang ASan on Windows has issues with exceptions: https://github.com/google/sanitizers/issues/749
    if (MSVC)
        replace_cxx_flag("/EHsc" "/EHs-c-")
    else()
        target_compile_options(witest.asan PRIVATE -fno-exceptions)
    endif()

    # Since we're disabling exceptions, we also need to explicitly enable use of the STL since some tests rely on it
    target_compile_definitions(witest.asan PRIVATE
        -DWIL_USE_STL=1
        )

    if ($ENV{Platform} STREQUAL "x86")
        target_link_libraries(witest.asan PRIVATE
            clang_rt.asan_dynamic-i386.lib
            clang_rt.asan_dynamic_runtime_thunk-i386.lib
            )
    elseif ($ENV{Platform} STREQUAL "x64")
        target_link_libraries(witest.asan PRIVATE
            clang_rt.asan_dynamic-x86_64.lib
            clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
            )
    else()
        message(FATAL_ERROR "Platform '$ENV{Platform}' does not have support for ASan")
    endif()
else()
    # Using exceptions; we can compile all of the C++/WinRT tests
    set(EXTRA_SOURCES
        ${CPP20_SOURCES}
        )
endif()

target_compile_definitions(witest.asan PRIVATE
    # Since we link with Catch2, which is pre-built
    -D_DISABLE_VECTOR_ANNOTATION
    -D_DISABLE_STRING_ANNOTATION
    )

target_sources(witest.asan PUBLIC
    ${COMMON_SOURCES}
    ${DOWNLEVEL_SOURCES}
    ${WINRT_SOURCES}
    ${WIN11_DESKTOP_SOURCES}
    ${EXTRA_SOURCES}
    )
