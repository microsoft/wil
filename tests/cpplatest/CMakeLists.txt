
add_executable(witest.cpplatest)

target_precompile_headers(witest.cpplatest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

# Compilers often don't use the latest C++ standard as the default. Periodically update this value (possibly conditioned
# on compiler) as new standards are ratified/support is available
target_compile_features(witest.cpplatest PRIVATE cxx_std_23)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(witest.cpplatest PRIVATE
        -fno-ms-compatibility
        )
else()
    target_compile_options(witest.cpplatest PRIVATE
        /Zc:preprocessor
        )
endif()

target_sources(witest.cpplatest PRIVATE
    ${COMMON_SOURCES}
    ${DOWNLEVEL_SOURCES}
    ${WINRT_SOURCES}
    ${WIN11_DESKTOP_SOURCES}
    ${CPP20_SOURCES}
    )

# For things like intellisense in VS code and making clang-tidy not take an excruciatingly long time, only use this
# target for generating compile_commands.json, which should encompass the largest "surface area" of all targets.
set_target_properties(witest.cpplatest
    PROPERTIES
        EXPORT_COMPILE_COMMANDS ON
    )
