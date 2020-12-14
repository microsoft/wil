include(${PROJECT_SOURCE_DIR}/cmake/common_build_flags.cmake)
cmake_minimum_required(VERSION 3.11)

# All projects need to reference the WIL headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# TODO: Might be worth trying to conditionally do this on SDK version, assuming there's a semi-easy way to detect that
include_directories(BEFORE SYSTEM ./workarounds/wrl)

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
    )

add_subdirectory(app)
add_subdirectory(cpplatest)
add_subdirectory(noexcept)
add_subdirectory(normal)
add_subdirectory(win7)
