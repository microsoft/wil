
project(witest.app)
add_executable(witest.app)

add_definitions(-DWINAPI_FAMILY=WINAPI_FAMILY_PC_APP)

target_sources(witest.app PUBLIC
    ${COMMON_SOURCES}
    ${CMAKE_CURRENT_SOURCE_DIR}/../StlTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../UniqueWinRTEventTokenTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../WinRTTests.cpp
    )
