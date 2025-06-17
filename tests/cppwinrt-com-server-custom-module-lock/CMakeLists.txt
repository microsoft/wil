
add_executable(witest.cppwinrt-com-server-custom-module-lock)

target_precompile_headers(witest.cppwinrt-com-server-custom-module-lock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../pch.h)

# Compilers often don't use the latest C++ standard as the default. Periodically update this value (possibly conditioned
# on compiler) as new standards are ratified/support is available
target_compile_features(witest.cppwinrt-com-server-custom-module-lock PRIVATE cxx_std_20)

target_sources(witest.cppwinrt-com-server-custom-module-lock PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CppWinRTComServerCustomModuleLockTests.cpp
    )
