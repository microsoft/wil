
project(witest.win7)
add_executable(witest.win7)

add_definitions("-D_WIN32_WINNT=0x0601")

target_sources(witest.win7 PUBLIC
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TokenHelpersTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WatcherTests.cpp
    )
