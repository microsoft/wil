
include(${CMAKE_SOURCE_DIR}/cmake/common_build_flags.cmake)

# Compilers often don't use the latest C++ standard as the default. Periodically update this value (possibly conditioned
# on compiler) as new standards are ratified/support is available
set(CMAKE_CXX_STANDARD 17)

project(witest.cpplatest)
add_executable(witest.cpplatest)

target_sources(witest.cpplatest PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ComTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../FileSystemTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ResourceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SafeCastTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WistdTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../wiTest.cpp
    )
