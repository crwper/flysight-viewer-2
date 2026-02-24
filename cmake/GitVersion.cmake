# =============================================================================
# GitVersion.cmake
# =============================================================================
#
# Detects the project version from Git tags.
#
# Tag format: v<major>.<minor>.<patch> (e.g., v1.2.3)
#
# Uses `git describe --tags --match "v[0-9]*"` to find the most recent
# version tag. If no tags are found, or git is not available, falls back
# to a default version of 0.0.0.
#
# Override behavior:
#   -DFLYSIGHT_VERSION_OVERRIDE=1.2.3   Bypass git detection entirely
#
# After inclusion, the following variables are set:
#   PROJECT_VERSION            - Clean X.Y.Z (for CMake, plist, CPack metadata)
#   PROJECT_VERSION_MAJOR/MINOR/PATCH
#   FLYSIGHT_VERSION_DISPLAY   - Full descriptor for display and filenames:
#                                "1.2.3" on a tag, "1.2.3-47-gabcdef" between
#                                tags, "0.0.0-gabcdef" with no tags
#
# Usage:
#   project(FlySightViewer VERSION 0.0.0 LANGUAGES CXX)
#   include(cmake/GitVersion.cmake)
#
# =============================================================================

if(_GIT_VERSION_INCLUDED)
    return()
endif()
set(_GIT_VERSION_INCLUDED TRUE)

set(_FLYSIGHT_FALLBACK_VERSION "0.0.0")

set(FLYSIGHT_VERSION_OVERRIDE "" CACHE STRING
    "Override version (e.g., '1.2.3'). Bypasses git detection when set.")

if(FLYSIGHT_VERSION_OVERRIDE)
    set(_detected_version "${FLYSIGHT_VERSION_OVERRIDE}")
    set(_display_version "${FLYSIGHT_VERSION_OVERRIDE}")
    set(_version_source "override")
else()
    find_package(Git QUIET)

    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE _git_describe_output
            ERROR_VARIABLE _git_describe_error
            RESULT_VARIABLE _git_describe_result
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(_git_describe_result EQUAL 0 AND _git_describe_output)
            # Parse the clean X.Y.Z from the tag portion
            string(REGEX MATCH "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)" _version_match "${_git_describe_output}")

            if(_version_match)
                set(_detected_version "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
                # Display version is the full git describe output minus the 'v' prefix
                string(REGEX REPLACE "^v" "" _display_version "${_git_describe_output}")
                set(_version_source "git tag (${_git_describe_output})")
            else()
                set(_detected_version "${_FLYSIGHT_FALLBACK_VERSION}")
                set(_display_version "${_FLYSIGHT_FALLBACK_VERSION}")
                set(_version_source "fallback (git tag '${_git_describe_output}' did not match vX.Y.Z)")
            endif()
        else()
            # No version tags found — get the short commit hash for identification
            set(_detected_version "${_FLYSIGHT_FALLBACK_VERSION}")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE _git_short_hash
                ERROR_QUIET
                RESULT_VARIABLE _git_hash_result
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            if(_git_hash_result EQUAL 0 AND _git_short_hash)
                set(_display_version "${_FLYSIGHT_FALLBACK_VERSION}-g${_git_short_hash}")
            else()
                set(_display_version "${_FLYSIGHT_FALLBACK_VERSION}")
            endif()
            set(_version_source "fallback (no version tags found)")
        endif()
    else()
        set(_detected_version "${_FLYSIGHT_FALLBACK_VERSION}")
        set(_display_version "${_FLYSIGHT_FALLBACK_VERSION}")
        set(_version_source "fallback (git not found)")
    endif()
endif()

# Parse version components
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _parsed "${_detected_version}")
if(_parsed)
    set(_version_major "${CMAKE_MATCH_1}")
    set(_version_minor "${CMAKE_MATCH_2}")
    set(_version_patch "${CMAKE_MATCH_3}")
else()
    message(WARNING "GitVersion: Could not parse version '${_detected_version}', using fallback")
    set(_detected_version "${_FLYSIGHT_FALLBACK_VERSION}")
    set(_display_version "${_FLYSIGHT_FALLBACK_VERSION}")
    set(_version_major "0")
    set(_version_minor "0")
    set(_version_patch "0")
    set(_version_source "fallback (parse failure)")
endif()

# Override PROJECT_VERSION variables set by project()
set(PROJECT_VERSION "${_detected_version}")
set(PROJECT_VERSION_MAJOR "${_version_major}")
set(PROJECT_VERSION_MINOR "${_version_minor}")
set(PROJECT_VERSION_PATCH "${_version_patch}")
set(${PROJECT_NAME}_VERSION "${_detected_version}")

# Display version for About dialog, package filenames, etc.
set(FLYSIGHT_VERSION_DISPLAY "${_display_version}")

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Version Configuration")
message(STATUS "========================================")
message(STATUS "  Version: ${PROJECT_VERSION}")
if(NOT "${FLYSIGHT_VERSION_DISPLAY}" STREQUAL "${PROJECT_VERSION}")
    message(STATUS "  Display: ${FLYSIGHT_VERSION_DISPLAY}")
endif()
message(STATUS "  Source:  ${_version_source}")
message(STATUS "========================================")
message(STATUS "")
