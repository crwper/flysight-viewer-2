# PatchGTSAM.cmake
# Patches GeographicLib's CMakeLists.txt to disable the js subdirectory build.
# The js subdirectory causes build failures on Windows and must be commented out.
#
# Usage: cmake -DGTSAM_SOURCE_DIR=/path/to/gtsam -P cmake/PatchGTSAM.cmake
#
# This script is idempotent - running it multiple times will not corrupt the file.

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
    return()
endif()

# 5. Check if unpatched line exists
# NOTE: The exact syntax has a space before the opening parenthesis: "add_subdirectory (js)"
string(FIND "${FILE_CONTENT}" "add_subdirectory (js)" UNPATCHED_POS)
if(UNPATCHED_POS EQUAL -1)
    message(WARNING
        "Could not find 'add_subdirectory (js)' line in ${TARGET_FILE}.\n"
        "The file may have been modified in an unexpected way.")
    return()
endif()

# 6. Apply patch - comment out the add_subdirectory (js) line
string(REPLACE "add_subdirectory (js)" "#add_subdirectory (js)" FILE_CONTENT "${FILE_CONTENT}")

# 7. Write patched content back to file
file(WRITE "${TARGET_FILE}" "${FILE_CONTENT}")
message(STATUS "Successfully patched GeographicLib CMakeLists.txt - commented out 'add_subdirectory (js)'")
