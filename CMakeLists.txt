cmake_minimum_required(VERSION 3.11)

# Set by build server to speed up build/reduce file/object size
option(WIL_CI_BUILD "Sets options to speed up build/reduce obj/executable size" OFF)

add_subdirectory(tests)
