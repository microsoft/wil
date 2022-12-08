
project(witest.asan)
add_executable(witest.asan)

target_compile_options(witest.asan PRIVATE -fsanitize=address)

target_compile_definitions(witest.asan PRIVATE
    -DCATCH_CONFIG_NO_WINDOWS_SEH # ASAN relies on first chance AVs
    -DWITEST_ADDRESS_SANITIZER # To conditionally enable/disable code
    )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_definitions(witest.asan PRIVATE
        -D_DISABLE_VECTOR_ANNOTATION # Not compatible with using lld-link
        )

    target_link_libraries(witest.asan PRIVATE
        clang_rt.asan_dynamic-x86_64.lib
        clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
endif()

target_sources(witest.asan PUBLIC
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinVerifyTrustTest.cpp
    )
