# =============================================================================
# FlySight Viewer - Superbuild CMakeLists.txt
# =============================================================================
#
# This is the root CMakeLists.txt for the FlySight Viewer superbuild system.
# It orchestrates building all third-party dependencies and the main application
# using CMake's ExternalProject module for build isolation.
#
# Usage Examples:
#   # Build everything (third-party + application):
#   cmake -G "Visual Studio 17 2022" -A x64 -B build -S .
#   cmake --build build --config Release
#
#   # Build only third-party dependencies:
#   cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DFLYSIGHT_THIRD_PARTY_ONLY=ON
#   cmake --build build --config Release
#
#   # Skip third-party (use pre-built dependencies):
#   cmake -G "Visual Studio 17 2022" -A x64 -B build -S . -DFLYSIGHT_BUILD_THIRD_PARTY=OFF
#   cmake --build build --config Release
#
# =============================================================================

cmake_minimum_required(VERSION 3.18)

project(FlySightViewer-Superbuild LANGUAGES C CXX)

# Include the ExternalProject module for managing external builds
include(ExternalProject)

# =============================================================================
# Generator Configuration
# =============================================================================

# Provide information about the expected generator for Windows development
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    message(STATUS "Using Visual Studio generator: ${CMAKE_GENERATOR}")
    if(CMAKE_GENERATOR_PLATFORM)
        message(STATUS "Target platform: ${CMAKE_GENERATOR_PLATFORM}")
    endif()
else()
    message(STATUS "Using generator: ${CMAKE_GENERATOR}")
    message(STATUS "Note: Visual Studio 2022 is the primary generator for Windows development")
endif()

# =============================================================================
# Build Options
# =============================================================================
#
# These options control which parts of the build system are active.
#
# FLYSIGHT_BUILD_THIRD_PARTY: Build third-party dependencies (oneTBB, GTSAM, KDDockWidgets)
#   - Set to OFF to use pre-built dependencies from third-party/*-install directories
#
# FLYSIGHT_BUILD_APP: Build the main FlySight Viewer application
#   - Set to OFF to skip building the application (useful for dependency builds only)
#
# FLYSIGHT_THIRD_PARTY_ONLY: Convenience option to build only third-party dependencies
#   - When ON, automatically disables FLYSIGHT_BUILD_APP
#   - Equivalent to: -DFLYSIGHT_BUILD_THIRD_PARTY=ON -DFLYSIGHT_BUILD_APP=OFF
#

option(FLYSIGHT_BUILD_THIRD_PARTY "Build third-party dependencies" ON)
option(FLYSIGHT_BUILD_APP "Build the main FlySight Viewer application" ON)
option(FLYSIGHT_THIRD_PARTY_ONLY "Build only third-party dependencies (skip application)" OFF)

# Handle option interaction: THIRD_PARTY_ONLY implies no app build
if(FLYSIGHT_THIRD_PARTY_ONLY)
    set(FLYSIGHT_BUILD_APP OFF)
    message(STATUS "FLYSIGHT_THIRD_PARTY_ONLY is ON - application build disabled")
endif()

# =============================================================================
# Path Variables for Third-Party Dependencies
# =============================================================================
#
# These paths define where third-party dependencies are located and installed.
# They match the existing directory structure created by BUILD.BAT.
# Users can override these via CMake cache or command line.
#

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party"
    CACHE PATH "Root directory for third-party dependencies")

set(ONETBB_INSTALL_DIR "${THIRD_PARTY_DIR}/oneTBB-install"
    CACHE PATH "oneTBB installation directory")

set(GTSAM_INSTALL_DIR "${THIRD_PARTY_DIR}/GTSAM-install"
    CACHE PATH "GTSAM installation directory")

set(KDDW_INSTALL_DIR "${THIRD_PARTY_DIR}/KDDockWidgets-install"
    CACHE PATH "KDDockWidgets installation directory")

# =============================================================================
# Third-Party Superbuild
# =============================================================================
#
# Include the third-party build definitions if enabled.
# The ThirdPartySuperbuild.cmake file defines ExternalProject_Add calls
# for oneTBB, GTSAM, and KDDockWidgets.
#

if(FLYSIGHT_BUILD_THIRD_PARTY)
    set(_THIRD_PARTY_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ThirdPartySuperbuild.cmake")
    if(EXISTS "${_THIRD_PARTY_CMAKE}")
        message(STATUS "Including third-party superbuild configuration")
        include("${_THIRD_PARTY_CMAKE}")
    else()
        message(WARNING "Third-party superbuild file not found: ${_THIRD_PARTY_CMAKE}")
        message(WARNING "Third-party dependencies will not be built.")
        message(WARNING "This file will be created in Phase 2 of the build system implementation.")
    endif()
else()
    message(STATUS "Third-party build disabled - using pre-built dependencies")
endif()

# =============================================================================
# Main Application
# =============================================================================
#
# Add the main FlySight Viewer application as an ExternalProject.
# This invokes the existing src/CMakeLists.txt which handles the actual
# application build, Qt integration, and all application-specific targets.
#

if(FLYSIGHT_BUILD_APP)
    message(STATUS "Application build enabled")

    # Collect third-party targets for DEPENDS clause
    set(THIRD_PARTY_TARGETS "")
    if(FLYSIGHT_BUILD_THIRD_PARTY)
        if(TARGET ext_oneTBB)
            list(APPEND THIRD_PARTY_TARGETS ext_oneTBB)
        endif()
        if(TARGET ext_GTSAM)
            list(APPEND THIRD_PARTY_TARGETS ext_GTSAM)
        endif()
        if(TARGET ext_KDDockWidgets)
            list(APPEND THIRD_PARTY_TARGETS ext_KDDockWidgets)
        endif()
    endif()

    # Build the main FlySight Viewer application
    # Forward CMAKE_PREFIX_PATH so the inner build can find Qt and other
    # packages (e.g., MacPorts installs Qt6 modules to /opt/local/libexec/qt6
    # which is not on the default search path).
    set(_APP_CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        -DTHIRD_PARTY_DIR=${THIRD_PARTY_DIR}
        -DGTSAM_ROOT=${GTSAM_INSTALL_DIR}
        -DTBB_ROOT=${ONETBB_INSTALL_DIR}
        -DKDDW_ROOT=${KDDW_INSTALL_DIR}
    )
    if(CMAKE_PREFIX_PATH)
        list(APPEND _APP_CMAKE_ARGS "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
    endif()

    ExternalProject_Add(FlySightViewer
        SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src"
        BINARY_DIR "${CMAKE_BINARY_DIR}/FlySightViewer-build"
        INSTALL_DIR "${CMAKE_BINARY_DIR}/install"
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
        CMAKE_ARGS ${_APP_CMAKE_ARGS}
        BUILD_ALWAYS TRUE
        DEPENDS ${THIRD_PARTY_TARGETS}
    )
else()
    message(STATUS "Application build disabled")
endif()

# =============================================================================
# Build Configuration Summary
# =============================================================================

message(STATUS "")
message(STATUS "========================================")
message(STATUS "FlySight Viewer Superbuild Configuration")
message(STATUS "========================================")
message(STATUS "")
message(STATUS "CMake Version:              ${CMAKE_VERSION}")
message(STATUS "CMake Generator:            ${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR_PLATFORM)
    message(STATUS "Generator Platform:         ${CMAKE_GENERATOR_PLATFORM}")
endif()
message(STATUS "")
message(STATUS "Build Options:")
message(STATUS "  FLYSIGHT_BUILD_THIRD_PARTY: ${FLYSIGHT_BUILD_THIRD_PARTY}")
message(STATUS "  FLYSIGHT_BUILD_APP:         ${FLYSIGHT_BUILD_APP}")
message(STATUS "  FLYSIGHT_THIRD_PARTY_ONLY:  ${FLYSIGHT_THIRD_PARTY_ONLY}")
message(STATUS "")
message(STATUS "Path Variables:")
message(STATUS "  THIRD_PARTY_DIR:    ${THIRD_PARTY_DIR}")
message(STATUS "  ONETBB_INSTALL_DIR: ${ONETBB_INSTALL_DIR}")
message(STATUS "  GTSAM_INSTALL_DIR:  ${GTSAM_INSTALL_DIR}")
message(STATUS "  KDDW_INSTALL_DIR:   ${KDDW_INSTALL_DIR}")
message(STATUS "")
message(STATUS "========================================")
message(STATUS "")
