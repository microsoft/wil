cmake_minimum_required(VERSION 3.11)

# Set by build server to speed up build/reduce file/object size
option(FAST_BUILD "Sets options to speed up build/reduce obj/executable size" OFF)

if (NOT DEFINED WIL_BUILD_VERSION)
    set(WIL_BUILD_VERSION "0.0.0")
endif()

add_subdirectory(packaging)
add_subdirectory(tests)
