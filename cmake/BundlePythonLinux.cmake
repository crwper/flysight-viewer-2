# =============================================================================
# BundlePythonLinux.cmake
# =============================================================================
#
# Downloads and bundles python-build-standalone into the AppDir, providing a
# portable Python runtime that matches the build-time Python version for
# pybind11 compatibility.
#
# The module:
# 1. Downloads python-build-standalone from GitHub releases at configure time
# 2. Supports both x86_64 and aarch64 architectures
# 3. Installs Python to usr/share/python/ in the AppDir
# 4. Installs NumPy and copies python_src/ to site-packages
# 5. Copies libpython shared library to lib directory with proper symlinks
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

# Python version to bundle - should match the build-time Python version
set(BUNDLE_PYTHON_VERSION "3.13" CACHE STRING "Python version to bundle")
set(BUNDLE_PYTHON_FULL_VERSION "3.13.1" CACHE STRING "Full Python version")

# python-build-standalone release tag
# Check https://github.com/indygreg/python-build-standalone/releases for latest
set(PBS_RELEASE_TAG "20241206" CACHE STRING "python-build-standalone release tag")

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    set(PBS_ARCH "aarch64")
else()
    set(PBS_ARCH "x86_64")
endif()

# Build the download URL
# Format: cpython-{version}+{tag}-{arch}-unknown-linux-gnu-install_only_stripped.tar.gz
set(PBS_FILENAME "cpython-${BUNDLE_PYTHON_FULL_VERSION}+${PBS_RELEASE_TAG}-${PBS_ARCH}-unknown-linux-gnu-install_only_stripped.tar.gz")
set(PBS_URL "https://github.com/indygreg/python-build-standalone/releases/download/${PBS_RELEASE_TAG}/${PBS_FILENAME}")

# Local paths
set(PBS_DOWNLOAD_DIR "${CMAKE_BINARY_DIR}/python-standalone")
set(PBS_ARCHIVE_PATH "${PBS_DOWNLOAD_DIR}/${PBS_FILENAME}")
set(PBS_EXTRACT_DIR "${PBS_DOWNLOAD_DIR}/extracted")

# =============================================================================
# Download python-build-standalone at configure time
# =============================================================================

message(STATUS "BundlePythonLinux: Configuring Python ${BUNDLE_PYTHON_FULL_VERSION} bundling")
message(STATUS "  Architecture: ${PBS_ARCH}")
message(STATUS "  Download URL: ${PBS_URL}")

# Create download directory
file(MAKE_DIRECTORY "${PBS_DOWNLOAD_DIR}")

# Download the archive if it doesn't exist
if(NOT EXISTS "${PBS_ARCHIVE_PATH}")
    message(STATUS "Downloading python-build-standalone...")
    file(DOWNLOAD
        "${PBS_URL}"
        "${PBS_ARCHIVE_PATH}"
        SHOW_PROGRESS
        STATUS download_status
        TLS_VERIFY ON
    )
    list(GET download_status 0 download_error)
    if(download_error)
        list(GET download_status 1 download_message)
        message(WARNING "Failed to download python-build-standalone: ${download_message}")
        message(WARNING "Python bundling will be skipped. AppImage may not work correctly.")
        set(FLYSIGHT_PYTHON_BUNDLING_AVAILABLE FALSE CACHE INTERNAL "")
        return()
    endif()
    message(STATUS "Download complete: ${PBS_ARCHIVE_PATH}")
else()
    message(STATUS "Using cached python-build-standalone: ${PBS_ARCHIVE_PATH}")
endif()

# Extract the archive if not already extracted
set(PBS_PYTHON_DIR "${PBS_EXTRACT_DIR}/python")
if(NOT EXISTS "${PBS_PYTHON_DIR}")
    message(STATUS "Extracting python-build-standalone...")
    file(MAKE_DIRECTORY "${PBS_EXTRACT_DIR}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${PBS_ARCHIVE_PATH}"
        WORKING_DIRECTORY "${PBS_EXTRACT_DIR}"
        RESULT_VARIABLE extract_result
    )
    if(extract_result)
        message(WARNING "Failed to extract python-build-standalone")
        set(FLYSIGHT_PYTHON_BUNDLING_AVAILABLE FALSE CACHE INTERNAL "")
        return()
    endif()
    message(STATUS "Extraction complete")
endif()

set(FLYSIGHT_PYTHON_BUNDLING_AVAILABLE TRUE CACHE INTERNAL "")
set(FLYSIGHT_PBS_PYTHON_DIR "${PBS_PYTHON_DIR}" CACHE INTERNAL "Path to extracted Python")

# =============================================================================
# Install Commands
# =============================================================================

# Install Python runtime to AppDir
install(CODE "
    message(STATUS \"Installing bundled Python runtime...\")

    set(PBS_PYTHON_DIR \"${PBS_PYTHON_DIR}\")
    set(APPDIR_USR \"${FLYSIGHT_APPDIR_USR}\")
    set(PYTHON_VERSION \"${BUNDLE_PYTHON_VERSION}\")

    # Create target directories
    file(MAKE_DIRECTORY \"\${APPDIR_USR}/share/python\")
    file(MAKE_DIRECTORY \"\${APPDIR_USR}/lib\")

    # Copy Python installation
    if(EXISTS \"\${PBS_PYTHON_DIR}\")
        # Copy the entire Python directory structure
        file(COPY \"\${PBS_PYTHON_DIR}/\"
             DESTINATION \"\${APPDIR_USR}/share/python\"
             PATTERN \"__pycache__\" EXCLUDE
             PATTERN \"*.pyc\" EXCLUDE
             PATTERN \"test\" EXCLUDE
             PATTERN \"tests\" EXCLUDE
             PATTERN \"idle_test\" EXCLUDE
        )
        message(STATUS \"Copied Python runtime to \${APPDIR_USR}/share/python\")

        # Copy libpython to lib directory
        file(GLOB LIBPYTHON_FILES \"\${PBS_PYTHON_DIR}/lib/libpython*.so*\")
        foreach(lib \${LIBPYTHON_FILES})
            get_filename_component(libname \"\${lib}\" NAME)
            # Copy file (preserving symlinks)
            execute_process(
                COMMAND cp -P \"\${lib}\" \"\${APPDIR_USR}/lib/\"
                RESULT_VARIABLE cp_result
            )
            if(NOT cp_result)
                message(STATUS \"Copied \${libname} to lib/\")
            endif()
        endforeach()

        # Also ensure libpython is in share/python/lib for PYTHONHOME
        file(GLOB LIBPYTHON_FILES \"\${APPDIR_USR}/share/python/lib/libpython*.so*\")
        foreach(lib \${LIBPYTHON_FILES})
            get_filename_component(libname \"\${lib}\" NAME)
            if(NOT EXISTS \"\${APPDIR_USR}/lib/\${libname}\")
                execute_process(
                    COMMAND cp -P \"\${lib}\" \"\${APPDIR_USR}/lib/\"
                )
            endif()
        endforeach()
    else()
        message(WARNING \"Python source directory not found: \${PBS_PYTHON_DIR}\")
    endif()
")

# Install NumPy into the bundled Python
install(CODE "
    message(STATUS \"Installing NumPy into bundled Python...\")

    set(APPDIR_USR \"${FLYSIGHT_APPDIR_USR}\")
    set(PYTHON_VERSION \"${BUNDLE_PYTHON_VERSION}\")
    set(SITE_PACKAGES \"\${APPDIR_USR}/share/python/lib/python\${PYTHON_VERSION}/site-packages\")
    set(BUNDLED_PYTHON \"\${APPDIR_USR}/share/python/bin/python3\")

    if(EXISTS \"\${BUNDLED_PYTHON}\")
        # Use pip from the bundled Python to install NumPy
        execute_process(
            COMMAND \"\${BUNDLED_PYTHON}\" -m pip install --target \"\${SITE_PACKAGES}\" numpy
            RESULT_VARIABLE pip_result
            OUTPUT_VARIABLE pip_output
            ERROR_VARIABLE pip_error
        )
        if(pip_result)
            message(WARNING \"Failed to install NumPy: \${pip_error}\")
            message(STATUS \"Trying alternative method...\")
            # Try using ensurepip first
            execute_process(
                COMMAND \"\${BUNDLED_PYTHON}\" -m ensurepip --default-pip
                RESULT_VARIABLE ensurepip_result
            )
            if(NOT ensurepip_result)
                execute_process(
                    COMMAND \"\${BUNDLED_PYTHON}\" -m pip install --target \"\${SITE_PACKAGES}\" numpy
                    RESULT_VARIABLE pip_result2
                )
                if(NOT pip_result2)
                    message(STATUS \"NumPy installed successfully (after ensurepip)\")
                endif()
            endif()
        else()
            message(STATUS \"NumPy installed successfully\")
        endif()
    else()
        message(WARNING \"Bundled Python not found at \${BUNDLED_PYTHON}\")
    endif()
")

# Install python_src contents to site-packages
install(CODE "
    message(STATUS \"Installing FlySight Python SDK...\")

    set(APPDIR_USR \"${FLYSIGHT_APPDIR_USR}\")
    set(PYTHON_VERSION \"${BUNDLE_PYTHON_VERSION}\")
    set(SITE_PACKAGES \"\${APPDIR_USR}/share/python/lib/python\${PYTHON_VERSION}/site-packages\")
    set(PYTHON_SRC_DIR \"${CMAKE_CURRENT_SOURCE_DIR}/../plugins\")

    file(MAKE_DIRECTORY \"\${SITE_PACKAGES}\")

    # Copy plugin SDK files
    if(EXISTS \"\${PYTHON_SRC_DIR}\")
        file(GLOB PYTHON_FILES \"\${PYTHON_SRC_DIR}/*.py\")
        foreach(pyfile \${PYTHON_FILES})
            get_filename_component(filename \"\${pyfile}\" NAME)
            file(COPY \"\${pyfile}\" DESTINATION \"\${SITE_PACKAGES}\")
            message(STATUS \"Installed \${filename} to site-packages\")
        endforeach()
    endif()
")

# Ensure lib-dynload is accessible
install(CODE "
    message(STATUS \"Setting up lib-dynload paths...\")

    set(APPDIR_USR \"${FLYSIGHT_APPDIR_USR}\")
    set(PYTHON_VERSION \"${BUNDLE_PYTHON_VERSION}\")

    # The lib-dynload directory contains C extension modules
    set(LIB_DYNLOAD_SRC \"\${APPDIR_USR}/share/python/lib/python\${PYTHON_VERSION}/lib-dynload\")
    set(LIB_DYNLOAD_DST \"\${APPDIR_USR}/lib/python\${PYTHON_VERSION}/lib-dynload\")

    if(EXISTS \"\${LIB_DYNLOAD_SRC}\")
        # Create symlink for lib-dynload in the lib directory
        get_filename_component(LIB_DYNLOAD_DST_PARENT \"\${LIB_DYNLOAD_DST}\" DIRECTORY)
        file(MAKE_DIRECTORY \"\${LIB_DYNLOAD_DST_PARENT}\")

        if(NOT EXISTS \"\${LIB_DYNLOAD_DST}\")
            execute_process(
                COMMAND ln -sf \"\${LIB_DYNLOAD_SRC}\" \"\${LIB_DYNLOAD_DST}\"
            )
            message(STATUS \"Created lib-dynload symlink\")
        endif()
    endif()
")

message(STATUS "BundlePythonLinux.cmake: Python bundling configured")
message(STATUS "  Python will be installed to: ${FLYSIGHT_APPDIR_USR}/share/python")
