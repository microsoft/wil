
add_executable(witest)

target_precompile_headers(witest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

target_sources(witest PRIVATE
    ${COMMON_SOURCES}
    ${DOWNLEVEL_SOURCES}
    ${WINRT_SOURCES}
    ${WIN11_DESKTOP_SOURCES}
    )
