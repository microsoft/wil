
# If doxygen can't be found, don't error out as this is not a hard requirement. We will instead create a dummy target that prints
# an error message to the user
find_program(DOXYGEN doxygen)

if (DOXYGEN)
    # Add a custom target to generate the documentation
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E env "DOXYGEN_OUTPUT_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}" "WIL_BUILD_VERSION=${WIL_BUILD_VERSION}" ${DOXYGEN} Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
else()
    message(WARNING "Doxygen not found. The 'docs' target will not be available. If you would like to generate documentation, "
        "please install Doxygen and ensure that it is available in your PATH and re-run CMake.")
endif()
