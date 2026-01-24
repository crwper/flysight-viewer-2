# =============================================================================
# DeployThirdPartyWindows.cmake
# =============================================================================
#
# This module copies third-party DLLs (GTSAM, TBB, KDDockWidgets, Boost) to the
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
# TBB DLLs
# =============================================================================

set(TBB_BIN_DIR "${TBB_ROOT}/bin")
if(EXISTS "${TBB_BIN_DIR}")
    message(STATUS "TBB bin directory: ${TBB_BIN_DIR}")

    # Release DLLs
    set(TBB_RELEASE_DLLS
        "${TBB_BIN_DIR}/tbb12.dll"
        "${TBB_BIN_DIR}/tbbmalloc.dll"
    )

    # Debug DLLs
    set(TBB_DEBUG_DLLS
        "${TBB_BIN_DIR}/tbb12_debug.dll"
        "${TBB_BIN_DIR}/tbbmalloc_debug.dll"
    )

    # Install Release DLLs for non-Debug configurations
    foreach(dll ${TBB_RELEASE_DLLS})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Release RelWithDebInfo MinSizeRel
            )
            message(STATUS "  [Release] ${dll}")
        endif()
    endforeach()

    # Install Debug DLLs for Debug configuration
    foreach(dll ${TBB_DEBUG_DLLS})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Debug
            )
            message(STATUS "  [Debug] ${dll}")
        endif()
    endforeach()
else()
    message(WARNING "TBB bin directory not found: ${TBB_BIN_DIR}")
endif()

# =============================================================================
# GTSAM DLLs
# =============================================================================

set(GTSAM_BIN_DIR "${GTSAM_ROOT}/bin")
if(EXISTS "${GTSAM_BIN_DIR}")
    message(STATUS "GTSAM bin directory: ${GTSAM_BIN_DIR}")

    # Release DLLs (GTSAM and its dependencies)
    set(GTSAM_RELEASE_DLLS
        "${GTSAM_BIN_DIR}/gtsam.dll"
        "${GTSAM_BIN_DIR}/metis-gtsam.dll"
        "${GTSAM_BIN_DIR}/cephes-gtsam.dll"
    )

    # Debug DLLs
    set(GTSAM_DEBUG_DLLS
        "${GTSAM_BIN_DIR}/gtsamDebug.dll"
        "${GTSAM_BIN_DIR}/metis-gtsamDebug.dll"
        "${GTSAM_BIN_DIR}/cephes-gtsamDebug.dll"
    )

    # Install Release DLLs
    foreach(dll ${GTSAM_RELEASE_DLLS})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Release RelWithDebInfo MinSizeRel
            )
            message(STATUS "  [Release] ${dll}")
        endif()
    endforeach()

    # Install Debug DLLs
    foreach(dll ${GTSAM_DEBUG_DLLS})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Debug
            )
            message(STATUS "  [Debug] ${dll}")
        endif()
    endforeach()
else()
    message(WARNING "GTSAM bin directory not found: ${GTSAM_BIN_DIR}")
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

# =============================================================================
# Boost DLLs
# =============================================================================

# Boost DLLs are typically in ${BOOST_LIBRARYDIR}
# The naming convention varies based on compiler and settings
# Common patterns: boost_<lib>-vc143-mt-x64-1_87.dll

if(DEFINED BOOST_LIBRARYDIR AND EXISTS "${BOOST_LIBRARYDIR}")
    message(STATUS "Boost library directory: ${BOOST_LIBRARYDIR}")

    # Find Boost DLLs by pattern
    # The exact naming depends on the Boost build configuration
    file(GLOB BOOST_RELEASE_DLLS
        "${BOOST_LIBRARYDIR}/boost_serialization-*.dll"
        "${BOOST_LIBRARYDIR}/boost_timer-*.dll"
        "${BOOST_LIBRARYDIR}/boost_chrono-*.dll"
        "${BOOST_LIBRARYDIR}/boost_system-*.dll"
    )

    # Filter out debug DLLs (typically have -gd- in the name)
    set(BOOST_RELEASE_DLLS_FILTERED)
    foreach(dll ${BOOST_RELEASE_DLLS})
        string(FIND "${dll}" "-gd-" is_debug)
        if(is_debug EQUAL -1)
            list(APPEND BOOST_RELEASE_DLLS_FILTERED "${dll}")
        endif()
    endforeach()

    # Find Debug DLLs (have -gd- in the name)
    file(GLOB BOOST_DEBUG_DLLS
        "${BOOST_LIBRARYDIR}/boost_serialization-*-gd-*.dll"
        "${BOOST_LIBRARYDIR}/boost_timer-*-gd-*.dll"
        "${BOOST_LIBRARYDIR}/boost_chrono-*-gd-*.dll"
        "${BOOST_LIBRARYDIR}/boost_system-*-gd-*.dll"
    )

    # Install Release DLLs
    foreach(dll ${BOOST_RELEASE_DLLS_FILTERED})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Release RelWithDebInfo MinSizeRel
            )
            message(STATUS "  [Release] ${dll}")
        endif()
    endforeach()

    # Install Debug DLLs
    foreach(dll ${BOOST_DEBUG_DLLS})
        if(EXISTS "${dll}")
            install(FILES "${dll}"
                DESTINATION "."
                CONFIGURATIONS Debug
            )
            message(STATUS "  [Debug] ${dll}")
        endif()
    endforeach()

    if(NOT BOOST_RELEASE_DLLS_FILTERED AND NOT BOOST_DEBUG_DLLS)
        message(STATUS "  No Boost DLLs found - Boost may be built as static libraries")
    endif()
else()
    message(STATUS "Boost library directory not found or BOOST_LIBRARYDIR not set")
    message(STATUS "  Boost DLLs will not be bundled (Boost may be static)")
endif()

message(STATUS "----------------------------------------")
message(STATUS "")
