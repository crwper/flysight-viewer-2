# =============================================================================
# DeployThirdPartyLinux.cmake
# =============================================================================
#
# Copies GTSAM, TBB, and KDDockWidgets shared libraries to the AppDir's lib
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

set(LINUX_TBB_ROOT "${THIRD_PARTY_DIR}/oneTBB-install" CACHE PATH "oneTBB install prefix")
set(LINUX_GTSAM_ROOT "${THIRD_PARTY_DIR}/GTSAM-install" CACHE PATH "GTSAM install prefix")
set(LINUX_KDDW_ROOT "${THIRD_PARTY_DIR}/KDDockWidgets-install" CACHE PATH "KDDockWidgets install prefix")

# =============================================================================
# Helper function to copy shared libraries preserving symlinks
# =============================================================================

# This function is implemented as install(CODE) to run at install time
# because the libraries may not exist at configure time (if third-party
# hasn't been built yet)

# =============================================================================
# Install TBB Libraries
# =============================================================================

install(CODE "
    message(STATUS \"Installing TBB shared libraries...\")

    set(TBB_LIB_DIR \"${LINUX_TBB_ROOT}/lib\")
    set(DEST_LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    file(MAKE_DIRECTORY \"\${DEST_LIB_DIR}\")

    if(EXISTS \"\${TBB_LIB_DIR}\")
        # Find all TBB shared libraries (libtbb.so*, libtbbmalloc.so*, etc.)
        file(GLOB TBB_LIBS
            \"\${TBB_LIB_DIR}/libtbb.so*\"
            \"\${TBB_LIB_DIR}/libtbbmalloc.so*\"
            \"\${TBB_LIB_DIR}/libtbbmalloc_proxy.so*\"
        )

        # Separate real files from symlinks
        set(TBB_REAL_LIBS \"\")
        set(TBB_SYMLINKS \"\")
        foreach(lib \${TBB_LIBS})
            if(IS_SYMLINK \"\${lib}\")
                list(APPEND TBB_SYMLINKS \"\${lib}\")
            else()
                list(APPEND TBB_REAL_LIBS \"\${lib}\")
            endif()
        endforeach()

        # Copy real files first and set RPATH on them
        find_program(PATCHELF_EXE patchelf)
        foreach(lib \${TBB_REAL_LIBS})
            get_filename_component(libname \"\${lib}\" NAME)
            execute_process(
                COMMAND cp \"\${lib}\" \"\${DEST_LIB_DIR}/\"
                RESULT_VARIABLE cp_result
            )
            if(cp_result EQUAL 0)
                message(STATUS \"  Copied: \${libname}\")

                # Set RPATH on the library itself so it can find other TBB libs
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
        foreach(lib \${TBB_SYMLINKS})
            get_filename_component(libname \"\${lib}\" NAME)
            execute_process(
                COMMAND cp -P \"\${lib}\" \"\${DEST_LIB_DIR}/\"
                RESULT_VARIABLE cp_result
            )
            if(cp_result EQUAL 0)
                message(STATUS \"  Copied symlink: \${libname}\")
            endif()
        endforeach()

        # Count copied files
        file(GLOB COPIED_TBB \"\${DEST_LIB_DIR}/libtbb*.so*\")
        list(LENGTH COPIED_TBB tbb_count)
        message(STATUS \"TBB: Installed \${tbb_count} library files\")
    else()
        message(WARNING \"TBB library directory not found: \${TBB_LIB_DIR}\")
    endif()
")

# =============================================================================
# Install GTSAM Libraries
# =============================================================================

install(CODE "
    message(STATUS \"Installing GTSAM shared libraries...\")

    set(GTSAM_LIB_DIR \"${LINUX_GTSAM_ROOT}/lib\")
    set(DEST_LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    if(EXISTS \"\${GTSAM_LIB_DIR}\")
        # Find all GTSAM shared libraries
        file(GLOB GTSAM_LIBS
            \"\${GTSAM_LIB_DIR}/libgtsam.so*\"
            \"\${GTSAM_LIB_DIR}/libgtsam_unstable.so*\"
            \"\${GTSAM_LIB_DIR}/libmetis-gtsam.so*\"
            \"\${GTSAM_LIB_DIR}/libcephes-gtsam.so*\"
        )

        # Also include GeographicLib if built with GTSAM
        file(GLOB GEOGRAPHIC_LIBS \"\${GTSAM_LIB_DIR}/libGeographic*.so*\")
        list(APPEND GTSAM_LIBS \${GEOGRAPHIC_LIBS})

        foreach(lib \${GTSAM_LIBS})
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

        file(GLOB COPIED_GTSAM \"\${DEST_LIB_DIR}/libgtsam*.so*\" \"\${DEST_LIB_DIR}/libmetis*.so*\" \"\${DEST_LIB_DIR}/libcephes*.so*\" \"\${DEST_LIB_DIR}/libGeographic*.so*\")
        list(LENGTH COPIED_GTSAM gtsam_count)
        message(STATUS \"GTSAM: Installed \${gtsam_count} library files\")
    else()
        message(WARNING \"GTSAM library directory not found: \${GTSAM_LIB_DIR}\")
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
        # Library name is libkddockwidgets-qt6.so.X.Y.Z with symlinks
        file(GLOB KDDW_LIBS \"\${KDDW_SEARCH_DIR}/libkddockwidgets-qt6.so*\")

        if(NOT KDDW_LIBS)
            # Fallback to more general pattern in case naming differs
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
# Install Boost Libraries (if dynamically linked)
# =============================================================================

install(CODE "
    message(STATUS \"Checking for Boost shared libraries...\")

    # Boost libraries that GTSAM might need
    set(BOOST_LIBS_NEEDED
        \"serialization\"
        \"timer\"
        \"chrono\"
        \"system\"
    )

    set(DEST_LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    # Common Boost library paths
    set(BOOST_LIB_PATHS
        \"/usr/lib/x86_64-linux-gnu\"
        \"/usr/lib64\"
        \"/usr/lib\"
        \"/usr/local/lib\"
    )

    foreach(boost_lib \${BOOST_LIBS_NEEDED})
        set(found FALSE)
        foreach(lib_path \${BOOST_LIB_PATHS})
            file(GLOB BOOST_SO \"\${lib_path}/libboost_\${boost_lib}.so*\")
            if(BOOST_SO)
                foreach(lib \${BOOST_SO})
                    get_filename_component(libname \"\${lib}\" NAME)
                    if(NOT EXISTS \"\${DEST_LIB_DIR}/\${libname}\")
                        execute_process(
                            COMMAND cp -P \"\${lib}\" \"\${DEST_LIB_DIR}/\"
                            RESULT_VARIABLE cp_result
                        )
                        if(cp_result EQUAL 0)
                            message(STATUS \"  Copied Boost library: \${libname}\")
                            set(found TRUE)
                        endif()
                    endif()
                endforeach()
                if(found)
                    break()
                endif()
            endif()
        endforeach()
    endforeach()
")

# =============================================================================
# Verify and Set RPATH Configuration
# =============================================================================
# The main executable needs RPATH $ORIGIN/../lib to find libraries in usr/lib/
# The pybind11 bridge module is in usr/share/python/lib/pythonX.X/site-packages/
# so it needs RPATH $ORIGIN/../../../../lib to reach usr/lib/

install(CODE "
    message(STATUS \"Setting RPATH on executables...\")

    set(EXECUTABLE \"${FLYSIGHT_APPDIR_USR}/bin/FlySightViewer\")
    set(EXPECTED_RPATH \"\\\$ORIGIN/../lib\")
    set(BRIDGE_RPATH \"\\\$ORIGIN/../../../../lib\")

    # patchelf should be installed by Phase 1 (apt-get install patchelf)
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
            # Verify
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

    # Set RPATH on pybind11 bridge module (in site-packages, deeper path)
    # Bridge module is at: usr/share/python/lib/pythonX.X/site-packages/flysight_cpp_bridge*.so
    # Relative path to usr/lib/ is: ../../../../lib
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
            # Verify
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
# Critical for versioned libraries like libtbb.so.12 -> libtbb.so.12.15
# The cp -P flag preserves symlinks during copying

install(CODE "
    message(STATUS \"Verifying library symlinks...\")

    set(LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    # Check for expected symlink patterns
    set(EXPECTED_SYMLINKS
        \"libtbb.so\"
        \"libgtsam.so\"
        \"libkddockwidgets-qt6.so\"
        \"libGeographic.so\"
    )

    foreach(lib \${EXPECTED_SYMLINKS})
        set(lib_path \"\${LIB_DIR}/\${lib}\")
        if(EXISTS \"\${lib_path}\")
            # Check if it's a symlink
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
            # Check for versioned name
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
message(STATUS "  TBB from: ${LINUX_TBB_ROOT}")
message(STATUS "  GTSAM from: ${LINUX_GTSAM_ROOT}")
message(STATUS "  KDDockWidgets from: ${LINUX_KDDW_ROOT}")
message(STATUS "  Target: ${FLYSIGHT_APPDIR_USR}/lib")
