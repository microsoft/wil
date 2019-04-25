cmake_minimum_required(VERSION 3.11)

# Set by build server to speed up build/reduce file/object size
option(WilCIBuild "WIL_CI_BUILD" OFF)

add_subdirectory(tests)
