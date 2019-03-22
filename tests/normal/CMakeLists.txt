
include(${CMAKE_SOURCE_DIR}/cmake/common_build_flags.cmake)

project(wi.UnitTests)
add_executable(wi.UnitTests)

target_sources(wi.UnitTests PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonTests.cpp)
