
# E.g. replace_cxx_flag("/W[0-4]", "/W4")
macro(replace_cxx_flag pattern text)
    foreach (flag
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)

        string(REGEX REPLACE "${pattern}" "${text}" ${flag} "${${flag}}")

    endforeach()
endmacro()

macro(append_cxx_flag text)
    foreach (flag
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)

        string(APPEND ${flag} " ${text}")

    endforeach()
endmacro()

# Fixup default compiler settings

# Be as strict as reasonably possible, since we want to support consumers using strict warning levels
replace_cxx_flag("/W[0-4]" "/W4")
append_cxx_flag("/WX")

# We want to be as conformant as possible, so tell MSVC to not be permissive
append_cxx_flag("/permissive-")

# wistd::function has padding due to alignment. This is expected
append_cxx_flag("/wd4324")

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    # Ignore a few Clang warnings
    # TODO: Revisit later to see if we want a source level fix!
    append_cxx_flag("-Wno-switch")
    append_cxx_flag("-Wno-invalid-noreturn")
    append_cxx_flag("-Wno-c++17-compat-mangling")
    append_cxx_flag("-Wno-missing-field-initializers")

    # For tests, we want to be able to test self assignment, so disable this warning
    append_cxx_flag("-Wno-self-assign-overloaded")
else()
    # Flags that are either ignored or unrecognized by clang-cl
    # TODO: Disabled for now due to some outstanding issues
    # append_cxx_flag("/experimental:preprocessor")

    # CRT headers are not yet /experimental:preprocessor clean, so work around the known issues
    # append_cxx_flag("/Wv:18")

    # TODO: This is working around a bug...
    # append_cxx_flag("/std:c++17")
    append_cxx_flag("/bigobj")
endif()
