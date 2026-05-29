
# witest.zsvsafe -- mirrors witest but builds with WIL_ZSV_DEFAULT_TO_EMPTY_BUFFER defined,
# exercising the opt-in safe default-ctor behaviour for wil::basic_zstring_view.
add_executable(witest.zsvsafe)

target_precompile_headers(witest.zsvsafe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

target_compile_definitions(witest.zsvsafe PRIVATE
    -DWIL_ZSV_DEFAULT_TO_EMPTY_BUFFER
    )

target_sources(witest.zsvsafe PRIVATE
    ${COMMON_SOURCES}
    ${DOWNLEVEL_SOURCES}
    ${WINRT_SOURCES}
    ${WIN11_DESKTOP_SOURCES}
    )
