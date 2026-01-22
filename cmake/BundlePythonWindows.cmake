# =============================================================================
# BundlePythonWindows.cmake
# =============================================================================
#
# This module downloads and bundles the official Python embeddable package into
# the install directory, providing a portable Python runtime for Windows.
#
# The module:
#   1. Downloads the Python embeddable package at configure time
#   2. Modifies the ._pth file to enable site-packages
#   3. Installs pip and NumPy into the bundled Python
#   4. Creates install rules to copy the runtime to the install directory
#
# The Python version is derived from find_package(Python) to ensure pybind11
# compatibility.
#
# =============================================================================

if(NOT WIN32)
    return()
endif()

# Ensure Python was found before this module is included
if(NOT DEFINED Python_VERSION_MAJOR OR NOT DEFINED Python_VERSION_MINOR)
    message(FATAL_ERROR "BundlePythonWindows.cmake requires find_package(Python) to be called first")
endif()

# =============================================================================
# Configuration
# =============================================================================

set(PYTHON_EMBED_VERSION "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}.${Python_VERSION_PATCH}")
set(PYTHON_EMBED_VERSION_NODOT "${Python_VERSION_MAJOR}${Python_VERSION_MINOR}")

# Construct the download URL for the Python embeddable package
# Format: https://www.python.org/ftp/python/3.x.y/python-3.x.y-embed-amd64.zip
set(PYTHON_EMBED_URL "https://www.python.org/ftp/python/${PYTHON_EMBED_VERSION}/python-${PYTHON_EMBED_VERSION}-embed-amd64.zip")
set(PYTHON_EMBED_ARCHIVE "${CMAKE_BINARY_DIR}/python-embed/python-${PYTHON_EMBED_VERSION}-embed-amd64.zip")
set(PYTHON_EMBED_DIR "${CMAKE_BINARY_DIR}/python-embed/python-${PYTHON_EMBED_VERSION}")

message(STATUS "")
message(STATUS "----------------------------------------")
message(STATUS "Python Embeddable Package Configuration")
message(STATUS "----------------------------------------")
message(STATUS "Python version: ${PYTHON_EMBED_VERSION}")
message(STATUS "Download URL: ${PYTHON_EMBED_URL}")
message(STATUS "Local directory: ${PYTHON_EMBED_DIR}")
message(STATUS "----------------------------------------")
message(STATUS "")

# =============================================================================
# Download Python Embeddable Package
# =============================================================================

if(NOT EXISTS "${PYTHON_EMBED_DIR}/python.exe")
    message(STATUS "Downloading Python ${PYTHON_EMBED_VERSION} embeddable package...")

    # Create directory for download
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/python-embed")

    # Download the archive
    if(NOT EXISTS "${PYTHON_EMBED_ARCHIVE}")
        file(DOWNLOAD
            "${PYTHON_EMBED_URL}"
            "${PYTHON_EMBED_ARCHIVE}"
            SHOW_PROGRESS
            STATUS download_status
            TLS_VERIFY ON
        )
        list(GET download_status 0 download_code)
        list(GET download_status 1 download_message)
        if(NOT download_code EQUAL 0)
            message(FATAL_ERROR "Failed to download Python embeddable package: ${download_message}")
        endif()
    endif()

    # Extract the archive
    message(STATUS "Extracting Python embeddable package...")
    file(ARCHIVE_EXTRACT
        INPUT "${PYTHON_EMBED_ARCHIVE}"
        DESTINATION "${PYTHON_EMBED_DIR}"
    )

    # =============================================================================
    # Modify ._pth File to Enable site-packages
    # =============================================================================

    set(PTH_FILE "${PYTHON_EMBED_DIR}/python${PYTHON_EMBED_VERSION_NODOT}._pth")
    if(EXISTS "${PTH_FILE}")
        message(STATUS "Modifying ${PTH_FILE} to enable site-packages...")
        file(READ "${PTH_FILE}" PTH_CONTENT)
        # The ._pth file has "import site" commented out - uncomment it
        string(REPLACE "#import site" "import site" PTH_CONTENT "${PTH_CONTENT}")
        # Add Lib/site-packages to the path
        string(APPEND PTH_CONTENT "\nLib/site-packages\n")
        file(WRITE "${PTH_FILE}" "${PTH_CONTENT}")
    else()
        message(WARNING "Python ._pth file not found at ${PTH_FILE}")
    endif()

    # =============================================================================
    # Create Lib/site-packages Directory
    # =============================================================================

    file(MAKE_DIRECTORY "${PYTHON_EMBED_DIR}/Lib/site-packages")

    # =============================================================================
    # Install pip using get-pip.py (ensurepip is not available in embeddable Python)
    # =============================================================================

    set(GET_PIP_URL "https://bootstrap.pypa.io/get-pip.py")
    set(GET_PIP_PATH "${CMAKE_BINARY_DIR}/python-embed/get-pip.py")

    message(STATUS "Downloading get-pip.py...")
    if(NOT EXISTS "${GET_PIP_PATH}")
        file(DOWNLOAD
            "${GET_PIP_URL}"
            "${GET_PIP_PATH}"
            SHOW_PROGRESS
            STATUS getpip_download_status
            TLS_VERIFY ON
        )
        list(GET getpip_download_status 0 getpip_code)
        list(GET getpip_download_status 1 getpip_message)
        if(NOT getpip_code EQUAL 0)
            message(FATAL_ERROR "Failed to download get-pip.py: ${getpip_message}")
        endif()
    endif()

    message(STATUS "Installing pip into bundled Python using get-pip.py...")
    execute_process(
        COMMAND "${PYTHON_EMBED_DIR}/python.exe" "${GET_PIP_PATH}" --no-warn-script-location
        WORKING_DIRECTORY "${PYTHON_EMBED_DIR}"
        RESULT_VARIABLE pip_result
        OUTPUT_VARIABLE pip_output
        ERROR_VARIABLE pip_error
    )
    if(NOT pip_result EQUAL 0)
        message(FATAL_ERROR "Failed to install pip:\nOutput: ${pip_output}\nError: ${pip_error}")
    else()
        message(STATUS "pip installed successfully")
    endif()

    # =============================================================================
    # Install NumPy
    # =============================================================================

    message(STATUS "Installing NumPy into bundled Python...")
    execute_process(
        COMMAND "${PYTHON_EMBED_DIR}/python.exe" -m pip install numpy --target "${PYTHON_EMBED_DIR}/Lib/site-packages"
        WORKING_DIRECTORY "${PYTHON_EMBED_DIR}"
        RESULT_VARIABLE numpy_result
        OUTPUT_VARIABLE numpy_output
        ERROR_VARIABLE numpy_error
    )
    if(NOT numpy_result EQUAL 0)
        message(FATAL_ERROR "Failed to install NumPy:\nOutput: ${numpy_output}\nError: ${numpy_error}")
    else()
        message(STATUS "NumPy installed successfully")
    endif()

else()
    message(STATUS "Python embeddable package already downloaded at ${PYTHON_EMBED_DIR}")
endif()

# =============================================================================
# Install Rules for Python Runtime
# =============================================================================

# Install Python runtime files to <install>/python/
# This includes python.exe, pythonXX.dll, pythonXX.zip, the ._pth file,
# and site-packages (including NumPy with its .pyd extensions)
install(
    DIRECTORY "${PYTHON_EMBED_DIR}/"
    DESTINATION "python"
    PATTERN "__pycache__" EXCLUDE
    PATTERN "*.pyc" EXCLUDE
)

# Export the Python embed directory for use by other modules
set(PYTHON_EMBED_DIR "${PYTHON_EMBED_DIR}" PARENT_SCOPE)
set(PYTHON_EMBED_VERSION_NODOT "${PYTHON_EMBED_VERSION_NODOT}" PARENT_SCOPE)

message(STATUS "Python embeddable package configured for installation to: python/")
