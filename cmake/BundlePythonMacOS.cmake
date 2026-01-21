# =============================================================================
# BundlePythonMacOS.cmake
# =============================================================================
#
# This module downloads and bundles python-build-standalone into a macOS
# .app bundle, providing a portable Python runtime that matches the build-time
# Python version for pybind11 compatibility.
#
# Usage:
#   include(BundlePythonMacOS)
#   bundle_python_macos(
#       TARGET FlySightViewer
#       PYTHON_VERSION "3.13"
#       INSTALL_COMPONENT "Runtime"
#   )
#
# This module:
#   1. Downloads python-build-standalone from GitHub releases
#   2. Supports both arm64 (Apple Silicon) and x86_64 architectures
#   3. Installs Python to Contents/Resources/python/ in the app bundle
#   4. Copies libpython dylib to Contents/Frameworks/
#   5. Installs NumPy into the bundled site-packages
#   6. Copies python_src/ contents to site-packages
#
# =============================================================================

include_guard(GLOBAL)

# Python-build-standalone release information
# Update these when upgrading Python version
set(PYTHON_STANDALONE_VERSION "20241206" CACHE STRING "python-build-standalone release date")
set(PYTHON_STANDALONE_PYTHON_VERSION "3.13.1" CACHE STRING "Python version in the standalone build")

# Base URL for python-build-standalone releases
set(PYTHON_STANDALONE_BASE_URL
    "https://github.com/indygreg/python-build-standalone/releases/download/${PYTHON_STANDALONE_VERSION}"
)

# =============================================================================
# bundle_python_macos()
# =============================================================================
#
# Main function to set up Python bundling for macOS app bundles.
#
# Arguments:
#   TARGET          - The application target name (e.g., FlySightViewer)
#   PYTHON_VERSION  - Optional: Python version to bundle (default: 3.13)
#   INSTALL_COMPONENT - Optional: Install component name (default: Runtime)
#
function(bundle_python_macos)
    cmake_parse_arguments(
        PARSE_ARGV 0
        ARG
        ""
        "TARGET;PYTHON_VERSION;INSTALL_COMPONENT"
        ""
    )

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "bundle_python_macos: TARGET argument is required")
    endif()

    if(NOT ARG_PYTHON_VERSION)
        set(ARG_PYTHON_VERSION "3.13")
    endif()

    if(NOT ARG_INSTALL_COMPONENT)
        set(ARG_INSTALL_COMPONENT "Runtime")
    endif()

    # Determine architecture
    if(CMAKE_OSX_ARCHITECTURES)
        set(_arch ${CMAKE_OSX_ARCHITECTURES})
    else()
        # Default to current system architecture
        execute_process(
            COMMAND uname -m
            OUTPUT_VARIABLE _arch
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    # Map architecture to python-build-standalone naming
    if(_arch STREQUAL "arm64" OR _arch STREQUAL "aarch64")
        set(_pbs_arch "aarch64")
    elseif(_arch STREQUAL "x86_64" OR _arch STREQUAL "AMD64")
        set(_pbs_arch "x86_64")
    else()
        message(WARNING "bundle_python_macos: Unknown architecture '${_arch}', defaulting to x86_64")
        set(_pbs_arch "x86_64")
    endif()

    # Construct download URL
    # Format: cpython-X.Y.Z+YYYYMMDD-ARCH-apple-darwin-install_only.tar.gz
    set(_python_archive_name
        "cpython-${PYTHON_STANDALONE_PYTHON_VERSION}+${PYTHON_STANDALONE_VERSION}-${_pbs_arch}-apple-darwin-install_only.tar.gz"
    )
    set(_python_download_url "${PYTHON_STANDALONE_BASE_URL}/${_python_archive_name}")

    # Download directory in the build tree
    set(_download_dir "${CMAKE_BINARY_DIR}/_python_standalone")
    set(_archive_path "${_download_dir}/${_python_archive_name}")
    set(_extract_dir "${_download_dir}/python")

    message(STATUS "")
    message(STATUS "----------------------------------------")
    message(STATUS "Python Bundling Configuration (macOS)")
    message(STATUS "----------------------------------------")
    message(STATUS "  Target:       ${ARG_TARGET}")
    message(STATUS "  Architecture: ${_pbs_arch}")
    message(STATUS "  Python:       ${PYTHON_STANDALONE_PYTHON_VERSION}")
    message(STATUS "  Download URL: ${_python_download_url}")
    message(STATUS "----------------------------------------")
    message(STATUS "")

    # Download python-build-standalone at configure time
    if(NOT EXISTS "${_extract_dir}/bin/python3")
        message(STATUS "Downloading python-build-standalone...")

        # Create download directory
        file(MAKE_DIRECTORY "${_download_dir}")

        # Download archive
        if(NOT EXISTS "${_archive_path}")
            file(DOWNLOAD
                "${_python_download_url}"
                "${_archive_path}"
                SHOW_PROGRESS
                STATUS _download_status
                TLS_VERIFY ON
            )
            list(GET _download_status 0 _download_error)
            if(_download_error)
                list(GET _download_status 1 _download_message)
                message(FATAL_ERROR
                    "Failed to download python-build-standalone: ${_download_message}\n"
                    "URL: ${_python_download_url}"
                )
            endif()
        endif()

        # Extract archive
        message(STATUS "Extracting python-build-standalone...")
        file(ARCHIVE_EXTRACT
            INPUT "${_archive_path}"
            DESTINATION "${_download_dir}"
        )

        # Verify extraction
        if(NOT EXISTS "${_extract_dir}/bin/python3")
            message(FATAL_ERROR "Failed to extract python-build-standalone - python3 not found")
        endif()

        message(STATUS "python-build-standalone ready at: ${_extract_dir}")
    else()
        message(STATUS "Using cached python-build-standalone at: ${_extract_dir}")
    endif()

    # Get major.minor version for library naming
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)" _version_match "${PYTHON_STANDALONE_PYTHON_VERSION}")
    set(_py_major_minor "${CMAKE_MATCH_1}${CMAKE_MATCH_2}")

    # =======================================================================
    # Install Python runtime to app bundle
    # =======================================================================

    # Install Python standard library and runtime to Contents/Resources/python/
    install(
        DIRECTORY "${_extract_dir}/"
        DESTINATION "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Resources/python"
        COMPONENT ${ARG_INSTALL_COMPONENT}
        USE_SOURCE_PERMISSIONS
        PATTERN "*.pyc" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "test" EXCLUDE
        PATTERN "tests" EXCLUDE
        PATTERN "idle_test" EXCLUDE
        PATTERN "tkinter" EXCLUDE
        PATTERN "turtledemo" EXCLUDE
    )

    # Copy libpython dylib to Contents/Frameworks/
    # The dylib is typically at lib/libpython3.XX.dylib
    install(
        FILES "${_extract_dir}/lib/libpython${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.dylib"
        DESTINATION "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Frameworks"
        COMPONENT ${ARG_INSTALL_COMPONENT}
    )

    # Also copy the symlink (libpython3.dylib -> libpython3.XX.dylib)
    if(EXISTS "${_extract_dir}/lib/libpython3.dylib")
        install(
            FILES "${_extract_dir}/lib/libpython3.dylib"
            DESTINATION "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Frameworks"
            COMPONENT ${ARG_INSTALL_COMPONENT}
        )
    endif()

    # =======================================================================
    # Install NumPy using pip at install time
    # =======================================================================

    # Create a script that installs NumPy into the bundled Python
    set(_install_numpy_script "${CMAKE_BINARY_DIR}/install_numpy_macos.cmake")
    file(WRITE "${_install_numpy_script}" "
# Install NumPy into bundled Python site-packages
set(BUNDLE_PYTHON_DIR \"\${CMAKE_INSTALL_PREFIX}/$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Resources/python\")
set(PYTHON_EXE \"\${BUNDLE_PYTHON_DIR}/bin/python3\")
set(SITE_PACKAGES \"\${BUNDLE_PYTHON_DIR}/lib/python${CMAKE_MATCH_1}.${CMAKE_MATCH_2}/site-packages\")

message(STATUS \"Installing NumPy to bundled Python...\")
message(STATUS \"  Python: \${PYTHON_EXE}\")
message(STATUS \"  Site-packages: \${SITE_PACKAGES}\")

# Ensure pip is available and install numpy
execute_process(
    COMMAND \"\${PYTHON_EXE}\" -m ensurepip --upgrade
    RESULT_VARIABLE _pip_result
    OUTPUT_VARIABLE _pip_output
    ERROR_VARIABLE _pip_error
)
if(_pip_result)
    message(WARNING \"ensurepip failed: \${_pip_error}\")
endif()

execute_process(
    COMMAND \"\${PYTHON_EXE}\" -m pip install --upgrade pip
    RESULT_VARIABLE _pip_upgrade_result
)

execute_process(
    COMMAND \"\${PYTHON_EXE}\" -m pip install numpy --target \"\${SITE_PACKAGES}\"
    RESULT_VARIABLE _numpy_result
    OUTPUT_VARIABLE _numpy_output
    ERROR_VARIABLE _numpy_error
)
if(_numpy_result)
    message(WARNING \"NumPy installation failed: \${_numpy_error}\")
else()
    message(STATUS \"NumPy installed successfully\")
endif()
")
    install(SCRIPT "${_install_numpy_script}" COMPONENT ${ARG_INSTALL_COMPONENT})

    # =======================================================================
    # Install Python SDK files to site-packages
    # =======================================================================

    # flysight_plugin_sdk.py from plugins directory
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/flysight_plugin_sdk.py")
        install(
            FILES "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/flysight_plugin_sdk.py"
            DESTINATION "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Resources/python/lib/python${CMAKE_MATCH_1}.${CMAKE_MATCH_2}/site-packages"
            COMPONENT ${ARG_INSTALL_COMPONENT}
        )
    endif()

    # python_src/ directory contents if it exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../python_src")
        install(
            DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../python_src/"
            DESTINATION "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Resources/python/lib/python${CMAKE_MATCH_1}.${CMAKE_MATCH_2}/site-packages"
            COMPONENT ${ARG_INSTALL_COMPONENT}
            PATTERN "*.pyc" EXCLUDE
            PATTERN "__pycache__" EXCLUDE
        )
    endif()

    # =======================================================================
    # Store Python paths for other modules to use
    # =======================================================================

    set(BUNDLED_PYTHON_HOME
        "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Resources/python"
        CACHE INTERNAL "Path to bundled Python home in app bundle"
    )
    set(BUNDLED_PYTHON_LIBDIR
        "$<TARGET_BUNDLE_DIR:${ARG_TARGET}>/Contents/Frameworks"
        CACHE INTERNAL "Path to bundled Python library in app bundle"
    )
    set(BUNDLED_PYTHON_VERSION "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}"
        CACHE INTERNAL "Bundled Python major.minor version"
    )

endfunction()

# =============================================================================
# Convenience macro to check if we should bundle Python
# =============================================================================
macro(should_bundle_python_macos result_var)
    if(APPLE AND CMAKE_INSTALL_PREFIX)
        set(${result_var} TRUE)
    else()
        set(${result_var} FALSE)
    endif()
endmacro()
