
# All projects need to reference the WIL headers
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(app)
add_subdirectory(cpplatest)
add_subdirectory(noexcept)
add_subdirectory(normal)
