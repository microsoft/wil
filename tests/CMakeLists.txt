
include(${PROJECT_SOURCE_DIR}/cmake/common_build_flags.cmake)

# All projects need to reference the WIL headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# Because we don't always use msbuild, we need to run nuget manually
find_program(NUGET nuget)
if (NOT NUGET)
    message(FATAL_ERROR "Unable to find the nuget CLI tool. Please install it from https://www.nuget.org/downloads and ensure it has been added to the PATH")
endif()

execute_process(COMMAND
    ${NUGET} install Microsoft.Windows.CppWinRT -Version ${CPPWINRT_VERSION} -OutputDirectory packages
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE ret)
if (NOT ret EQUAL 0)
    message(FATAL_ERROR "Failed to install nuget package Microsoft.Windows.CppWinRT.${CPPWINRT_VERSION}")
endif()

set(CPPWINRT ${CMAKE_BINARY_DIR}/packages/Microsoft.Windows.CppWinRT.${CPPWINRT_VERSION}/bin/cppwinrt.exe)
execute_process(COMMAND
    ${CPPWINRT} -input sdk+ -output include
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE ret)
if (NOT ret EQUAL 0)
    message(FATAL_ERROR "Failed to run cppwinrt.exe")
endif()

# In general, we've supported C++14 as the minimum standard, however the C++17 standard is already a couple releases old
# and the Windows OS build has been on C++17 for some time. For the time being, we will still try and guard uses of
# C++17+ features, however we've started to take dependencies on C++17 features in the tests and therefore no longer
# build them with older standards
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Turn on CTest support
enable_testing()

include_directories(BEFORE SYSTEM ${CMAKE_BINARY_DIR}/include)

# The build pipelines have limitations that local development environments do not, so turn a few knobs
if (${FAST_BUILD})
    if (MSVC)
        replace_cxx_flag("/GR" "/GR-") # Disables RTTI
    else()
        add_compile_options(-fno-rtti)
    endif()

    add_definitions(-DCATCH_CONFIG_FAST_COMPILE -DWIL_FAST_BUILD)
endif()

# For some unknown reason, 'RelWithDebInfo' compiles with '/Ob1' as opposed to '/Ob2' which prevents inlining of
# functions not marked 'inline'. The reason we prefer 'RelWithDebInfo' over 'Release' is to get debug info, so manually
# revert to the desired (and default) inlining behavior as that exercises more interesting code paths
if (MSVC AND "${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    # TODO: This is currently blocked by an apparent Clang bug: https://github.com/llvm/llvm-project/issues/59690
    # replace_cxx_flag("/Ob1" "/Ob2")
endif()

set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CommonTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FileSystemTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MockingTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NTResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Rpc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SafeCastTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TraceLoggingTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TraceLoggingTests_PartB.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WindowingTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WistdTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wiTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../natvis/wil.natvis
    )

# Source files that can be compiled downlevel to at least Win7
set(DOWNLEVEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/NetworkTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/RegistryTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WatcherTests.cpp
    )

# Source files that require WinRT (Win8+ & can compile with App partition)
set(WINRT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WinRTTests.cpp
    )

# Source files that can compile on Win11+, but only on the Desktop partition
set(WIN11_DESKTOP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/WinVerifyTrustTest.cpp
    )

# Source files that require C++20
set(CPP20_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CoroutineTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CppWinRT20Tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CppWinRTAuthoringTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CppWinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/app.manifest
    )

# It appears as though Clang has some issues with exception handling inside of coroutines, which causes issues when
# trying to run the ComApartmentVariableTests.cpp tests, so disable on Clang for now
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CPP20_SOURCES
        ${CPP20_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/ComApartmentVariableTests.cpp
        )
endif()

if (MSVC)
    add_link_options(/NATVIS:${CMAKE_SOURCE_DIR}/natvis/wil.natvis)
endif()

find_path(DETOURS_INCLUDE_DIRS detours/detours.h)
find_library(DETOURS_LIBRARY detours REQUIRED)

find_package(Catch2 CONFIG REQUIRED)

include_directories(${DETOURS_INCLUDE_DIRS})
add_definitions(-DNOMINMAX)

# We build the 'app' tests just to ensure various things can build for the App partition. We don't actually run them in
# an app container or anything. By building 'main.cpp' separately and linking to it, we allow the app tests to get the
# same callstack printing logic as the rest of the tests.
add_library(witest.main STATIC)
target_sources(witest.main
    PRIVATE
        main.cpp
    )
target_link_libraries(witest.main
    INTERFACE
        ${DETOURS_LIBRARY}
        Catch2::Catch2
        ntdll.lib
        dbghelp.lib
        ws2_32.lib
    )

link_libraries(witest.main)

add_subdirectory(app)
add_subdirectory(cpplatest)
add_subdirectory(cppwinrt-notifiable-server-lock)
add_subdirectory(noexcept)
add_subdirectory(normal)
add_subdirectory(win7)

add_test(NAME app COMMAND $<TARGET_FILE:witest.app>)
add_test(NAME cpplatest COMMAND $<TARGET_FILE:witest.cpplatest>)
add_test(NAME cppwinrt-notifiable-server-lock COMMAND $<TARGET_FILE:witest.cppwinrt-notifiable-server-lock>)
add_test(NAME noexcept COMMAND $<TARGET_FILE:witest.noexcept>)
add_test(NAME normal COMMAND $<TARGET_FILE:witest>)
add_test(NAME win7 COMMAND $<TARGET_FILE:witest.win7>)

if (${WIL_ENABLE_ASAN})
    add_subdirectory(sanitize-address)
    add_test(NAME witest.asan COMMAND $<TARGET_FILE:witest.asan>)
endif()

if (${WIL_ENABLE_UBSAN})
    # TODO: Disabled until https://github.com/microsoft/STL/issues/3568 is resolved
    # add_subdirectory(sanitize-undefined-behavior)
endif()
