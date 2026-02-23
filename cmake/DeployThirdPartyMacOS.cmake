# =============================================================================
# DeployThirdPartyMacOS.cmake
# =============================================================================
#
# This module copies GeographicLib and KDDockWidgets dylibs to the app bundle's
# Frameworks directory and fixes their library paths using install_name_tool.
#
# Usage:
#   include(DeployThirdPartyMacOS)
#   deploy_third_party_macos(
#       TARGET FlySightViewer
#       GEOGRAPHIC_ROOT "/path/to/GeographicLib-install"
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
#   TARGET          - The application target name (e.g., FlySightViewer)
#   GEOGRAPHIC_ROOT - Path to GeographicLib install directory
#   KDDW_ROOT       - Path to KDDockWidgets install directory
#
function(deploy_third_party_macos)
    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        ""
        "TARGET;GEOGRAPHIC_ROOT;KDDW_ROOT"
        ""
    )

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "deploy_third_party_macos: TARGET argument is required")
    endif()

    # Resolve the actual output name (may differ from target name, e.g. "FlySight Viewer" vs "FlySightViewer")
    get_target_property(_output_name ${ARG_TARGET} OUTPUT_NAME)
    if(NOT _output_name)
        set(_output_name "${ARG_TARGET}")
    endif()

    # Bundle name for install destinations (relative to CMAKE_INSTALL_PREFIX)
    set(_bundle_name "${_output_name}.app")

    message(STATUS "")
    message(STATUS "----------------------------------------")
    message(STATUS "Third-Party Deployment Configuration (macOS)")
    message(STATUS "----------------------------------------")
    message(STATUS "  Target:          ${ARG_TARGET}")
    message(STATUS "  GEOGRAPHIC_ROOT: ${ARG_GEOGRAPHIC_ROOT}")
    message(STATUS "  KDDW_ROOT:       ${ARG_KDDW_ROOT}")
    message(STATUS "----------------------------------------")
    message(STATUS "")

    # NOTE: RPATH properties are set in src/CMakeLists.txt (single source of truth)
    # Do not set BUILD_RPATH, INSTALL_RPATH, or MACOSX_RPATH here.

    # =======================================================================
    # Collect dylibs from each third-party directory
    # =======================================================================

    set(_all_dylibs "")

    # GeographicLib dylibs
    if(ARG_GEOGRAPHIC_ROOT AND EXISTS "${ARG_GEOGRAPHIC_ROOT}/lib")
        file(GLOB _geographic_dylibs "${ARG_GEOGRAPHIC_ROOT}/lib/*.dylib")
        list(APPEND _all_dylibs ${_geographic_dylibs})
        message(STATUS "Found GeographicLib dylibs: ${_geographic_dylibs}")
    endif()

    # KDDockWidgets dylibs or framework
    set(_kddw_framework_dir "${ARG_KDDW_ROOT}/lib/kddockwidgets-qt6.framework")
    set(_kddw_has_framework FALSE)

    if(ARG_KDDW_ROOT AND EXISTS "${ARG_KDDW_ROOT}/lib")
        # Check for framework bundle first (preferred on macOS)
        if(EXISTS "${_kddw_framework_dir}")
            set(_kddw_has_framework TRUE)
            # Try versioned path first (e.g., Versions/3/kddockwidgets-qt6)
            file(GLOB _kddw_framework_binaries
                "${_kddw_framework_dir}/Versions/*/kddockwidgets-qt6"
            )
            if(_kddw_framework_binaries)
                # Use the first found (should be the current version)
                list(GET _kddw_framework_binaries 0 _kddw_framework_binary)
                message(STATUS "Found KDDockWidgets framework: ${_kddw_framework_binary}")
            else()
                message(WARNING "KDDockWidgets framework exists but no binary found inside")
            endif()
        endif()

        # Also check for standalone dylibs (may exist alongside or instead of framework)
        file(GLOB _kddw_dylibs "${ARG_KDDW_ROOT}/lib/libkddockwidgets-qt6*.dylib")
        if(_kddw_dylibs)
            list(APPEND _all_dylibs ${_kddw_dylibs})
            message(STATUS "Found KDDockWidgets dylibs: ${_kddw_dylibs}")
        elseif(NOT _kddw_has_framework)
            message(WARNING "No KDDockWidgets libraries found in ${ARG_KDDW_ROOT}/lib")
        endif()
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
            DESTINATION "${_bundle_name}/Contents/Frameworks"
        )
    endforeach()

    # =======================================================================
    # Recreate dylib symlinks in Frameworks directory
    # =======================================================================
    # Libraries install versioned files (e.g., libfoo.1.2.3.dylib) with symlinks
    # (libfoo.1.dylib -> libfoo.1.2.3.dylib, libfoo.dylib -> libfoo.1.dylib).
    # The install loop above copies only real files, so we read the symlink
    # targets from the source directories at configure time and generate an
    # install script that recreates them.

    set(_symlink_commands "")
    foreach(_dylib ${_all_dylibs})
        if(IS_SYMLINK "${_dylib}")
            get_filename_component(_link_name "${_dylib}" NAME)
            file(READ_SYMLINK "${_dylib}" _link_target)
            string(APPEND _symlink_commands "
if(NOT EXISTS \"\${FRAMEWORKS_DIR}/${_link_name}\")
    execute_process(
        COMMAND ln -sf \"${_link_target}\" \"${_link_name}\"
        WORKING_DIRECTORY \"\${FRAMEWORKS_DIR}\"
    )
    message(STATUS \"  Created symlink: ${_link_name} -> ${_link_target}\")
endif()
")
        endif()
    endforeach()

    if(_symlink_commands)
        set(_create_symlinks_script "${CMAKE_BINARY_DIR}/create_dylib_symlinks_macos.cmake")
        file(WRITE "${_create_symlinks_script}" "
# Recreate versioned dylib symlinks in Frameworks directory
set(FRAMEWORKS_DIR \"\${CMAKE_INSTALL_PREFIX}/${_bundle_name}/Contents/Frameworks\")
message(STATUS \"Creating library symlinks in \${FRAMEWORKS_DIR}\")
${_symlink_commands}
message(STATUS \"Symlink creation complete\")
")
        install(SCRIPT "${_create_symlinks_script}")
    endif()

    # =======================================================================
    # Install KDDockWidgets framework (if present)
    # =======================================================================
    # For framework bundles, copy the entire framework directory to preserve
    # the macOS framework structure (Versions, symlinks, etc.)

    if(_kddw_has_framework AND EXISTS "${_kddw_framework_dir}")
        message(STATUS "Installing KDDockWidgets framework to app bundle")
        install(
            DIRECTORY "${_kddw_framework_dir}"
            DESTINATION "${_bundle_name}/Contents/Frameworks"
            USE_SOURCE_PERMISSIONS
        )
    endif()

    # =======================================================================
    # Create install script to fix library paths
    # =======================================================================

    set(_fix_paths_script "${CMAKE_BINARY_DIR}/fix_third_party_paths_macos.cmake")
    file(WRITE "${_fix_paths_script}" "
# Fix library paths for third-party dylibs in macOS app bundle
# This script runs at install time and processes:
# - Contents/Frameworks/*.dylib (third-party and Python libraries)
# - Contents/PlugIns/**/*.dylib (Qt plugins like platforms, imageformats)
# - Contents/Resources/qml/**/*.dylib (QML module libraries)

set(BUNDLE_DIR \"\${CMAKE_INSTALL_PREFIX}/${_bundle_name}\")
set(FRAMEWORKS_DIR \"\${BUNDLE_DIR}/Contents/Frameworks\")
set(MACOS_DIR \"\${BUNDLE_DIR}/Contents/MacOS\")
set(PLUGINS_DIR \"\${BUNDLE_DIR}/Contents/PlugIns\")
set(QML_DIR \"\${BUNDLE_DIR}/Contents/Resources/qml\")

message(STATUS \"Fixing third-party library paths...\")
message(STATUS \"  Bundle: \${BUNDLE_DIR}\")
message(STATUS \"  Frameworks: \${FRAMEWORKS_DIR}\")
message(STATUS \"  Plugins: \${PLUGINS_DIR}\")
message(STATUS \"  QML: \${QML_DIR}\")

# Collect all dylibs to process
set(ALL_DYLIBS \"\")
file(GLOB _fw_dylibs \"\${FRAMEWORKS_DIR}/*.dylib\")
file(GLOB_RECURSE _plugin_dylibs \"\${PLUGINS_DIR}/*.dylib\")
file(GLOB_RECURSE _qml_dylibs \"\${QML_DIR}/*.dylib\")
list(APPEND ALL_DYLIBS \${_fw_dylibs} \${_plugin_dylibs} \${_qml_dylibs})

list(LENGTH ALL_DYLIBS _dylib_count)
message(STATUS \"Found \${_dylib_count} dylibs to process\")

# Process each dylib
foreach(DYLIB \${ALL_DYLIBS})
    # Skip symlinks
    if(IS_SYMLINK \"\${DYLIB}\")
        continue()
    endif()

    get_filename_component(DYLIB_NAME \"\${DYLIB}\" NAME)
    get_filename_component(DYLIB_DIR \"\${DYLIB}\" DIRECTORY)

    # Determine if this is in Frameworks (set ID) or elsewhere (plugin/QML)
    string(FIND \"\${DYLIB}\" \"/Frameworks/\" _is_framework)

    if(NOT _is_framework EQUAL -1)
        # Library in Frameworks - set its ID to @rpath/libname
        execute_process(
            COMMAND install_name_tool -id \"@rpath/\${DYLIB_NAME}\" \"\${DYLIB}\"
            RESULT_VARIABLE _result
            ERROR_VARIABLE _error
        )
        if(_result AND NOT _result EQUAL 0)
            message(STATUS \"  Note: Could not set id for \${DYLIB_NAME}: \${_error}\")
        endif()
    else()
        # Plugin or QML module - add rpath to find Frameworks
        file(RELATIVE_PATH _rel_path \"\${DYLIB_DIR}\" \"\${FRAMEWORKS_DIR}\")

        execute_process(
            COMMAND otool -l \"\${DYLIB}\"
            OUTPUT_VARIABLE _load_commands
            ERROR_QUIET
        )

        if(NOT _load_commands MATCHES \"@loader_path/\${_rel_path}\")
            execute_process(
                COMMAND install_name_tool -add_rpath \"@loader_path/\${_rel_path}\" \"\${DYLIB}\"
                ERROR_QUIET
            )
        endif()
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
        if(DEP MATCHES \"^/usr/lib\" OR DEP MATCHES \"^/System/Library\")
            continue()
        endif()

        if(DEP MATCHES \"^@\")
            continue()
        endif()

        get_filename_component(DEP_NAME \"\${DEP}\" NAME)

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

    # Fix framework references (Qt, KDDockWidgets, etc.) in this dylib
    string(REGEX MATCHALL \"[^\\n\\t ]+[A-Za-z0-9_-]+\\\\.framework/[^\\n\\t ]*\" _fw_deps \"\${_deps}\")
    foreach(DEP \${_fw_deps})
        if(DEP MATCHES \"^@\" OR DEP MATCHES \"^/System/Library\" OR DEP MATCHES \"^/usr/lib\")
            continue()
        endif()
        string(REGEX MATCH \"([A-Za-z0-9_-]+)\\\\.framework\" _fw_match \"\${DEP}\")
        set(_fw_name \"\${CMAKE_MATCH_1}\")
        if(EXISTS \"\${FRAMEWORKS_DIR}/\${_fw_name}.framework\")
            string(REGEX REPLACE \"^.*(\${_fw_name}\\\\.framework.*)$\" \"\\\\1\" _fw_rel \"\${DEP}\")
            execute_process(
                COMMAND install_name_tool -change \"\${DEP}\" \"@rpath/\${_fw_rel}\" \"\${DYLIB}\"
                RESULT_VARIABLE _result
                ERROR_QUIET
            )
            if(_result EQUAL 0)
                message(STATUS \"  Fixed: \${DYLIB_NAME} -> \${_fw_rel}\")
            endif()
        endif()
    endforeach()

    # Ad-hoc re-sign after all path modifications (required on macOS 12+)
    execute_process(
        COMMAND codesign --force --sign - \"\${DYLIB}\"
        ERROR_QUIET
    )
endforeach()

# Fix the main executable
set(EXECUTABLE \"\${MACOS_DIR}/${_output_name}\")
if(EXISTS \"\${EXECUTABLE}\")
    message(STATUS \"Fixing executable: \${EXECUTABLE}\")

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

    # Handle KDDockWidgets framework
    string(REGEX MATCHALL \"[^\\n\\t ]+kddockwidgets-qt6\\\\.framework[^\\n\\t ]*\" KDDW_DEP_LIST \"\${_exe_deps}\")
    foreach(DEP \${KDDW_DEP_LIST})
        if(DEP MATCHES \"^@rpath/\")
            continue()
        endif()

        string(REGEX REPLACE \"^.*(kddockwidgets-qt6\\\\.framework.*)$\" \"\\\\1\" FRAMEWORK_REL_PATH \"\${DEP}\")

        if(EXISTS \"\${FRAMEWORKS_DIR}/\${FRAMEWORK_REL_PATH}\")
            execute_process(
                COMMAND install_name_tool -change \"\${DEP}\" \"@rpath/\${FRAMEWORK_REL_PATH}\" \"\${EXECUTABLE}\"
                RESULT_VARIABLE _result
                ERROR_QUIET
            )
            if(_result EQUAL 0)
                message(STATUS \"  Fixed framework dep: \${FRAMEWORK_REL_PATH}\")
            endif()
        endif()
    endforeach()

    # Handle Qt framework references
    message(STATUS \"Checking Qt framework references...\")
    string(REGEX MATCHALL \"[^\\n\\t ]+Qt[A-Za-z0-9]+\\\\.framework[^\\n\\t ]*\" QT_DEP_LIST \"\${_exe_deps}\")

    foreach(DEP \${QT_DEP_LIST})
        if(DEP MATCHES \"^@\")
            message(STATUS \"  Qt OK: \${DEP}\")
            continue()
        endif()

        string(REGEX MATCH \"Qt[A-Za-z0-9]+\" _qt_name \"\${DEP}\")
        set(_qt_framework \"\${BUNDLE_DIR}/Contents/Frameworks/\${_qt_name}.framework\")

        if(EXISTS \"\${_qt_framework}\")
            string(REGEX REPLACE \"^.*(\${_qt_name}\\\\.framework.*)$\" \"\\\\1\" _qt_rel_path \"\${DEP}\")

            execute_process(
                COMMAND install_name_tool -change \"\${DEP}\" \"@executable_path/../Frameworks/\${_qt_rel_path}\" \"\${EXECUTABLE}\"
                RESULT_VARIABLE _result
                ERROR_QUIET
            )
            if(_result EQUAL 0)
                message(STATUS \"  Fixed Qt reference: \${_qt_name}\")
            endif()
        else()
            message(WARNING \"  Qt framework not deployed: \${_qt_name} (referenced from executable)\")
        endif()
    endforeach()

    # Handle Python framework reference
    string(REGEX MATCH \"[^\\n\\t ]*/Python\\\\.framework/Versions/([0-9]+\\\\.[0-9]+)/Python\" _python_dep \"\${_exe_deps}\")
    # Save CMAKE_MATCH_1 immediately — the MATCHES operator in the if()
    # below clears CMAKE_MATCH variables even when the match fails (CMake 3.10+).
    set(_py_ver \"\${CMAKE_MATCH_1}\")
    if(_python_dep AND NOT _python_dep MATCHES \"^@\")
        set(_py_dylib_name \"libpython\${_py_ver}.dylib\")
        if(EXISTS \"\${FRAMEWORKS_DIR}/\${_py_dylib_name}\")
            execute_process(
                COMMAND install_name_tool -change \"\${_python_dep}\" \"@rpath/\${_py_dylib_name}\" \"\${EXECUTABLE}\"
                RESULT_VARIABLE _result
                ERROR_QUIET
            )
            if(_result EQUAL 0)
                message(STATUS \"  Fixed Python: -> @rpath/\${_py_dylib_name}\")
            endif()
        endif()
    endif()

    # Ad-hoc re-sign executable after all path modifications
    execute_process(
        COMMAND codesign --force --sign - \"\${EXECUTABLE}\"
        ERROR_QUIET
    )
endif()

# Fix KDDockWidgets framework binary if present
set(KDDW_FRAMEWORK \"\${FRAMEWORKS_DIR}/kddockwidgets-qt6.framework\")
if(EXISTS \"\${KDDW_FRAMEWORK}\")
    message(STATUS \"Fixing KDDockWidgets framework paths\")
    file(GLOB_RECURSE KDDW_BINARIES \"\${KDDW_FRAMEWORK}/Versions/*/kddockwidgets-qt6\")
    foreach(KDDW_BIN \${KDDW_BINARIES})
        get_filename_component(KDDW_BIN_DIR \"\${KDDW_BIN}\" DIRECTORY)
        file(RELATIVE_PATH KDDW_REL_PATH \"\${FRAMEWORKS_DIR}\" \"\${KDDW_BIN}\")

        execute_process(
            COMMAND install_name_tool -id \"@rpath/\${KDDW_REL_PATH}\" \"\${KDDW_BIN}\"
            RESULT_VARIABLE _result
            ERROR_QUIET
        )
        if(_result EQUAL 0)
            message(STATUS \"  Set framework id: @rpath/\${KDDW_REL_PATH}\")
        endif()

        execute_process(
            COMMAND codesign --force --sign - \"\${KDDW_BIN}\"
            ERROR_QUIET
        )
    endforeach()
endif()

message(STATUS \"Third-party library path fixing complete\")
")
    install(SCRIPT "${_fix_paths_script}")

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

    # Resolve the actual output name for the bundle directory
    get_target_property(_output_name ${target} OUTPUT_NAME)
    if(NOT _output_name)
        set(_output_name "${target}")
    endif()

    get_filename_component(_dylib_name "${dylib_path}" NAME)

    install(
        FILES "${dylib_path}"
        DESTINATION "${_output_name}.app/Contents/Frameworks"
    )
endfunction()
