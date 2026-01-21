# =============================================================================
# DeployThirdPartyMacOS.cmake
# =============================================================================
#
# This module copies GTSAM, TBB, and KDDockWidgets dylibs to the app bundle's
# Frameworks directory and fixes their library paths using install_name_tool.
#
# Usage:
#   include(DeployThirdPartyMacOS)
#   deploy_third_party_macos(
#       TARGET FlySightViewer
#       TBB_ROOT "/path/to/oneTBB-install"
#       GTSAM_ROOT "/path/to/GTSAM-install"
#       KDDW_ROOT "/path/to/KDDockWidgets-install"
#   )
#
# This module:
#   1. Identifies required dylibs from each third-party install directory
#   2. Installs libraries to Contents/Frameworks/
#   3. Fixes library paths using install_name_tool
#   4. Sets executable rpath during build
#
# =============================================================================

include_guard(GLOBAL)

# =============================================================================
# deploy_third_party_macos()
# =============================================================================
#
# Main function to set up third-party library deployment for macOS app bundles.
#
# Arguments:
#   TARGET     - The application target name (e.g., FlySightViewer)
#   TBB_ROOT   - Path to oneTBB install directory
#   GTSAM_ROOT - Path to GTSAM install directory
#   KDDW_ROOT  - Path to KDDockWidgets install directory
#
function(deploy_third_party_macos)
    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        ""
        "TARGET;TBB_ROOT;GTSAM_ROOT;KDDW_ROOT"
        ""
    )

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "deploy_third_party_macos: TARGET argument is required")
    endif()

    message(STATUS "")
    message(STATUS "----------------------------------------")
    message(STATUS "Third-Party Deployment Configuration (macOS)")
    message(STATUS "----------------------------------------")
    message(STATUS "  Target:     ${ARG_TARGET}")
    message(STATUS "  TBB_ROOT:   ${ARG_TBB_ROOT}")
    message(STATUS "  GTSAM_ROOT: ${ARG_GTSAM_ROOT}")
    message(STATUS "  KDDW_ROOT:  ${ARG_KDDW_ROOT}")
    message(STATUS "----------------------------------------")
    message(STATUS "")

    # Set rpath for the executable at build time
    set_target_properties(${ARG_TARGET} PROPERTIES
        BUILD_RPATH "@executable_path/../Frameworks"
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    # =======================================================================
    # Collect dylibs from each third-party directory
    # =======================================================================

    set(_all_dylibs "")

    # TBB dylibs
    if(ARG_TBB_ROOT AND EXISTS "${ARG_TBB_ROOT}/lib")
        file(GLOB _tbb_dylibs "${ARG_TBB_ROOT}/lib/*.dylib")
        list(APPEND _all_dylibs ${_tbb_dylibs})
        message(STATUS "Found TBB dylibs: ${_tbb_dylibs}")
    endif()

    # GTSAM dylibs
    if(ARG_GTSAM_ROOT AND EXISTS "${ARG_GTSAM_ROOT}/lib")
        file(GLOB _gtsam_dylibs "${ARG_GTSAM_ROOT}/lib/*.dylib")
        list(APPEND _all_dylibs ${_gtsam_dylibs})
        message(STATUS "Found GTSAM dylibs: ${_gtsam_dylibs}")
    endif()

    # KDDockWidgets dylibs
    if(ARG_KDDW_ROOT AND EXISTS "${ARG_KDDW_ROOT}/lib")
        file(GLOB _kddw_dylibs "${ARG_KDDW_ROOT}/lib/*.dylib")
        # Also check for framework bundles
        if(EXISTS "${ARG_KDDW_ROOT}/lib/kddockwidgets-qt6.framework")
            # For framework bundles, we need the actual dylib inside
            file(GLOB _kddw_framework_dylibs
                "${ARG_KDDW_ROOT}/lib/kddockwidgets-qt6.framework/Versions/*/kddockwidgets-qt6")
            list(APPEND _kddw_dylibs ${_kddw_framework_dylibs})
        endif()
        list(APPEND _all_dylibs ${_kddw_dylibs})
        message(STATUS "Found KDDockWidgets dylibs: ${_kddw_dylibs}")
    endif()

    # =======================================================================
    # Install dylibs to Frameworks directory
    # =======================================================================

    foreach(_dylib ${_all_dylibs})
        # Skip symlinks - we'll handle the real files
        if(IS_SYMLINK "${_dylib}")
            continue()
        endif()

        get_filename_component(_dylib_name "${_dylib}" NAME)

        install(
            FILES "${_dylib}"
            DESTINATION "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Frameworks"
            COMPONENT Runtime
        )
    endforeach()

    # =======================================================================
    # Create install script to fix library paths
    # =======================================================================

    set(_fix_paths_script "${CMAKE_BINARY_DIR}/fix_third_party_paths_macos.cmake")
    file(WRITE "${_fix_paths_script}" "
# Fix library paths for third-party dylibs in macOS app bundle
# This script runs at install time

set(BUNDLE_DIR \"\${CMAKE_INSTALL_PREFIX}/$<TARGET_BUNDLE_DIR:${ARG_TARGET}>\")
set(FRAMEWORKS_DIR \"\${BUNDLE_DIR}/Contents/Frameworks\")
set(MACOS_DIR \"\${BUNDLE_DIR}/Contents/MacOS\")

message(STATUS \"Fixing third-party library paths in \${FRAMEWORKS_DIR}\")

# Get list of all dylibs in Frameworks
file(GLOB ALL_DYLIBS \"\${FRAMEWORKS_DIR}/*.dylib\")

# Function to fix a single dylib
foreach(DYLIB \${ALL_DYLIBS})
    get_filename_component(DYLIB_NAME \"\${DYLIB}\" NAME)

    # Set the library's own ID to @rpath/libname.dylib
    execute_process(
        COMMAND install_name_tool -id \"@rpath/\${DYLIB_NAME}\" \"\${DYLIB}\"
        RESULT_VARIABLE _result
        ERROR_VARIABLE _error
    )
    if(_result AND NOT _result EQUAL 0)
        message(STATUS \"Note: Could not set id for \${DYLIB_NAME}: \${_error}\")
    endif()

    # Get list of dependencies
    execute_process(
        COMMAND otool -L \"\${DYLIB}\"
        OUTPUT_VARIABLE _deps
        ERROR_QUIET
    )

    # Parse dependencies and fix paths
    string(REGEX MATCHALL \"[^\\n\\t ]+\\\\.dylib\" DEP_LIST \"\${_deps}\")

    foreach(DEP \${DEP_LIST})
        # Skip system libraries
        if(DEP MATCHES \"^/usr/lib\" OR DEP MATCHES \"^/System/Library\")
            continue()
        endif()

        # Skip @rpath, @executable_path, @loader_path references (already correct)
        if(DEP MATCHES \"^@\")
            continue()
        endif()

        # Get just the filename
        get_filename_component(DEP_NAME \"\${DEP}\" NAME)

        # Check if this dependency exists in our Frameworks
        if(EXISTS \"\${FRAMEWORKS_DIR}/\${DEP_NAME}\")
            execute_process(
                COMMAND install_name_tool -change \"\${DEP}\" \"@rpath/\${DEP_NAME}\" \"\${DYLIB}\"
                RESULT_VARIABLE _result
                ERROR_QUIET
            )
            if(_result EQUAL 0)
                message(STATUS \"  Fixed: \${DYLIB_NAME} -> \${DEP_NAME}\")
            endif()
        endif()
    endforeach()
endforeach()

# Fix the main executable
set(EXECUTABLE \"\${MACOS_DIR}/${ARG_TARGET}\")
if(EXISTS \"\${EXECUTABLE}\")
    message(STATUS \"Fixing executable: \${EXECUTABLE}\")

    # Add rpath if not present
    execute_process(
        COMMAND otool -l \"\${EXECUTABLE}\"
        OUTPUT_VARIABLE _exe_info
        ERROR_QUIET
    )

    if(NOT _exe_info MATCHES \"@executable_path/../Frameworks\")
        execute_process(
            COMMAND install_name_tool -add_rpath \"@executable_path/../Frameworks\" \"\${EXECUTABLE}\"
            ERROR_QUIET
        )
    endif()

    # Get dependencies and fix paths
    execute_process(
        COMMAND otool -L \"\${EXECUTABLE}\"
        OUTPUT_VARIABLE _exe_deps
        ERROR_QUIET
    )

    string(REGEX MATCHALL \"[^\\n\\t ]+\\\\.dylib\" EXE_DEP_LIST \"\${_exe_deps}\")

    foreach(DEP \${EXE_DEP_LIST})
        if(DEP MATCHES \"^/usr/lib\" OR DEP MATCHES \"^/System/Library\" OR DEP MATCHES \"^@\")
            continue()
        endif()

        get_filename_component(DEP_NAME \"\${DEP}\" NAME)

        if(EXISTS \"\${FRAMEWORKS_DIR}/\${DEP_NAME}\")
            execute_process(
                COMMAND install_name_tool -change \"\${DEP}\" \"@rpath/\${DEP_NAME}\" \"\${EXECUTABLE}\"
                RESULT_VARIABLE _result
                ERROR_QUIET
            )
            if(_result EQUAL 0)
                message(STATUS \"  Fixed executable dep: \${DEP_NAME}\")
            endif()
        endif()
    endforeach()
endif()

message(STATUS \"Third-party library path fixing complete\")
")
    install(SCRIPT "${_fix_paths_script}" COMPONENT Runtime)

endfunction()

# =============================================================================
# set_macos_rpath()
# =============================================================================
#
# Convenience function to set macOS rpath for a target.
#
function(set_macos_rpath target)
    if(APPLE)
        set_target_properties(${target} PROPERTIES
            BUILD_RPATH "@executable_path/../Frameworks;@loader_path/../Frameworks"
            INSTALL_RPATH "@executable_path/../Frameworks;@loader_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH TRUE
            MACOSX_RPATH TRUE
        )
    endif()
endfunction()

# =============================================================================
# copy_dylib_to_bundle()
# =============================================================================
#
# Helper function to copy a single dylib to the bundle's Frameworks directory.
#
function(copy_dylib_to_bundle target dylib_path)
    if(NOT EXISTS "${dylib_path}")
        message(WARNING "copy_dylib_to_bundle: dylib not found: ${dylib_path}")
        return()
    endif()

    get_filename_component(_dylib_name "${dylib_path}" NAME)

    install(
        FILES "${dylib_path}"
        DESTINATION "$<TARGET_BUNDLE_DIR:${target}>/Contents/Frameworks"
        COMPONENT Runtime
    )
endfunction()
