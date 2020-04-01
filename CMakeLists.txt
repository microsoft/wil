cmake_minimum_required(VERSION 3.11)

project(WIL
    VERSION 0.0.0.0
    DESCRIPTION "The Windows Implementation Libraries (wil) were created to improve productivity and solve problems commonly seen by Windows developers."
    HOMEPAGE_URL "https://github.com/Microsoft/wil"
    LANGUAGES CXX)

# Set by build server to speed up build/reduce file/object size
option(FAST_BUILD "Sets options to speed up build/reduce obj/executable size" OFF)

# Detect the Windows SDK version. If we're using the Visual Studio generator, this will be provided for us. Otherwise
# we'll need to assume that this value comes from the command line (e.g. through the VS command prompt)
if (DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(WIL_WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
else()
    # This has a trailing backslash for whatever reason...
    string(REGEX REPLACE "\\\\$" "" WIL_WINDOWS_SDK_VERSION "$ENV{WindowsSDKVersion}")
endif()

add_library(WIL INTERFACE)

target_include_directories(WIL
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>")

install(
    DIRECTORY "include/"
    DESTINATION "include")

install(
    FILES "ThirdPartyNotices.txt"
    DESTINATION ".")

# Export the targets for consumption by other CMake projects

include(CMakePackageConfigHelpers)

configure_package_config_file(
        "cmake/WILConfig.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake/WILConfig.cmake"
    INSTALL_DESTINATION "cmake")

write_basic_package_version_file(
        "${CMAKE_BINARY_DIR}/cmake/WILConfigVersion.cmake"
    COMPATIBILITY ExactVersion ARCH_INDEPENDENT)

install(
    FILES
        "${CMAKE_BINARY_DIR}/cmake/WILConfig.cmake"
        "${CMAKE_BINARY_DIR}/cmake/WILConfigVersion.cmake"
    DESTINATION "cmake")

export(TARGETS WIL FILE "cmake/WILTargets.cmake")

install(TARGETS WIL EXPORT WILTargets)
install(EXPORT WILTargets DESTINATION cmake)

# Configure and build the NuGet package

set(CPACK_GENERATOR "NuGet")

set(CPACK_PACKAGE_NAME "Microsoft.Windows.ImplementationLibrary")
set(CPACK_PACKAGE_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "")
set(CPACK_PACKAGE_VENDOR "Microsoft")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

set(CPACK_NUGET_PACKAGE_LICENSEURL "https://github.com/microsoft/wil/blob/master/LICENSE")

set(CPACK_NUGET_PACKAGE_TITLE "Windows Implementation Library")
set(CPACK_NUGET_PACKAGE_COPYRIGHT "© Microsoft Corporation. All rights reserved.")
set(CPACK_NUGET_PACKAGE_TAGS "windows utility wil native")

include(CPack)

add_subdirectory(tests)
