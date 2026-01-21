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

        foreach(lib \${TBB_LIBS})
            get_filename_component(libname \"\${lib}\" NAME)
            # Use cp -P to preserve symlinks
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
    set(DEST_LIB_DIR \"${FLYSIGHT_APPDIR_USR}/lib\")

    if(EXISTS \"\${KDDW_LIB_DIR}\")
        # Find KDDockWidgets shared libraries
        file(GLOB KDDW_LIBS \"\${KDDW_LIB_DIR}/libkddockwidgets*.so*\")

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
    else()
        message(WARNING \"KDDockWidgets library directory not found: \${KDDW_LIB_DIR}\")
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
# Verify RPATH Configuration
# =============================================================================

install(CODE "
    message(STATUS \"Verifying RPATH configuration...\")

    set(EXECUTABLE \"${FLYSIGHT_APPDIR_USR}/bin/FlySightViewer\")
    set(BRIDGE_MODULE \"${FLYSIGHT_APPDIR_USR}/bin/flysight_cpp_bridge.so\")

    # Check if patchelf is available
    find_program(PATCHELF_EXE patchelf)

    if(PATCHELF_EXE)
        # Set RPATH on executable
        if(EXISTS \"\${EXECUTABLE}\")
            execute_process(
                COMMAND \"\${PATCHELF_EXE}\" --set-rpath \"\\\$ORIGIN/../lib\" \"\${EXECUTABLE}\"
                RESULT_VARIABLE patchelf_result
            )
            if(patchelf_result EQUAL 0)
                message(STATUS \"Set RPATH on FlySightViewer executable\")
            else()
                message(WARNING \"Failed to set RPATH on executable\")
            endif()
        endif()

        # Set RPATH on pybind11 bridge module
        if(EXISTS \"\${BRIDGE_MODULE}\")
            execute_process(
                COMMAND \"\${PATCHELF_EXE}\" --set-rpath \"\\\$ORIGIN/../lib\" \"\${BRIDGE_MODULE}\"
                RESULT_VARIABLE patchelf_result
            )
            if(patchelf_result EQUAL 0)
                message(STATUS \"Set RPATH on flysight_cpp_bridge.so\")
            else()
                message(WARNING \"Failed to set RPATH on bridge module\")
            endif()
        endif()
    else()
        message(STATUS \"patchelf not found - RPATH should be set by CMake build\")
        message(STATUS \"Install patchelf for runtime RPATH modification if needed\")
    endif()
")

# =============================================================================
# Summary
# =============================================================================

message(STATUS "DeployThirdPartyLinux.cmake: Third-party library deployment configured")
message(STATUS "  TBB from: ${LINUX_TBB_ROOT}")
message(STATUS "  GTSAM from: ${LINUX_GTSAM_ROOT}")
message(STATUS "  KDDockWidgets from: ${LINUX_KDDW_ROOT}")
message(STATUS "  Target: ${FLYSIGHT_APPDIR_USR}/lib")
