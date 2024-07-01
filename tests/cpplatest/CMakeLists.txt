
add_executable(witest.cpplatest)

target_precompile_headers(witest.cpplatest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

# Compilers often don't use the latest C++ standard as the default. Periodically update this value (possibly conditioned
# on compiler) as new standards are ratified/support is available
target_compile_features(witest.cpplatest PRIVATE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang seems to have issues with coroutines and exceptions, so disable some tests for now
    set(COROUTINE_SOURCES)

    target_compile_options(witest.cpplatest PRIVATE
        -fno-ms-compatibility
        )
else()
    set(COROUTINE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../ComApartmentVariableTests.cpp)

    target_compile_options(witest.cpplatest PRIVATE
        /Zc:preprocessor
        )
endif()

target_sources(witest.cpplatest PRIVATE
    ${COMMON_SOURCES}
    ${COROUTINE_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../RegistryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CoroutineTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CppWinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CppWinRT20Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CppWinRTAuthoringTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinVerifyTrustTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.manifest
    )
