
add_executable(witest.app)

target_precompile_headers(witest.app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

target_compile_definitions(witest.app PRIVATE
    -DWINAPI_FAMILY=WINAPI_FAMILY_PC_APP
    )

target_sources(witest.app PRIVATE
    ${COMMON_SOURCES}
    ${WINRT_SOURCES}
    )
