cmake_minimum_required(VERSION 3.11)
project(WIL)

include(GNUInstallDirs)

# Set by build server to speed up build/reduce file/object size
option(FAST_BUILD "Sets options to speed up build/reduce obj/executable size" OFF)
option(WIL_BUILD_PACKAGING "Sets option to build the packaging, default on" On)
option(WIL_BUILD_TESTS "Sets option to build the unit tests, default on" On)

if (NOT DEFINED WIL_BUILD_VERSION)
    set(WIL_BUILD_VERSION "0.0.0")
endif()

# Detect the Windows SDK version. If we're using the Visual Studio generator, this will be provided for us. Otherwise
# we'll need to assume that this value comes from the command line (e.g. through the VS command prompt)
if (DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(WIL_WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
else()
    # This has a trailing backslash for whatever reason...
    string(REGEX REPLACE "\\\\$" "" WIL_WINDOWS_SDK_VERSION "$ENV{WindowsSDKVersion}")
endif()

if (${WIL_BUILD_PACKAGING})
    add_subdirectory(packaging)
endif()

if (${WIL_BUILD_TESTS})
    add_subdirectory(tests)
endif()

# Gather headers into an interface library.
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.h")
add_library(${PROJECT_NAME} INTERFACE)

# The interface's include directory.
target_include_directories(${PROJECT_NAME} INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Install Package Configuration
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}_targets)
install(EXPORT ${PROJECT_NAME}_targets
    NAMESPACE ${PROJECT_NAME}::
    FILE ${PROJECT_NAME}-config.cmake
    DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
)

# Install the headers at a standard cmake location.
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")