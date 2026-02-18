# =============================================================================
# QtPathDiscovery.cmake
# =============================================================================
#
# Cross-platform Qt path detection for FlySight Viewer.
#
# This module provides functions to reliably discover Qt plugin and QML
# directories regardless of how Qt was installed (system packages, Homebrew,
# jurplel/install-qt-action, etc.).
#
# Usage:
#   include(cmake/QtPathDiscovery.cmake)
#   find_qt6_resource_dirs()
#
# After calling find_qt6_resource_dirs(), the following variables are set:
#   QT6_PREFIX      - Qt installation prefix
#   QT6_PLUGINS_DIR - Path to Qt plugins directory
#   QT6_QML_DIR     - Path to QML modules directory
#
# The function uses multiple discovery strategies in order of reliability:
#   1. QT6_INSTALL_PREFIX if defined (most reliable for CI)
#   2. Qt6_DIR to derive prefix
#   3. Qt6::Core target location as fallback
#
# =============================================================================

# Guard against multiple inclusion
if(_QT_PATH_DISCOVERY_INCLUDED)
    return()
endif()
set(_QT_PATH_DISCOVERY_INCLUDED TRUE)

# =============================================================================
# find_qt6_resource_dirs()
# =============================================================================
# Discovers Qt6 plugin and QML directories across different installation layouts.
#
# Sets in parent scope:
#   QT6_PREFIX      - Qt installation prefix
#   QT6_PLUGINS_DIR - Path to Qt plugins directory
#   QT6_QML_DIR     - Path to QML modules directory
#
function(find_qt6_resource_dirs)
    # Strategy 1: Use QT6_INSTALL_PREFIX if available (most reliable for CI)
    if(DEFINED QT6_INSTALL_PREFIX AND EXISTS "${QT6_INSTALL_PREFIX}")
        set(_qt6_prefix "${QT6_INSTALL_PREFIX}")
        message(STATUS "Qt6 path discovery: Using QT6_INSTALL_PREFIX=${_qt6_prefix}")

    # Strategy 2: Use Qt6_DIR to derive prefix
    # Qt6_DIR typically points to <prefix>/lib/cmake/Qt6/
    elseif(DEFINED Qt6_DIR AND EXISTS "${Qt6_DIR}")
        get_filename_component(_qt6_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
        message(STATUS "Qt6 path discovery: Derived from Qt6_DIR=${Qt6_DIR}")
        message(STATUS "  Computed prefix: ${_qt6_prefix}")

    # Strategy 3: Derive from Qt6::Core target location
    elseif(TARGET Qt6::Core)
        get_target_property(_qt6_core_location Qt6::Core LOCATION)
        if(_qt6_core_location)
            get_filename_component(_qt6_lib_dir "${_qt6_core_location}" DIRECTORY)
            # On Windows, Qt6Core.dll is in bin/; on Unix, in lib/
            get_filename_component(_qt6_prefix "${_qt6_lib_dir}/.." ABSOLUTE)
            message(STATUS "Qt6 path discovery: Derived from Qt6::Core location=${_qt6_core_location}")
            message(STATUS "  Computed prefix: ${_qt6_prefix}")
        else()
            message(WARNING "Qt6 path discovery: Qt6::Core target exists but LOCATION is not set")
            set(_qt6_prefix "")
        endif()
    else()
        message(WARNING "Qt6 path discovery: Unable to determine Qt prefix")
        set(_qt6_prefix "")
    endif()

    # Export prefix to parent scope
    set(QT6_PREFIX "${_qt6_prefix}" PARENT_SCOPE)

    # ==========================================================================
    # Plugin Directory Discovery
    # ==========================================================================
    set(_plugins_dir "")

    if(_qt6_prefix)
        # Platform-specific plugin path search order
        if(WIN32)
            set(_plugin_search_paths
                "${_qt6_prefix}/plugins"
            )
        elseif(APPLE)
            set(_plugin_search_paths
                "${_qt6_prefix}/share/qt6/plugins"  # install-qt-action layout
                "${_qt6_prefix}/plugins"             # Homebrew layout
                "${_qt6_prefix}/lib/qt6/plugins"     # Alternate layout
            )
        else()
            # Linux
            set(_plugin_search_paths
                "${_qt6_prefix}/plugins"             # install-qt-action layout
                "${_qt6_prefix}/lib/qt6/plugins"     # System package layout
                "/usr/lib/x86_64-linux-gnu/qt6/plugins"  # Debian/Ubuntu multiarch
            )
        endif()

        # Search for plugins directory
        foreach(_search_path ${_plugin_search_paths})
            if(EXISTS "${_search_path}")
                set(_plugins_dir "${_search_path}")
                break()
            endif()
        endforeach()

        if(NOT _plugins_dir)
            message(WARNING "Qt6 path discovery: Plugin directory not found")
            message(STATUS "  Searched paths:")
            foreach(_path ${_plugin_search_paths})
                message(STATUS "    - ${_path}")
            endforeach()
        endif()
    endif()

    set(QT6_PLUGINS_DIR "${_plugins_dir}" PARENT_SCOPE)

    # ==========================================================================
    # QML Directory Discovery
    # ==========================================================================
    set(_qml_dir "")

    if(_qt6_prefix)
        # Platform-specific QML path search order
        if(WIN32)
            set(_qml_search_paths
                "${_qt6_prefix}/qml"
            )
        elseif(APPLE)
            set(_qml_search_paths
                "${_qt6_prefix}/qml"                 # Standard layout
                "${_qt6_prefix}/share/qt6/qml"       # install-qt-action layout
                "${_qt6_prefix}/lib/qt6/qml"         # Alternate layout
            )
        else()
            # Linux
            set(_qml_search_paths
                "${_qt6_prefix}/qml"                 # install-qt-action layout
                "${_qt6_prefix}/lib/qt6/qml"         # System package layout
                "/usr/lib/x86_64-linux-gnu/qt6/qml"  # Debian/Ubuntu multiarch
            )
        endif()

        # Search for QML directory
        foreach(_search_path ${_qml_search_paths})
            if(EXISTS "${_search_path}")
                set(_qml_dir "${_search_path}")
                break()
            endif()
        endforeach()

        if(NOT _qml_dir)
            message(WARNING "Qt6 path discovery: QML directory not found")
            message(STATUS "  Searched paths:")
            foreach(_path ${_qml_search_paths})
                message(STATUS "    - ${_path}")
            endforeach()
        endif()
    endif()

    set(QT6_QML_DIR "${_qml_dir}" PARENT_SCOPE)

    # ==========================================================================
    # Diagnostic Output
    # ==========================================================================
    message(STATUS "Qt6 resource discovery results:")
    message(STATUS "  Prefix: ${_qt6_prefix}")
    message(STATUS "  Plugins: ${_plugins_dir}")
    message(STATUS "  QML: ${_qml_dir}")

endfunction()
