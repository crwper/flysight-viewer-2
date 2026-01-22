# PatchGTSAM.cmake
# Patches GeographicLib files to fix build issues:
#   1. CMakeLists.txt: Disables the js subdirectory build (fails on Windows)
#   2. Geoid.hpp: Fixes std::ios::streamoff to std::streamoff (fails on modern GCC)
#
# Usage: cmake -DGTSAM_SOURCE_DIR=/path/to/gtsam -P cmake/PatchGTSAM.cmake
#
# This script is idempotent - running it multiple times will not corrupt files.

# 1. Validate input - GTSAM_SOURCE_DIR must be defined
if(NOT DEFINED GTSAM_SOURCE_DIR)
    message(FATAL_ERROR
        "GTSAM_SOURCE_DIR must be defined.\n"
        "Usage: cmake -DGTSAM_SOURCE_DIR=/path/to/gtsam -P cmake/PatchGTSAM.cmake\n"
        "Example: cmake -DGTSAM_SOURCE_DIR=third-party/gtsam -P cmake/PatchGTSAM.cmake")
endif()

# 2. Build path and validate file exists
set(TARGET_FILE "${GTSAM_SOURCE_DIR}/gtsam/3rdparty/GeographicLib/CMakeLists.txt")
if(NOT EXISTS "${TARGET_FILE}")
    message(FATAL_ERROR
        "Target file not found: ${TARGET_FILE}\n"
        "Please verify that GTSAM_SOURCE_DIR points to a valid GTSAM source directory.")
endif()

# 3. Read file content
file(READ "${TARGET_FILE}" FILE_CONTENT)

# 4. Check if already patched (idempotency check)
# Look for the commented-out version of the line
string(FIND "${FILE_CONTENT}" "#add_subdirectory (js)" PATCHED_POS)
if(NOT PATCHED_POS EQUAL -1)
    message(STATUS "GeographicLib CMakeLists.txt is already patched - no changes needed.")
else()
    # 5. Check if unpatched line exists
    # NOTE: The exact syntax has a space before the opening parenthesis: "add_subdirectory (js)"
    string(FIND "${FILE_CONTENT}" "add_subdirectory (js)" UNPATCHED_POS)
    if(UNPATCHED_POS EQUAL -1)
        message(WARNING
            "Could not find 'add_subdirectory (js)' line in ${TARGET_FILE}.\n"
            "The file may have been modified in an unexpected way.")
    else()
        # 6. Apply patch - comment out the add_subdirectory (js) line
        string(REPLACE "add_subdirectory (js)" "#add_subdirectory (js)" FILE_CONTENT "${FILE_CONTENT}")

        # 7. Write patched content back to file
        file(WRITE "${TARGET_FILE}" "${FILE_CONTENT}")
        message(STATUS "Successfully patched GeographicLib CMakeLists.txt - commented out 'add_subdirectory (js)'")
    endif()
endif()

# =============================================================================
# Patch 2: Geoid.hpp - fix std::ios::streamoff -> std::streamoff
# =============================================================================
# Modern GCC (11.4.0+) rejects std::ios::streamoff as it's not valid standard C++.
# The correct type is std::streamoff (from <iosfwd>).

set(GEOID_HPP_FILE "${GTSAM_SOURCE_DIR}/gtsam/3rdparty/GeographicLib/include/GeographicLib/Geoid.hpp")

if(NOT EXISTS "${GEOID_HPP_FILE}")
    message(WARNING "Geoid.hpp not found at ${GEOID_HPP_FILE} - skipping streamoff patch")
else()
    file(READ "${GEOID_HPP_FILE}" GEOID_CONTENT)

    # Check for the incorrect form that needs patching
    string(FIND "${GEOID_CONTENT}" "std::ios::streamoff" INCORRECT_FORM_POS)

    if(INCORRECT_FORM_POS EQUAL -1)
        message(STATUS "Geoid.hpp does not contain 'std::ios::streamoff' - no changes needed.")
    else()
        # Found the incorrect form, apply patch
        string(REPLACE "std::ios::streamoff" "std::streamoff" GEOID_CONTENT "${GEOID_CONTENT}")
        file(WRITE "${GEOID_HPP_FILE}" "${GEOID_CONTENT}")
        message(STATUS "Successfully patched Geoid.hpp - replaced 'std::ios::streamoff' with 'std::streamoff'")
    endif()
endif()
