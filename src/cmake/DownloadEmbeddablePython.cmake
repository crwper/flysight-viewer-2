# Download and stage the CPython “embeddable” distribution (Windows)
# or install-only tarball (macOS / Linux) at configure time, then copy
# its contents into <install>/python when `cmake --install` is run.

set(PY_VERSION        3.13.0)

if(WIN32)
    set(PY_ARCHIVE    "python-${PY_VERSION}-embed-amd64.zip")
    set(PY_URL        "https://www.python.org/ftp/python/${PY_VERSION}/${PY_ARCHIVE}")
else()
    set(PY_ARCHIVE    "Python-${PY_VERSION}.tgz")
    set(PY_URL        "https://www.python.org/ftp/python/${PY_VERSION}/${PY_ARCHIVE}")
endif()

set(PY_ARCHIVE_PATH   "${CMAKE_BINARY_DIR}/${PY_ARCHIVE}")
set(PY_EXTRACT_DIR    "${CMAKE_BINARY_DIR}/python-${PY_VERSION}")

# Fetch & unpack only once per build tree
if(NOT EXISTS "${PY_EXTRACT_DIR}")
    message(STATUS "Downloading embeddable Python ${PY_VERSION}...")
    file(DOWNLOAD "${PY_URL}" "${PY_ARCHIVE_PATH}" SHOW_PROGRESS)

    if(NOT EXISTS "${PY_ARCHIVE_PATH}")
        message(FATAL_ERROR "Failed to download ${PY_URL}")
    endif()

    file(ARCHIVE_EXTRACT
         INPUT          "${PY_ARCHIVE_PATH}"
         DESTINATION    "${PY_EXTRACT_DIR}")
endif()

# Copy everything into the install tree
install(DIRECTORY "${PY_EXTRACT_DIR}/"
        DESTINATION python
        USE_SOURCE_PERMISSIONS)
