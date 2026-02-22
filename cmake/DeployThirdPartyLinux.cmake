# =============================================================================
# DeployThirdPartyLinux.cmake
# =============================================================================
#
# Copies GeographicLib and KDDockWidgets shared libraries to the AppDir's lib
# directory and ensures proper rpath configuration for Linux deployment.
#
# This module:
# 1. Identifies required shared libraries from each third-party install directory
# 2. Installs libraries to AppDir lib directory (preserving symlinks)
# 3. Verifies the executable has correct rpath ($ORIGIN/../lib)
#
# =============================================================================

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

# Ensure APPDIR variables are set
if(NOT DEFINED FLYSIGHT_APPDIR_PATH)
    set(FLYSIGHT_APPDIR_PATH "${CMAKE_INSTALL_PREFIX}/FlySightViewer.AppDir")
    set(FLYSIGHT_APPDIR_USR "${FLYSIGHT_APPDIR_PATH}/usr")
endif()

# =============================================================================
# Third-Party Install Directories
# =============================================================================

# These should match the paths defined in src/CMakeLists.txt
if(NOT DEFINED THIRD_PARTY_DIR)
    set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third-party")
endif()

set(LINUX_GEOGRAPHIC_ROOT "${THIRD_PARTY_DIR}/GeographicLib-install" CACHE PATH "GeographicLib install prefix")
set(LINUX_KDDW_ROOT "${THIRD_PARTY_DIR}/KDDockWidgets-install" CACHE PATH "KDDockWidgets install prefix")

# =============================================================================
# Install GeographicLib Libraries
# =============================================================================

install(CODE "
    message(STATUS \"Installing GeographicLib shared libraries...\")

    set(GEOGRAPHIC_LIB_DIR \"${LINUX_GEOGRAPHIC_ROOT}/lib\")
    set(DEST_LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    file(MAKE_DIRECTORY \"\${DEST_LIB_DIR}\")

    if(EXISTS \"\${GEOGRAPHIC_LIB_DIR}\")
        file(GLOB GEOGRAPHIC_LIBS \"\${GEOGRAPHIC_LIB_DIR}/libGeographic*.so*\")

        # Separate real files from symlinks
        set(GEO_REAL_LIBS \"\")
        set(GEO_SYMLINKS \"\")
        foreach(lib \${GEOGRAPHIC_LIBS})
            if(IS_SYMLINK \"\${lib}\")
                list(APPEND GEO_SYMLINKS \"\${lib}\")
            else()
                list(APPEND GEO_REAL_LIBS \"\${lib}\")
            endif()
        endforeach()

        # Copy real files first and set RPATH on them
        find_program(PATCHELF_EXE patchelf)
        foreach(lib \${GEO_REAL_LIBS})
            get_filename_component(libname \"\${lib}\" NAME)
            execute_process(
                COMMAND cp \"\${lib}\" \"\${DEST_LIB_DIR}/\"
                RESULT_VARIABLE cp_result
            )
            if(cp_result EQUAL 0)
                message(STATUS \"  Copied: \${libname}\")
                if(PATCHELF_EXE)
                    execute_process(
                        COMMAND \"\${PATCHELF_EXE}\" --set-rpath \"\\\$ORIGIN\" \"\${DEST_LIB_DIR}/\${libname}\"
                        ERROR_QUIET
                    )
                endif()
            else()
                message(WARNING \"  Failed to copy: \${libname}\")
            endif()
        endforeach()

        # Copy symlinks preserving link structure
        foreach(lib \${GEO_SYMLINKS})
            get_filename_component(libname \"\${lib}\" NAME)
            execute_process(
                COMMAND cp -P \"\${lib}\" \"\${DEST_LIB_DIR}/\"
                RESULT_VARIABLE cp_result
            )
            if(cp_result EQUAL 0)
                message(STATUS \"  Copied symlink: \${libname}\")
            endif()
        endforeach()

        file(GLOB COPIED_GEO \"\${DEST_LIB_DIR}/libGeographic*.so*\")
        list(LENGTH COPIED_GEO geo_count)
        message(STATUS \"GeographicLib: Installed \${geo_count} library files\")
    else()
        message(WARNING \"GeographicLib library directory not found: \${GEOGRAPHIC_LIB_DIR}\")
    endif()
")

# =============================================================================
# Install KDDockWidgets Libraries
# =============================================================================

install(CODE "
    message(STATUS \"Installing KDDockWidgets shared libraries...\")

    set(KDDW_LIB_DIR \"${LINUX_KDDW_ROOT}/lib\")
    set(KDDW_LIB64_DIR \"${LINUX_KDDW_ROOT}/lib64\")
    set(DEST_LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    # Try lib directory first, then lib64
    if(EXISTS \"\${KDDW_LIB_DIR}\")
        set(KDDW_SEARCH_DIR \"\${KDDW_LIB_DIR}\")
    elseif(EXISTS \"\${KDDW_LIB64_DIR}\")
        set(KDDW_SEARCH_DIR \"\${KDDW_LIB64_DIR}\")
        message(STATUS \"Using lib64 directory for KDDockWidgets\")
    else()
        set(KDDW_SEARCH_DIR \"\")
    endif()

    if(KDDW_SEARCH_DIR)
        # Find KDDockWidgets shared libraries
        file(GLOB KDDW_LIBS \"\${KDDW_SEARCH_DIR}/libkddockwidgets-qt6.so*\")

        if(NOT KDDW_LIBS)
            file(GLOB KDDW_LIBS \"\${KDDW_SEARCH_DIR}/libkddockwidgets*.so*\")
        endif()

        if(NOT KDDW_LIBS)
            message(WARNING \"No KDDockWidgets libraries found in \${KDDW_SEARCH_DIR}\")
            message(STATUS \"Contents of \${KDDW_SEARCH_DIR}:\")
            file(GLOB _kddw_all \"\${KDDW_SEARCH_DIR}/*\")
            foreach(_f \${_kddw_all})
                message(STATUS \"  \${_f}\")
            endforeach()
        else()
            foreach(lib \${KDDW_LIBS})
                get_filename_component(libname \"\${lib}\" NAME)
                execute_process(
                    COMMAND cp -P \"\${lib}\" \"\${DEST_LIB_DIR}/\"
                    RESULT_VARIABLE cp_result
                )
                if(cp_result EQUAL 0)
                    message(STATUS \"  Copied: \${libname}\")
                else()
                    message(WARNING \"  Failed to copy: \${libname}\")
                endif()
            endforeach()

            file(GLOB COPIED_KDDW \"\${DEST_LIB_DIR}/libkddockwidgets*.so*\")
            list(LENGTH COPIED_KDDW kddw_count)
            message(STATUS \"KDDockWidgets: Installed \${kddw_count} library files\")
        endif()
    else()
        message(WARNING \"KDDockWidgets library directory not found in ${LINUX_KDDW_ROOT}/lib or ${LINUX_KDDW_ROOT}/lib64\")
    endif()
")

# =============================================================================
# Verify and Set RPATH Configuration
# =============================================================================

install(CODE "
    message(STATUS \"Setting RPATH on executables...\")

    set(EXECUTABLE \"${FLYSIGHT_APPDIR_USR}/bin/FlySightViewer\")
    set(EXPECTED_RPATH \"\\\$ORIGIN/../lib\")
    set(BRIDGE_RPATH \"\\\$ORIGIN/../../../../lib\")

    find_program(PATCHELF_EXE patchelf REQUIRED)
    if(NOT PATCHELF_EXE)
        message(FATAL_ERROR \"patchelf not found! Install with: apt-get install patchelf\")
    endif()

    message(STATUS \"  Using patchelf: \${PATCHELF_EXE}\")

    # Set RPATH on main executable
    if(EXISTS \"\${EXECUTABLE}\")
        execute_process(
            COMMAND \"\${PATCHELF_EXE}\" --set-rpath \"\${EXPECTED_RPATH}\" \"\${EXECUTABLE}\"
            RESULT_VARIABLE patchelf_result
            ERROR_VARIABLE patchelf_error
        )
        if(patchelf_result EQUAL 0)
            message(STATUS \"  Set RPATH on FlySightViewer: \${EXPECTED_RPATH}\")
            execute_process(
                COMMAND \"\${PATCHELF_EXE}\" --print-rpath \"\${EXECUTABLE}\"
                OUTPUT_VARIABLE verified_rpath
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            message(STATUS \"  Verified RPATH: \${verified_rpath}\")
        else()
            message(WARNING \"  Failed to set RPATH: \${patchelf_error}\")
        endif()
    else()
        message(WARNING \"  Executable not found: \${EXECUTABLE}\")
    endif()

    # Set RPATH on pybind11 bridge module
    file(GLOB BRIDGE_FILES \"${FLYSIGHT_APPDIR_USR}/share/python/lib/python*/site-packages/flysight_cpp_bridge*.so\")
    foreach(bridge \${BRIDGE_FILES})
        execute_process(
            COMMAND \"\${PATCHELF_EXE}\" --set-rpath \"\${BRIDGE_RPATH}\" \"\${bridge}\"
            RESULT_VARIABLE patchelf_result
            ERROR_VARIABLE patchelf_error
        )
        if(patchelf_result EQUAL 0)
            get_filename_component(bridge_name \"\${bridge}\" NAME)
            message(STATUS \"  Set RPATH on \${bridge_name}: \${BRIDGE_RPATH}\")
            execute_process(
                COMMAND \"\${PATCHELF_EXE}\" --print-rpath \"\${bridge}\"
                OUTPUT_VARIABLE verified_bridge_rpath
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            message(STATUS \"  Verified bridge RPATH: \${verified_bridge_rpath}\")
        else()
            message(WARNING \"  Failed to set RPATH on bridge: \${patchelf_error}\")
        endif()
    endforeach()

    if(NOT BRIDGE_FILES)
        message(STATUS \"  No bridge module found yet (will be set after installation)\")
    endif()
")

# =============================================================================
# Verify Library Symlinks
# =============================================================================

install(CODE "
    message(STATUS \"Verifying library symlinks...\")

    set(LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    set(EXPECTED_SYMLINKS
        \"libkddockwidgets-qt6.so\"
        \"libGeographic.so\"
    )

    foreach(lib \${EXPECTED_SYMLINKS})
        set(lib_path \"\${LIB_DIR}/\${lib}\")
        if(EXISTS \"\${lib_path}\")
            execute_process(
                COMMAND readlink \"\${lib_path}\"
                OUTPUT_VARIABLE link_target
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE readlink_result
            )
            if(readlink_result EQUAL 0 AND link_target)
                message(STATUS \"  \${lib} -> \${link_target}\")
            else()
                message(STATUS \"  \${lib} (not a symlink - may be OK)\")
            endif()
        else()
            file(GLOB versioned_lib \"\${LIB_DIR}/\${lib}.*\")
            if(versioned_lib)
                list(GET versioned_lib 0 first_versioned)
                get_filename_component(versioned_name \"\${first_versioned}\" NAME)
                message(STATUS \"  \${lib}: Found as \${versioned_name}\")
            else()
                message(WARNING \"  \${lib}: NOT FOUND\")
            endif()
        endif()
    endforeach()

    message(STATUS \"Library symlink verification complete\")
")

# =============================================================================
# Summary
# =============================================================================

message(STATUS "DeployThirdPartyLinux.cmake: Third-party library deployment configured")
message(STATUS "  GeographicLib from: ${LINUX_GEOGRAPHIC_ROOT}")
message(STATUS "  KDDockWidgets from: ${LINUX_KDDW_ROOT}")
message(STATUS "  Target: ${FLYSIGHT_APPDIR_USR}/lib")
