# =============================================================================
# DeployThirdPartyWindows.cmake
# =============================================================================
#
# This module copies third-party DLLs (GeographicLib, KDDockWidgets) to the
# install directory so the application can locate them at runtime.
#
# DLLs are installed to the same directory as the executable (flat structure).
# Debug DLLs are installed for Debug builds, Release DLLs for Release builds.
#
# =============================================================================

if(NOT WIN32)
    return()
endif()

message(STATUS "")
message(STATUS "----------------------------------------")
message(STATUS "Third-Party DLL Deployment Configuration")
message(STATUS "----------------------------------------")

# =============================================================================
# GeographicLib DLLs
# =============================================================================

set(GEOGRAPHIC_BIN_DIR "${GEOGRAPHIC_ROOT}/bin")
if(EXISTS "${GEOGRAPHIC_BIN_DIR}")
    message(STATUS "GeographicLib bin directory: ${GEOGRAPHIC_BIN_DIR}")

    file(GLOB GEOGRAPHIC_RELEASE_DLLS "${GEOGRAPHIC_BIN_DIR}/Geographic*.dll")
    file(GLOB GEOGRAPHIC_DEBUG_DLLS "${GEOGRAPHIC_BIN_DIR}/Geographic*_d.dll")

    # Filter out debug DLLs from release list
    set(GEOGRAPHIC_RELEASE_DLLS_FILTERED)
    foreach(dll ${GEOGRAPHIC_RELEASE_DLLS})
        string(FIND "${dll}" "_d.dll" is_debug)
        if(is_debug EQUAL -1)
            list(APPEND GEOGRAPHIC_RELEASE_DLLS_FILTERED "${dll}")
        endif()
    endforeach()

    foreach(dll ${GEOGRAPHIC_RELEASE_DLLS_FILTERED})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Release RelWithDebInfo MinSizeRel
            )
            message(STATUS "  [Release] ${dll}")
        endif()
    endforeach()

    foreach(dll ${GEOGRAPHIC_DEBUG_DLLS})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Debug
            )
            message(STATUS "  [Debug] ${dll}")
        endif()
    endforeach()
else()
    # GeographicLib may install DLLs to lib/ instead of bin/ on some configurations
    set(GEOGRAPHIC_LIB_DIR "${GEOGRAPHIC_ROOT}/lib")
    file(GLOB GEOGRAPHIC_LIB_DLLS "${GEOGRAPHIC_LIB_DIR}/Geographic*.dll")
    if(GEOGRAPHIC_LIB_DLLS)
        message(STATUS "GeographicLib DLLs found in lib directory: ${GEOGRAPHIC_LIB_DIR}")
        foreach(dll ${GEOGRAPHIC_LIB_DLLS})
            install(FILES "${dll}"
                DESTINATION "."
            )
            message(STATUS "  ${dll}")
        endforeach()
    else()
        message(WARNING "GeographicLib bin directory not found: ${GEOGRAPHIC_BIN_DIR}")
    endif()
endif()

# =============================================================================
# KDDockWidgets DLLs
# =============================================================================

# Allow CI/CD to override KDDW_ROOT via environment
if(DEFINED ENV{KDDW_ROOT} AND NOT DEFINED KDDW_ROOT)
    set(KDDW_ROOT "$ENV{KDDW_ROOT}")
endif()

set(KDDW_BIN_DIR "${KDDW_ROOT}/bin")
if(EXISTS "${KDDW_BIN_DIR}")
    message(STATUS "KDDockWidgets bin directory: ${KDDW_BIN_DIR}")

    # Release DLL
    set(KDDW_RELEASE_DLL "${KDDW_BIN_DIR}/kddockwidgets-qt6.dll")
    if(EXISTS "${KDDW_RELEASE_DLL}")
        install(FILES "${KDDW_RELEASE_DLL}"
            DESTINATION "."
            CONFIGURATIONS Release RelWithDebInfo MinSizeRel
        )
        message(STATUS "  [Release] ${KDDW_RELEASE_DLL}")
    endif()

    # Debug DLL
    set(KDDW_DEBUG_DLL "${KDDW_BIN_DIR}/kddockwidgets-qt6d.dll")
    if(EXISTS "${KDDW_DEBUG_DLL}")
        install(FILES "${KDDW_DEBUG_DLL}"
            DESTINATION "."
            CONFIGURATIONS Debug
        )
        message(STATUS "  [Debug] ${KDDW_DEBUG_DLL}")
    endif()

    # Verify at least one DLL was found
    if(NOT EXISTS "${KDDW_RELEASE_DLL}" AND NOT EXISTS "${KDDW_DEBUG_DLL}")
        message(WARNING "KDDockWidgets DLL not found in ${KDDW_BIN_DIR}")
        message(STATUS "Expected: ${KDDW_RELEASE_DLL} or ${KDDW_DEBUG_DLL}")
        # List what's actually in the bin directory
        file(GLOB _kddw_files "${KDDW_BIN_DIR}/*")
        if(_kddw_files)
            message(STATUS "Available files in ${KDDW_BIN_DIR}:")
            foreach(_f ${_kddw_files})
                message(STATUS "  ${_f}")
            endforeach()
        endif()
    endif()
else()
    message(WARNING "KDDockWidgets bin directory not found: ${KDDW_BIN_DIR}")
    message(STATUS "KDDW_ROOT is set to: ${KDDW_ROOT}")
    if(EXISTS "${KDDW_ROOT}")
        message(STATUS "Contents of KDDW_ROOT:")
        file(GLOB _kddw_root_contents "${KDDW_ROOT}/*")
        foreach(_f ${_kddw_root_contents})
            message(STATUS "  ${_f}")
        endforeach()
    endif()
endif()

message(STATUS "----------------------------------------")
message(STATUS "")
