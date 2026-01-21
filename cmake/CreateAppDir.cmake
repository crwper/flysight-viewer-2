# =============================================================================
# CreateAppDir.cmake
# =============================================================================
#
# Creates the Linux AppDir directory structure required for AppImage creation.
# This module sets up the AppRun launcher, desktop file, and proper directory
# hierarchy following the AppImage specification.
#
# AppDir Structure:
#   FlySightViewer.AppDir/
#   ├── AppRun                    (executable launcher script)
#   ├── FlySightViewer.desktop    (desktop entry file)
#   ├── FlySightViewer.png        (application icon)
#   └── usr/
#       ├── bin/                  (executables)
#       ├── lib/                  (shared libraries)
#       └── share/
#           ├── python/           (bundled Python runtime)
#           └── applications/     (desktop files)
#
# =============================================================================

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

# Define the AppDir location within the install prefix
set(APPDIR_NAME "FlySightViewer.AppDir")
set(APPDIR_PATH "${CMAKE_INSTALL_PREFIX}/${APPDIR_NAME}")
set(APPDIR_USR "${APPDIR_PATH}/usr")

# Export these for use by other modules
set(FLYSIGHT_APPDIR_PATH "${APPDIR_PATH}" CACHE INTERNAL "Path to AppDir")
set(FLYSIGHT_APPDIR_USR "${APPDIR_USR}" CACHE INTERNAL "Path to AppDir/usr")

# =============================================================================
# AppRun Script
# =============================================================================
# The AppRun script is the entry point for the AppImage. It sets up the
# environment variables needed to locate libraries, Qt plugins, and Python.

set(APPRUN_CONTENT [=[#!/bin/bash
# AppRun launcher for FlySight Viewer AppImage
# Sets up environment variables and launches the application

# Get the directory where AppRun is located (the AppDir root)
HERE="$(dirname "$(readlink -f "${0}")")"

# Library paths for Qt, third-party libs, and Python
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH}"

# Qt plugin paths
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${HERE}/usr/lib/qt6/plugins:${QT_PLUGIN_PATH}"

# Qt QML paths
export QML2_IMPORT_PATH="${HERE}/usr/qml:${HERE}/usr/lib/qt6/qml:${QML2_IMPORT_PATH}"

# Python environment for bundled runtime
if [ -d "${HERE}/usr/share/python" ]; then
    export PYTHONHOME="${HERE}/usr/share/python"
    export PYTHONPATH="${HERE}/usr/share/python/lib/python3.13/site-packages:${PYTHONPATH}"
fi

# XDG paths for desktop integration
export XDG_DATA_DIRS="${HERE}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

# Launch the application
exec "${HERE}/usr/bin/FlySightViewer" "$@"
]=])

# =============================================================================
# Desktop Entry File
# =============================================================================
# The desktop file provides metadata for desktop integration.

set(DESKTOP_CONTENT [=[[Desktop Entry]
Type=Application
Name=FlySight Viewer
Comment=GPS flight data analysis and visualization
Exec=FlySightViewer %F
Icon=FlySightViewer
Terminal=false
Categories=Science;DataVisualization;
MimeType=application/x-flysight;
Keywords=GPS;flight;skydiving;wingsuit;analysis;
]=])

# =============================================================================
# Install Commands
# =============================================================================

# Create AppDir directory structure at install time
install(CODE "
    message(STATUS \"Creating AppDir structure at: ${APPDIR_PATH}\")
    file(MAKE_DIRECTORY \"${APPDIR_PATH}\")
    file(MAKE_DIRECTORY \"${APPDIR_USR}/bin\")
    file(MAKE_DIRECTORY \"${APPDIR_USR}/lib\")
    file(MAKE_DIRECTORY \"${APPDIR_USR}/share/applications\")
    file(MAKE_DIRECTORY \"${APPDIR_USR}/share/python\")
    file(MAKE_DIRECTORY \"${APPDIR_USR}/plugins\")
    file(MAKE_DIRECTORY \"${APPDIR_USR}/qml\")
")

# Write the AppRun script
install(CODE "
    set(APPRUN_CONTENT \"${APPRUN_CONTENT}\")
    file(WRITE \"${APPDIR_PATH}/AppRun\" \"\${APPRUN_CONTENT}\")
    # Make AppRun executable
    execute_process(COMMAND chmod +x \"${APPDIR_PATH}/AppRun\")
    message(STATUS \"Created AppRun script\")
")

# Write the desktop file
install(CODE "
    set(DESKTOP_CONTENT \"${DESKTOP_CONTENT}\")
    file(WRITE \"${APPDIR_PATH}/FlySightViewer.desktop\" \"\${DESKTOP_CONTENT}\")
    file(COPY \"${APPDIR_PATH}/FlySightViewer.desktop\"
         DESTINATION \"${APPDIR_USR}/share/applications\")
    message(STATUS \"Created desktop file\")
")

# Install the executable to AppDir/usr/bin
install(TARGETS FlySightViewer
    RUNTIME DESTINATION "${APPDIR_NAME}/usr/bin"
    COMPONENT AppImage
)

# Install the pybind11 bridge module to AppDir/usr/bin (next to executable)
install(TARGETS flysight_cpp_bridge
    LIBRARY DESTINATION "${APPDIR_NAME}/usr/bin"
    COMPONENT AppImage
)

# Install the icon (if it exists) to AppDir root
set(ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/FlySightViewer.png")
install(CODE "
    message(STATUS \"Installing application icon...\")

    set(ICON_SRC \"${ICON_SOURCE}\")
    set(APPDIR \"${APPDIR_PATH}\")

    if(EXISTS \"\${ICON_SRC}\")
        file(COPY \"\${ICON_SRC}\" DESTINATION \"\${APPDIR}\")
        message(STATUS \"  Installed icon from \${ICON_SRC}\")
    else()
        message(STATUS \"  Icon not found at \${ICON_SRC}, creating placeholder...\")

        # Create a minimal valid PNG placeholder
        # This is the binary content of a 1x1 blue PNG
        # Header: PNG signature (8 bytes) + IHDR chunk + IDAT chunk + IEND chunk
        # In production, replace with a proper 256x256 icon

        # For now, create an empty file and warn the user
        file(WRITE \"\${APPDIR}/FlySightViewer.png\" \"\")
        message(WARNING \"Created empty placeholder icon.\")
        message(STATUS \"Please provide a proper 256x256 PNG icon at:\")
        message(STATUS \"  ${CMAKE_CURRENT_SOURCE_DIR}/resources/FlySightViewer.png\")
    endif()

    # Also install to usr/share/icons for XDG compliance
    file(MAKE_DIRECTORY \"\${APPDIR}/usr/share/icons/hicolor/256x256/apps\")
    if(EXISTS \"\${APPDIR}/FlySightViewer.png\")
        file(COPY \"\${APPDIR}/FlySightViewer.png\"
             DESTINATION \"\${APPDIR}/usr/share/icons/hicolor/256x256/apps\")
    endif()
")

# Install plugin SDK and Python sources to site-packages
# Note: The Python version path is set by BundlePythonLinux.cmake
# Default to 3.13 but this will be populated correctly when Python is bundled
set(BUNDLE_PYTHON_VERSION "3.13" CACHE STRING "Python version for site-packages path")
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../plugins/"
    DESTINATION "${APPDIR_NAME}/usr/share/python/lib/python${BUNDLE_PYTHON_VERSION}/site-packages"
    COMPONENT AppImage
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
)

message(STATUS "CreateAppDir.cmake: AppDir structure will be created at install time")
message(STATUS "  AppDir path: ${APPDIR_PATH}")
