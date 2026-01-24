# =============================================================================
# BoostDiscovery.cmake
# =============================================================================
#
# Cross-platform Boost path detection for FlySight Viewer.
#
# This module sets BOOST_ROOT and BOOST_LIBRARYDIR to platform-appropriate
# defaults if they are not already defined in the cache.
#
# Usage:
#   include(cmake/BoostDiscovery.cmake)
#
# After including, BOOST_ROOT will be set as a cache variable that can be
# overridden via -DBOOST_ROOT=/path/to/boost on the command line.
#
# Supported Platforms:
#   - Windows: C:/Program Files/Boost/boost_1_87_0 (or GitHub Actions toolcache)
#   - macOS ARM64: /opt/homebrew
#   - macOS Intel: /usr/local
#   - Linux: /usr (system packages)
#
# =============================================================================

# Guard against multiple inclusion
if(_BOOST_DISCOVERY_INCLUDED)
    return()
endif()
set(_BOOST_DISCOVERY_INCLUDED TRUE)

# Only compute defaults if BOOST_ROOT is not already in the cache
if(NOT DEFINED CACHE{BOOST_ROOT})
    if(WIN32)
        # Windows: Standard Boost installer location
        # GitHub Actions toolcache uses C:\hostedtoolcache\windows\Boost\VERSION\x64
        # Local development typically uses C:\Program Files\Boost\boost_VERSION
        set(_BOOST_ROOT_DEFAULT "C:/Program Files/Boost/boost_1_87_0")
    elseif(APPLE)
        # macOS: Homebrew location depends on architecture
        execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE _MACOS_ARCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(_MACOS_ARCH STREQUAL "arm64")
            # Apple Silicon
            set(_BOOST_ROOT_DEFAULT "/opt/homebrew")
        else()
            # Intel Mac
            set(_BOOST_ROOT_DEFAULT "/usr/local")
        endif()
    else()
        # Linux: System packages from apt-get install libboost-all-dev
        set(_BOOST_ROOT_DEFAULT "/usr")
    endif()

    set(BOOST_ROOT "${_BOOST_ROOT_DEFAULT}"
        CACHE PATH "Boost root directory")
    message(STATUS "Boost: Using default BOOST_ROOT=${BOOST_ROOT}")
else()
    message(STATUS "Boost: Using cached BOOST_ROOT=${BOOST_ROOT}")
endif()

# Set BOOST_LIBRARYDIR for Windows if not already set
# On Unix systems, FindBoost handles multiarch library paths automatically
if(WIN32 AND NOT DEFINED CACHE{BOOST_LIBRARYDIR})
    # Check common Windows library directory patterns
    if(EXISTS "${BOOST_ROOT}/lib64-msvc-14.3")
        set(_BOOST_LIBRARYDIR_DEFAULT "${BOOST_ROOT}/lib64-msvc-14.3")
    elseif(EXISTS "${BOOST_ROOT}/stage/lib")
        set(_BOOST_LIBRARYDIR_DEFAULT "${BOOST_ROOT}/stage/lib")
    else()
        set(_BOOST_LIBRARYDIR_DEFAULT "${BOOST_ROOT}/lib")
    endif()

    set(BOOST_LIBRARYDIR "${_BOOST_LIBRARYDIR_DEFAULT}"
        CACHE PATH "Boost library directory")
    message(STATUS "Boost: Using default BOOST_LIBRARYDIR=${BOOST_LIBRARYDIR}")
endif()

# Helpful diagnostic output
if(BOOST_LIBRARYDIR)
    message(STATUS "Boost: Will search in BOOST_ROOT=${BOOST_ROOT}, BOOST_LIBRARYDIR=${BOOST_LIBRARYDIR}")
else()
    message(STATUS "Boost: Will search in BOOST_ROOT=${BOOST_ROOT}")
endif()
