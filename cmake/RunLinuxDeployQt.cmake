# =============================================================================
# RunLinuxDeployQt.cmake
# =============================================================================
#
# Downloads and runs linuxdeployqt to bundle additional Qt plugins and system
# libraries into the AppDir structure. This is an optional enhancement to the
# Qt 6 CMake deployment API.
#
# Note: linuxdeployqt has compatibility issues with newer glibc versions.
# The Qt 6 CMake deployment API (qt_generate_deploy_app_script()) is the
# primary method for Qt library bundling. This module provides linuxdeployqt
# as an optional enhancement for additional library bundling.
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
# Configuration
# =============================================================================

# linuxdeployqt download configuration
# Note: linuxdeployqt is typically distributed as an AppImage
set(LINUXDEPLOYQT_VERSION "continuous" CACHE STRING "linuxdeployqt version to download")
set(LINUXDEPLOYQT_URL "https://github.com/probonopd/linuxdeployqt/releases/download/${LINUXDEPLOYQT_VERSION}/linuxdeployqt-continuous-x86_64.AppImage")
set(LINUXDEPLOYQT_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/tools")
set(LINUXDEPLOYQT_PATH "${LINUXDEPLOYQT_DOWNLOAD_DIR}/linuxdeployqt")

# =============================================================================
# Download linuxdeployqt at configure time (optional)
# =============================================================================

# Check if linuxdeployqt is needed and available
option(FLYSIGHT_USE_LINUXDEPLOYQT "Use linuxdeployqt for additional library bundling" OFF)

if(FLYSIGHT_USE_LINUXDEPLOYQT)
    message(STATUS "RunLinuxDeployQt: linuxdeployqt integration enabled")

    # Create tools directory
    file(MAKE_DIRECTORY "${LINUXDEPLOYQT_DOWNLOAD_DIR}")

    # Download linuxdeployqt if not present
    if(NOT EXISTS "${LINUXDEPLOYQT_PATH}")
        message(STATUS "Downloading linuxdeployqt...")
        file(DOWNLOAD
            "${LINUXDEPLOYQT_URL}"
            "${LINUXDEPLOYQT_PATH}"
            SHOW_PROGRESS
            STATUS download_status
            TLS_VERIFY ON
        )
        list(GET download_status 0 download_error)
        if(download_error)
            list(GET download_status 1 download_message)
            message(WARNING "Failed to download linuxdeployqt: ${download_message}")
            message(STATUS "Qt 6 CMake deployment API will be used as primary method")
            set(FLYSIGHT_LINUXDEPLOYQT_AVAILABLE FALSE CACHE INTERNAL "")
        else()
            # Make it executable
            execute_process(COMMAND chmod +x "${LINUXDEPLOYQT_PATH}")
            message(STATUS "linuxdeployqt downloaded successfully")
            set(FLYSIGHT_LINUXDEPLOYQT_AVAILABLE TRUE CACHE INTERNAL "")
        endif()
    else()
        message(STATUS "Using cached linuxdeployqt: ${LINUXDEPLOYQT_PATH}")
        set(FLYSIGHT_LINUXDEPLOYQT_AVAILABLE TRUE CACHE INTERNAL "")
    endif()
else()
    message(STATUS "RunLinuxDeployQt: linuxdeployqt integration disabled")
    message(STATUS "  Qt 6 CMake deployment API is the primary method")
    message(STATUS "  Enable with -DFLYSIGHT_USE_LINUXDEPLOYQT=ON if needed")
    set(FLYSIGHT_LINUXDEPLOYQT_AVAILABLE FALSE CACHE INTERNAL "")
endif()

# =============================================================================
# Install-time linuxdeployqt execution
# =============================================================================

if(FLYSIGHT_USE_LINUXDEPLOYQT AND FLYSIGHT_LINUXDEPLOYQT_AVAILABLE)
    install(CODE "
        message(STATUS \"Running linuxdeployqt...\")

        set(LINUXDEPLOYQT \"${LINUXDEPLOYQT_PATH}\")
        set(APPDIR \"${FLYSIGHT_APPDIR_PATH}\")
        set(EXECUTABLE \"\${APPDIR}/usr/bin/FlySightViewer\")

        if(NOT EXISTS \"\${EXECUTABLE}\")
            message(WARNING \"Executable not found at \${EXECUTABLE}\")
            message(STATUS \"Skipping linuxdeployqt - Qt 6 CMake API should have handled deployment\")
        elseif(NOT EXISTS \"\${LINUXDEPLOYQT}\")
            message(WARNING \"linuxdeployqt not found at \${LINUXDEPLOYQT}\")
            message(STATUS \"Skipping linuxdeployqt - Qt 6 CMake API should have handled deployment\")
        else()
            # Set environment for AppImage extraction (works without FUSE)
            set(ENV{APPIMAGE_EXTRACT_AND_RUN} \"1\")

            # Run linuxdeployqt
            # -verbose=1 for some output
            # -no-translations to skip translation files (optional)
            # -exclude-libs=libpython to avoid messing with our bundled Python
            execute_process(
                COMMAND \"\${LINUXDEPLOYQT}\" \"\${EXECUTABLE}\"
                    -verbose=1
                    -no-translations
                    -exclude-libs=libpython
                WORKING_DIRECTORY \"\${APPDIR}\"
                RESULT_VARIABLE ldqt_result
                OUTPUT_VARIABLE ldqt_output
                ERROR_VARIABLE ldqt_error
            )

            if(ldqt_result EQUAL 0)
                message(STATUS \"linuxdeployqt completed successfully\")
            else()
                message(WARNING \"linuxdeployqt failed (this is often OK on newer systems):\")
                message(STATUS \"  Return code: \${ldqt_result}\")
                message(STATUS \"  Error: \${ldqt_error}\")
                message(STATUS \"Qt 6 CMake deployment API is the primary method - continuing\")
            endif()
        endif()
    ")
endif()

# =============================================================================
# Verification
# =============================================================================

# Add install-time verification that Qt XCB plugin is present
install(CODE "
    message(STATUS \"Verifying Qt XCB platform plugin...\")

    set(APPDIR_USR \"${FLYSIGHT_APPDIR_USR}\")

    # Check common locations for the XCB plugin
    set(XCB_LOCATIONS
        \"\${APPDIR_USR}/plugins/platforms/libqxcb.so\"
        \"\${APPDIR_USR}/lib/qt6/plugins/platforms/libqxcb.so\"
        \"\${APPDIR_USR}/lib/plugins/platforms/libqxcb.so\"
    )

    set(xcb_found FALSE)
    foreach(xcb_path \${XCB_LOCATIONS})
        if(EXISTS \"\${xcb_path}\")
            message(STATUS \"  Found: \${xcb_path}\")
            set(xcb_found TRUE)
            break()
        endif()
    endforeach()

    if(NOT xcb_found)
        message(WARNING \"Qt XCB platform plugin not found!\")
        message(STATUS \"The AppImage may not run on systems without Qt installed.\")
        message(STATUS \"Check that Qt deployment completed successfully.\")
    endif()
")

message(STATUS "RunLinuxDeployQt.cmake: Configuration complete")
if(FLYSIGHT_USE_LINUXDEPLOYQT)
    message(STATUS "  linuxdeployqt will run at install time")
else()
    message(STATUS "  linuxdeployqt disabled - using Qt 6 CMake API only")
endif()
