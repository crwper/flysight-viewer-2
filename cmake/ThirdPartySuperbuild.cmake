# =============================================================================
# ThirdPartySuperbuild.cmake
# =============================================================================
#
# This file defines ExternalProject targets for the three major third-party
# dependencies: oneTBB, GTSAM, and KDDockWidgets. It implements a superbuild
# pattern that builds these libraries in the correct order with proper
# dependency handling.
#
# This replaces the Windows-specific BUILD.BAT with a portable, cross-platform
# CMake solution.
#
# Build Order and Dependencies:
#   1. oneTBB     - No dependencies, builds first
#   2. GTSAM      - Depends on oneTBB (for TBB integration)
#   3. KDDockWidgets - Independent, can build in parallel with GTSAM
#
# Each library is built in both Debug and Release configurations, with
# outputs installed to separate install directories.
#
# =============================================================================

# =============================================================================
# Generator Detection and Configuration
# =============================================================================
#
# Detect the generator type to configure ExternalProject arguments correctly.
# Visual Studio generators require the -A (architecture) argument, while
# other generators (Ninja, Makefiles) do not.
#

get_property(_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

# Generator settings for ExternalProject
# Note: We use CMAKE_GENERATOR and CMAKE_GENERATOR_PLATFORM as ExternalProject
# parameters rather than passing -G/-A via CMAKE_ARGS. This avoids the
# "Multiple -A options not allowed" error that occurs when both the parent
# project's platform and an explicit -A argument are passed.

# =============================================================================
# Source Directory Variables
# =============================================================================
#
# Define paths to the third-party source directories (submodules).
# These use THIRD_PARTY_DIR which is defined in the root CMakeLists.txt.
#

set(ONETBB_SOURCE_DIR "${THIRD_PARTY_DIR}/oneTBB")
set(GTSAM_SOURCE_DIR "${THIRD_PARTY_DIR}/gtsam")
set(KDDW_SOURCE_DIR "${THIRD_PARTY_DIR}/KDDockWidgets")

# Verify source directories exist
foreach(_src_dir ONETBB_SOURCE_DIR GTSAM_SOURCE_DIR KDDW_SOURCE_DIR)
    if(NOT EXISTS "${${_src_dir}}")
        message(WARNING "Third-party source directory not found: ${${_src_dir}}")
        message(WARNING "Please ensure git submodules are initialized: git submodule update --init --recursive")
    endif()
endforeach()

# =============================================================================
# ext_oneTBB - Intel Threading Building Blocks
# =============================================================================
#
# oneTBB provides parallel algorithms and data structures used by GTSAM.
# We build it as a shared library (BUILD_SHARED_LIBS=ON) because:
#   1. oneTBB explicitly warns against static builds (see oneTBB/CMakeLists.txt:145)
#   2. Shared builds avoid issues with TBB's thread-local storage
#
# Key options:
#   - BUILD_SHARED_LIBS=ON  : Build as shared library (recommended by TBB)
#   - TBB_TEST=OFF          : Skip test builds
#   - TBB_EXAMPLES=OFF      : Skip example builds
#   - TBB_STRICT=OFF        : Disable warnings-as-errors to avoid build failures
#

ExternalProject_Add(ext_oneTBB
    SOURCE_DIR "${ONETBB_SOURCE_DIR}"
    BINARY_DIR "${THIRD_PARTY_DIR}/oneTBB-build"
    INSTALL_DIR "${ONETBB_INSTALL_DIR}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
    CMAKE_ARGS
        -DBUILD_SHARED_LIBS=ON
        -DTBB_TEST=OFF
        -DTBB_EXAMPLES=OFF
        -DTBB_STRICT=OFF
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
        COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Debug
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release --prefix <INSTALL_DIR>
        COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Debug --prefix <INSTALL_DIR>
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
)

# =============================================================================
# ext_GTSAM - Georgia Tech Smoothing and Mapping
# =============================================================================
#
# GTSAM is a factor graph optimization library used for GPS trajectory smoothing.
# It integrates with TBB for parallel computations.
#
# Key options:
#   - GTSAM_INSTALL_GEOGRAPHICLIB=ON : Install bundled GeographicLib
#   - GTSAM_BUILD_EXAMPLES_ALWAYS=OFF : Skip examples
#   - GTSAM_BUILD_TESTS=OFF          : Skip tests
#   - GTSAM_WITH_TBB=ON              : Enable TBB integration
#   - TBB_DIR                        : Path to oneTBB's CMake config
#
# PATCH_COMMAND: Invokes PatchGTSAM.cmake to comment out the problematic
#   'add_subdirectory (js)' line in GeographicLib's CMakeLists.txt, which
#   causes build failures on Windows.
#

# Path to TBB's CMake configuration directory
set(TBB_CMAKE_DIR "${ONETBB_INSTALL_DIR}/lib/cmake/TBB")

# Configure the patch command for GTSAM
# The patch script comments out the js subdirectory in GeographicLib
# When included from third-party/CMakeLists.txt, FLYSIGHT_CMAKE_DIR points to cmake/
# When included from root CMakeLists.txt, we use CMAKE_CURRENT_SOURCE_DIR/cmake/
if(DEFINED FLYSIGHT_CMAKE_DIR)
    set(_GTSAM_PATCH_SCRIPT "${FLYSIGHT_CMAKE_DIR}/PatchGTSAM.cmake")
else()
    set(_GTSAM_PATCH_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PatchGTSAM.cmake")
endif()
if(EXISTS "${_GTSAM_PATCH_SCRIPT}")
    set(_GTSAM_PATCH_COMMAND ${CMAKE_COMMAND} -DGTSAM_SOURCE_DIR=${GTSAM_SOURCE_DIR} -P "${_GTSAM_PATCH_SCRIPT}")
else()
    message(WARNING "PatchGTSAM.cmake not found at ${_GTSAM_PATCH_SCRIPT}")
    message(WARNING "GTSAM build may fail due to GeographicLib js subdirectory issue.")
    set(_GTSAM_PATCH_COMMAND "")
endif()

ExternalProject_Add(ext_GTSAM
    SOURCE_DIR "${GTSAM_SOURCE_DIR}"
    BINARY_DIR "${THIRD_PARTY_DIR}/gtsam-build"
    INSTALL_DIR "${GTSAM_INSTALL_DIR}"
    DEPENDS ext_oneTBB
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
    CMAKE_ARGS
        -DGTSAM_INSTALL_GEOGRAPHICLIB=ON
        -DGTSAM_BUILD_EXAMPLES_ALWAYS=OFF
        -DGTSAM_BUILD_TESTS=OFF
        -DGTSAM_WITH_TBB=ON
        -DTBB_DIR=${TBB_CMAKE_DIR}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    PATCH_COMMAND ${_GTSAM_PATCH_COMMAND}
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
        COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Debug
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release --prefix <INSTALL_DIR>
        COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Debug --prefix <INSTALL_DIR>
    UPDATE_COMMAND ""
)

# =============================================================================
# ext_KDDockWidgets - KDE Docking Widget Framework
# =============================================================================
#
# KDDockWidgets provides advanced docking functionality for Qt applications.
# It has no dependencies on the other third-party libraries.
#
# Key options:
#   - KDDockWidgets_QT6=ON       : Build for Qt6
#   - KDDockWidgets_EXAMPLES=OFF : Skip example builds
#   - KDDockWidgets_TESTS=OFF    : Skip test builds
#   - KDDockWidgets_DOCS=OFF     : Skip documentation generation
#
# Note: Uses an out-of-source build directory matching the pattern from
#   BUILD.BAT (builds in third-party/KDDockWidgets-build, not in the
#   source tree). This avoids potential issues with in-source builds.
#

set(KDDW_BINARY_DIR "${THIRD_PARTY_DIR}/KDDockWidgets-build")

ExternalProject_Add(ext_KDDockWidgets
    SOURCE_DIR "${KDDW_SOURCE_DIR}"
    BINARY_DIR "${KDDW_BINARY_DIR}"
    INSTALL_DIR "${KDDW_INSTALL_DIR}"
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
    CMAKE_ARGS
        -DKDDockWidgets_QT6=ON
        -DKDDockWidgets_EXAMPLES=OFF
        -DKDDockWidgets_TESTS=OFF
        -DKDDockWidgets_DOCS=OFF
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    BUILD_COMMAND
        ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
        COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Debug
    INSTALL_COMMAND
        ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release --prefix <INSTALL_DIR>
        COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Debug --prefix <INSTALL_DIR>
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
)

# =============================================================================
# Clean Targets
# =============================================================================
#
# Convenience targets to clean individual third-party builds or all of them.
# These replicate the functionality of CLEAN.BAT in a cross-platform way.
#
# Usage:
#   cmake --build build --target clean-oneTBB
#   cmake --build build --target clean-GTSAM
#   cmake --build build --target clean-KDDockWidgets
#   cmake --build build --target clean-third-party   (cleans all)
#

add_custom_target(clean-oneTBB
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${THIRD_PARTY_DIR}/oneTBB-build"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${ONETBB_INSTALL_DIR}"
    COMMENT "Cleaning oneTBB build and install directories..."
)

add_custom_target(clean-GTSAM
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${THIRD_PARTY_DIR}/gtsam-build"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${GTSAM_INSTALL_DIR}"
    COMMENT "Cleaning GTSAM build and install directories..."
)

add_custom_target(clean-KDDockWidgets
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${KDDW_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${KDDW_INSTALL_DIR}"
    COMMENT "Cleaning KDDockWidgets build and install directories..."
)

add_custom_target(clean-third-party
    DEPENDS clean-oneTBB clean-GTSAM clean-KDDockWidgets
    COMMENT "Cleaning all third-party build and install directories..."
)

# =============================================================================
# Build Status Summary
# =============================================================================
#
# Display information about the configured third-party builds.
#

message(STATUS "")
message(STATUS "----------------------------------------")
message(STATUS "Third-Party Superbuild Configuration")
message(STATUS "----------------------------------------")
message(STATUS "")
message(STATUS "ExternalProject Targets:")
message(STATUS "  ext_oneTBB        -> ${ONETBB_INSTALL_DIR}")
message(STATUS "  ext_GTSAM         -> ${GTSAM_INSTALL_DIR}")
message(STATUS "  ext_KDDockWidgets -> ${KDDW_INSTALL_DIR}")
message(STATUS "")
message(STATUS "Dependency Graph:")
message(STATUS "  ext_oneTBB       : (no dependencies)")
message(STATUS "  ext_GTSAM        : depends on ext_oneTBB")
message(STATUS "  ext_KDDockWidgets: (no dependencies)")
message(STATUS "")
message(STATUS "Build Order:")
message(STATUS "  1. ext_oneTBB (and ext_KDDockWidgets in parallel)")
message(STATUS "  2. ext_GTSAM (after ext_oneTBB completes)")
message(STATUS "")
message(STATUS "Clean Targets Available:")
message(STATUS "  clean-oneTBB, clean-GTSAM, clean-KDDockWidgets, clean-third-party")
message(STATUS "")
message(STATUS "----------------------------------------")
message(STATUS "")
