
add_executable(witest.asan)

target_precompile_headers(witest.asan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

target_compile_options(witest.asan PRIVATE -fsanitize=address)

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
    else()
        target_link_libraries(witest.asan PRIVATE
            clang_rt.asan_dynamic-x86_64.lib
            clang_rt.asan_dynamic_runtime_thunk-x86_64.lib
            )
    endif()
else()
    # Using exceptions, so we can compile the STL tests
    set(EXTRA_SOURCES
        ${EXTRA_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
        )
endif()

target_compile_definitions(witest.asan PRIVATE
    # Since we link with Catch2, which is pre-built
    -D_DISABLE_VECTOR_ANNOTATION
    -D_DISABLE_STRING_ANNOTATION
    )

target_sources(witest.asan PUBLIC
    ${COMMON_SOURCES}
    ${EXTRA_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../NetworkTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../RegistryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinVerifyTrustTest.cpp
    )
