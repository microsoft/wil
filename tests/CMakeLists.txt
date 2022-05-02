include(${PROJECT_SOURCE_DIR}/cmake/common_build_flags.cmake)
cmake_minimum_required(VERSION 3.11)

# All projects need to reference the WIL headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# TODO: Might be worth trying to conditionally do this on SDK version, assuming there's a semi-easy way to detect that
include_directories(BEFORE SYSTEM ./workarounds/wrl)

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
    ${CPPWINRT} -input sdk -output include
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE ret)
if (NOT ret EQUAL 0)
    message(FATAL_ERROR "Failed to run cppwinrt.exe")
endif()

include_directories(BEFORE SYSTEM ${CMAKE_BINARY_DIR}/include)

# The build pipelines have limitations that local development environments do not, so turn a few knobs
if (${FAST_BUILD})
    replace_cxx_flag("/GR" "/GR-") # Disables RTTI
    add_definitions(-DCATCH_CONFIG_FAST_COMPILE -DWIL_FAST_BUILD)
endif()

set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommonTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FileSystemTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NTResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Rpc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SafeCastTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WistdTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wiTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../natvis/wil.natvis
    )

add_subdirectory(app)
add_subdirectory(cpplatest)
add_subdirectory(noexcept)
add_subdirectory(normal)
add_subdirectory(win7)
