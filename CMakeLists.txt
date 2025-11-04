cmake_minimum_required(VERSION 3.29)
project(WIL)

include(GNUInstallDirs)

# Set by build server to speed up build/reduce file/object size
option(FAST_BUILD "Sets options to speed up build/reduce obj/executable size" OFF)
option(WIL_BUILD_PACKAGING "Sets option to build the packaging, default on" ON)
option(WIL_BUILD_TESTS "Sets option to build the unit tests, default on" ON)
option(WIL_ENABLE_ASAN "Enables building the test executable that has Address Sanitizer enabled" OFF)
option(WIL_ENABLE_UBSAN "Enables building the test executable that has Undefined Behavior Sanitizer enabled" OFF)

if (NOT DEFINED WIL_BUILD_VERSION)
    set(WIL_BUILD_VERSION "0.0.0")
endif()

if (NOT DEFINED CPPWINRT_VERSION)
    set(CPPWINRT_VERSION "2.0.240405.15")
endif()

if (${WIL_BUILD_PACKAGING})
    add_subdirectory(packaging)
endif()

if (${WIL_BUILD_TESTS})
    add_subdirectory(docs)
    add_subdirectory(tests)

    enable_testing()

    # Custom target for running clang-format
    add_custom_target(format
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND scripts/run-clang-format.cmd)

    # Custom target for running clang-tidy. Since Clang's tooling does not understand MSVC command lines, only enable
    # when targeting Clang
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_custom_target(run-clang-tidy
            DEPENDS witest.cpplatest # Clang needs the pch, otherwise it fails
            USES_TERMINAL # Otherwise, we don't get any output until the command is done, which takes _a while_
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMAND clang-tidy -p ${PROJECT_BINARY_DIR}/compile_commands.json tests/*.cpp
            )
    endif()
endif()

# Gather headers into an interface library.
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.h")
add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# The interface's include directory.
target_include_directories(${PROJECT_NAME} INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Include the .natvis files
if (MSVC)
    target_sources(${PROJECT_NAME} INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/natvis/wil.natvis>")
endif()

# Install Package Configuration
string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWER)
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME_LOWER}_targets)
install(EXPORT ${PROJECT_NAME_LOWER}_targets
    NAMESPACE ${PROJECT_NAME}::
    FILE ${PROJECT_NAME_LOWER}Config.cmake
    DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
)

# Install the headers at a standard cmake location.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/wil" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
