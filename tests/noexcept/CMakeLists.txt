
include(${CMAKE_SOURCE_DIR}/cmake/common_build_flags.cmake)

project(witest.noexcept)
add_executable(witest.noexcept)

# Turn off exceptions for this test
replace_cxx_flag("/EHsc" "")

target_sources(witest.noexcept PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CommonTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ComTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../FileSystemTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ResourceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SafeCastTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WistdTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../wiTest.cpp)
