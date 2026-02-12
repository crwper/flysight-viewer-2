cmake_minimum_required(VERSION 3.18)

project(FlySightViewer VERSION 0.1 LANGUAGES CXX)

# ─────────────────────────────── global settings
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Install into <build>/install by default so devs don't need admin rights
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE PATH "Install path prefix." FORCE)
endif()

# =============================================================================
# Variable Definition Conventions
# =============================================================================
#
# Third-party path variables (defined once, cached, can be overridden):
#   THIRD_PARTY_DIR, EIGEN_DIR, QCUSTOMPLOT_ROOT, QCUSTOMPLOT_DIR,
#   GTSAM_ROOT, TBB_ROOT, KDDW_ROOT, BOOST_ROOT, BOOST_LIBRARYDIR
#
# These are defined as CACHE PATH variables at the start of this file.
# Do not redefine these variables elsewhere - CMake's cache ensures
# command-line overrides work correctly.
#
# Platform-specific modules (cmake/Bundle*, cmake/Deploy*, cmake/Create*)
# may define their own CACHE variables, but should avoid redefining
# variables already defined here.
#
# When adding new third-party dependencies:
# 1. Add path variable in the "Third-party root" section below
# 2. Use the variable where needed without redefinition
# 3. Document the variable in this conventions block
#
# =============================================================================

# Third-party root (editable in Qt Creator / CMake cache)
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../third-party"
    CACHE PATH "Path to third-party dependencies")

set(EIGEN_DIR      "${THIRD_PARTY_DIR}/Eigen"                 CACHE PATH "Eigen include dir")
set(QCUSTOMPLOT_ROOT "${THIRD_PARTY_DIR}/QCustomPlot"         CACHE PATH "QCustomPlot root")
set(QCUSTOMPLOT_DIR  "${QCUSTOMPLOT_ROOT}/QCustomPlot"        CACHE PATH "QCustomPlot sources dir")
set(GTSAM_ROOT     "${THIRD_PARTY_DIR}/GTSAM-install"         CACHE PATH "GTSAM install prefix")
set(TBB_ROOT       "${THIRD_PARTY_DIR}/oneTBB-install"        CACHE PATH "oneTBB install prefix")
set(KDDW_ROOT      "${THIRD_PARTY_DIR}/KDDockWidgets-install" CACHE PATH "KDDockWidgets install prefix")

# Allow environment variable override for CI/CD
if(DEFINED ENV{KDDW_ROOT} AND NOT KDDW_ROOT)
    set(KDDW_ROOT "$ENV{KDDW_ROOT}")
endif()

# Make KDDW discoverable by find_package() without requiring external -DCMAKE_PREFIX_PATH=...
list(PREPEND CMAKE_PREFIX_PATH "${KDDW_ROOT}")

# KDDockWidgets CMake config location varies by platform
# Add common subdirectories to help find_package locate the config
set(_KDDW_CMAKE_PATHS
    "${KDDW_ROOT}/lib/cmake/KDDockWidgets-qt6"
    "${KDDW_ROOT}/lib64/cmake/KDDockWidgets-qt6"
    "${KDDW_ROOT}/share/cmake/KDDockWidgets-qt6"
)
foreach(_kddw_path ${_KDDW_CMAKE_PATHS})
    if(EXISTS "${_kddw_path}/KDDockWidgets-qt6Config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_kddw_path}/..")
        message(STATUS "Found KDDockWidgets config at: ${_kddw_path}")
        break()
    endif()
endforeach()

# Include cross-platform Boost discovery
# This sets BOOST_ROOT and BOOST_LIBRARYDIR with platform-appropriate defaults
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/BoostDiscovery.cmake")

# ─────────────────────────────── Qt
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets PrintSupport QuickWidgets Quick Qml Location Positioning Multimedia MultimediaWidgets)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  # Nice-to-have; safe to omit if you prefer older style
  include(${Qt6_DIR}/Qt6Config.cmake OPTIONAL RESULT_VARIABLE _qt6cfg)
  if(_qt6cfg)
    qt_standard_project_setup()
  endif()
endif()

# ─────────────────────────────── Python / pybind11
# Full Python install for embedding + extension module build
find_package(Python 3.10 REQUIRED COMPONENTS Interpreter Development Development.Embed)

# Export Python version info for bundling modules
# This ensures bundled Python matches build-time Python for pybind11 compatibility
set(FLYSIGHT_PYTHON_VERSION "${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}" CACHE INTERNAL "Python major.minor version")
set(FLYSIGHT_PYTHON_FULL_VERSION "${Python_VERSION}" CACHE INTERNAL "Python full version")

# =============================================================================
# Python Configuration Validation and Diagnostics
# =============================================================================
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Python Configuration")
message(STATUS "========================================")
message(STATUS "  Interpreter: ${Python_EXECUTABLE}")
message(STATUS "  Version: ${Python_VERSION}")
message(STATUS "  Include dirs: ${Python_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${Python_LIBRARIES}")
if(Python_Development.Embed_FOUND)
    message(STATUS "  Embed library: Found")
else()
    message(WARNING "  Embed library: NOT FOUND - embedding may fail")
endif()
message(STATUS "========================================")
message(STATUS "")

# Validate Python version is suitable for bundling
if(Python_VERSION_MAJOR LESS 3 OR (Python_VERSION_MAJOR EQUAL 3 AND Python_VERSION_MINOR LESS 8))
    message(FATAL_ERROR
        "Python ${Python_VERSION} is too old. Minimum required: 3.8\n"
        "pybind11 and the bundled Python require Python 3.8+"
    )
endif()

# Warn if Python version differs significantly from expected bundle version
# This can cause ABI incompatibility with pybind11 modules
set(_expected_bundle_minor "13")  # Update when changing default bundle version
if(NOT Python_VERSION_MINOR EQUAL ${_expected_bundle_minor})
    message(WARNING
        "Build-time Python ${Python_VERSION} differs from expected bundle version 3.${_expected_bundle_minor}.\n"
        "This may cause issues if python-build-standalone doesn't have version ${Python_VERSION}.\n"
        "The bundle will use Python ${Python_VERSION} - ensure it's available in python-build-standalone releases."
    )
endif()

add_subdirectory("${THIRD_PARTY_DIR}/pybind11"
                 "${CMAKE_BINARY_DIR}/pybind11-build")

# ─────────────────────────────── helpers
function(flysight_set_output_to_build_root tgt)
  # Make outputs predictable for multi-config generators (MSVC, Ninja Multi-Config)
  foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_u)
    set_target_properties(${tgt} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${cfg_u} "${CMAKE_BINARY_DIR}"
      LIBRARY_OUTPUT_DIRECTORY_${cfg_u} "${CMAKE_BINARY_DIR}"
      ARCHIVE_OUTPUT_DIRECTORY_${cfg_u} "${CMAKE_BINARY_DIR}"
    )
  endforeach()
endfunction()

function(flysight_msvc_fix_python_debug_autolink tgt)
  if(NOT MSVC)
    return()
  endif()

  # CPython headers auto-link pythonXY_d.lib when _DEBUG is defined.
  # Windows installers usually ship only pythonXY.lib (release import lib).
  set(_py_debug_lib "python${Python_VERSION_MAJOR}${Python_VERSION_MINOR}_d.lib")

  target_link_options(${tgt}
    PRIVATE $<$<CONFIG:Debug>:/NODEFAULTLIB:${_py_debug_lib}>)

  # Ensure the release import lib is present in Debug too
  target_link_libraries(${tgt}
    PRIVATE $<$<CONFIG:Debug>:Python::Python>)
endfunction()

# ─────────────────────────────── third-party targets

# QCustomPlot as a real library target (instead of compiling into the exe)
add_library(qcustomplot STATIC
  "${QCUSTOMPLOT_DIR}/qcustomplot.cpp"
)
target_include_directories(qcustomplot
  PUBLIC  "${QCUSTOMPLOT_ROOT}"
  PRIVATE "${QCUSTOMPLOT_DIR}"
)
target_link_libraries(qcustomplot PUBLIC Qt${QT_VERSION_MAJOR}::Widgets)

# Header-only includes bundled into one interface target
add_library(flysight_headers INTERFACE)
target_include_directories(flysight_headers INTERFACE
  "${EIGEN_DIR}"
)

# Prebuilt GTSAM bundle (interface target that chooses libs by config)
add_library(flysight_gtsam INTERFACE)
target_include_directories(flysight_gtsam INTERFACE "${GTSAM_ROOT}/include")

set(_GTSAM_LIB_DIR "${GTSAM_ROOT}/lib")

# Find GTSAM libraries using CMake's find_library for cross-platform support
# Use NAMES to handle potential library naming variations across platforms
find_library(GTSAM_CEPHES_RELEASE
    NAMES cephes-gtsam
    PATHS "${_GTSAM_LIB_DIR}"
    NO_DEFAULT_PATH REQUIRED)

find_library(GTSAM_MAIN_RELEASE
    NAMES gtsam
    PATHS "${_GTSAM_LIB_DIR}"
    NO_DEFAULT_PATH REQUIRED)

find_library(GTSAM_METIS_RELEASE
    NAMES metis-gtsam
    PATHS "${_GTSAM_LIB_DIR}"
    NO_DEFAULT_PATH REQUIRED)

find_library(GTSAM_GEOGRAPHIC_RELEASE
    NAMES Geographic
    PATHS "${_GTSAM_LIB_DIR}"
    NO_DEFAULT_PATH REQUIRED)

# Diagnostic output for GTSAM library discovery
message(STATUS "GTSAM library discovery:")
message(STATUS "  GTSAM_CEPHES_RELEASE:     ${GTSAM_CEPHES_RELEASE}")
message(STATUS "  GTSAM_MAIN_RELEASE:       ${GTSAM_MAIN_RELEASE}")
message(STATUS "  GTSAM_METIS_RELEASE:      ${GTSAM_METIS_RELEASE}")
message(STATUS "  GTSAM_GEOGRAPHIC_RELEASE: ${GTSAM_GEOGRAPHIC_RELEASE}")

find_library(GTSAM_CEPHES_DEBUG cephes-gtsamDebug PATHS "${_GTSAM_LIB_DIR}" NO_DEFAULT_PATH)
find_library(GTSAM_MAIN_DEBUG gtsamDebug PATHS "${_GTSAM_LIB_DIR}" NO_DEFAULT_PATH)
find_library(GTSAM_METIS_DEBUG metis-gtsamDebug PATHS "${_GTSAM_LIB_DIR}" NO_DEFAULT_PATH)
find_library(GTSAM_GEOGRAPHIC_DEBUG Geographic_d PATHS "${_GTSAM_LIB_DIR}" NO_DEFAULT_PATH)

# Fall back to release libs if debug not found
if(NOT GTSAM_CEPHES_DEBUG)
  set(GTSAM_CEPHES_DEBUG "${GTSAM_CEPHES_RELEASE}")
endif()
if(NOT GTSAM_MAIN_DEBUG)
  set(GTSAM_MAIN_DEBUG "${GTSAM_MAIN_RELEASE}")
endif()
if(NOT GTSAM_METIS_DEBUG)
  set(GTSAM_METIS_DEBUG "${GTSAM_METIS_RELEASE}")
endif()
if(NOT GTSAM_GEOGRAPHIC_DEBUG)
  set(GTSAM_GEOGRAPHIC_DEBUG "${GTSAM_GEOGRAPHIC_RELEASE}")
endif()

# GTSAM library linking order is important on Unix:
# - gtsam (main) references metis-gtsam, cephes-gtsam, Geographic
# - List gtsam first, then dependencies it needs
target_link_libraries(flysight_gtsam INTERFACE
  $<$<CONFIG:Debug>:${GTSAM_MAIN_DEBUG}>
  $<$<CONFIG:Debug>:${GTSAM_METIS_DEBUG}>
  $<$<CONFIG:Debug>:${GTSAM_CEPHES_DEBUG}>
  $<$<CONFIG:Debug>:${GTSAM_GEOGRAPHIC_DEBUG}>

  $<$<NOT:$<CONFIG:Debug>>:${GTSAM_MAIN_RELEASE}>
  $<$<NOT:$<CONFIG:Debug>>:${GTSAM_METIS_RELEASE}>
  $<$<NOT:$<CONFIG:Debug>>:${GTSAM_CEPHES_RELEASE}>
  $<$<NOT:$<CONFIG:Debug>>:${GTSAM_GEOGRAPHIC_RELEASE}>
)

# GTSAM uses Boost headers and may trigger Boost.Serialization autolink.
# Link explicitly so MSVC resolves the library reliably.
target_link_libraries(flysight_gtsam INTERFACE
  Boost::serialization
  Boost::timer
  Boost::chrono
  Boost::system
)

# Prebuilt oneTBB bundle
# First, try to find TBB via its installed CMake config (preferred method)
set(TBB_DIR "${TBB_ROOT}/lib/cmake/TBB" CACHE PATH "TBB CMake config directory")

find_package(TBB CONFIG QUIET HINTS "${TBB_DIR}")

if(TBB_FOUND)
    message(STATUS "Found TBB via CMake config at: ${TBB_DIR}")
    # TBB exports TBB::tbb and TBB::tbbmalloc targets
    add_library(flysight_tbb INTERFACE)
    target_link_libraries(flysight_tbb INTERFACE TBB::tbb TBB::tbbmalloc)

    # Store TBB library directory for RPATH configuration
    if(NOT WIN32)
        get_target_property(_tbb_lib_location TBB::tbb IMPORTED_LOCATION_RELEASE)
        if(NOT _tbb_lib_location)
            get_target_property(_tbb_lib_location TBB::tbb IMPORTED_LOCATION)
        endif()
        if(_tbb_lib_location)
            get_filename_component(_TBB_LIB_DIR "${_tbb_lib_location}" DIRECTORY)
            set(_TBB_LIB_DIR "${_TBB_LIB_DIR}" CACHE INTERNAL "TBB library directory for RPATH")
        endif()
    endif()
else()
    # Fallback to manual find_library for Windows or if config not found
    message(STATUS "TBB CMake config not found, using manual library discovery")

    add_library(flysight_tbb INTERFACE)
    target_include_directories(flysight_tbb INTERFACE "${TBB_ROOT}/include")

    set(_TBB_LIB_DIR "${TBB_ROOT}/lib")

    # Platform-specific library search
    if(WIN32)
        find_library(TBB_MAIN_RELEASE tbb PATHS "${_TBB_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
        find_library(TBB_MALLOC_RELEASE tbbmalloc PATHS "${_TBB_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
        find_library(TBB_MAIN_DEBUG tbb_debug PATHS "${_TBB_LIB_DIR}" NO_DEFAULT_PATH)
        find_library(TBB_MALLOC_DEBUG tbbmalloc_debug PATHS "${_TBB_LIB_DIR}" NO_DEFAULT_PATH)
        find_library(TBB_12_RELEASE tbb12 PATHS "${_TBB_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
        find_library(TBB_12_DEBUG tbb12_debug PATHS "${_TBB_LIB_DIR}" NO_DEFAULT_PATH)

        # Fall back to release libs if debug not found
        if(NOT TBB_MAIN_DEBUG)
            set(TBB_MAIN_DEBUG "${TBB_MAIN_RELEASE}")
        endif()
        if(NOT TBB_12_DEBUG)
            set(TBB_12_DEBUG "${TBB_12_RELEASE}")
        endif()
        if(NOT TBB_MALLOC_DEBUG)
            set(TBB_MALLOC_DEBUG "${TBB_MALLOC_RELEASE}")
        endif()

        target_link_libraries(flysight_tbb INTERFACE
            $<$<CONFIG:Debug>:${TBB_MAIN_DEBUG}>
            $<$<CONFIG:Debug>:${TBB_MALLOC_DEBUG}>
            $<$<CONFIG:Debug>:${TBB_12_DEBUG}>
            $<$<NOT:$<CONFIG:Debug>>:${TBB_MAIN_RELEASE}>
            $<$<NOT:$<CONFIG:Debug>>:${TBB_MALLOC_RELEASE}>
            $<$<NOT:$<CONFIG:Debug>>:${TBB_12_RELEASE}>
        )
    else()
        # macOS and Linux: search for shared libraries with platform naming
        find_library(TBB_MAIN_RELEASE
            NAMES tbb libtbb.so.12 libtbb.12.dylib
            PATHS "${_TBB_LIB_DIR}"
            NO_DEFAULT_PATH REQUIRED)
        find_library(TBB_MALLOC_RELEASE
            NAMES tbbmalloc libtbbmalloc.so.12 libtbbmalloc.12.dylib
            PATHS "${_TBB_LIB_DIR}"
            NO_DEFAULT_PATH REQUIRED)

        target_link_libraries(flysight_tbb INTERFACE
            ${TBB_MAIN_RELEASE}
            ${TBB_MALLOC_RELEASE}
        )
    endif()
endif()

# Set build RPATH to include TBB library directory on Unix
# This allows the executable to find TBB at runtime during development
if(NOT WIN32 AND DEFINED _TBB_LIB_DIR)
    list(APPEND CMAKE_BUILD_RPATH "${_TBB_LIB_DIR}")
    message(STATUS "Added TBB library directory to BUILD_RPATH: ${_TBB_LIB_DIR}")
endif()

# Boost (need Serialization; use imported targets, no link_directories)
# BOOST_ROOT and BOOST_LIBRARYDIR are set by cmake/BoostDiscovery.cmake with
# platform-appropriate defaults. BOOST_LIBRARYDIR is only set on Windows;
# on Unix systems, FindBoost handles multiarch library paths automatically.

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost REQUIRED COMPONENTS serialization timer chrono system)

# Disable Boost auto-linking on MSVC - CMake handles linking via Boost::* targets
# This prevents LNK1104 errors when using dynamic Boost libraries, as auto-linking
# defaults to static library names (libboost_*.lib) instead of import libs (boost_*.lib)
if(MSVC)
  add_compile_definitions(BOOST_ALL_NO_LIB)
endif()

# KDDockWidgets (KDDW)
find_package(KDDockWidgets-qt6 CONFIG REQUIRED)

# Verify KDDockWidgets was found correctly
if(TARGET KDAB::kddockwidgets)
    get_target_property(_kddw_location KDAB::kddockwidgets LOCATION)
    get_target_property(_kddw_include_dirs KDAB::kddockwidgets INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "KDDockWidgets found:")
    message(STATUS "  Library: ${_kddw_location}")
    message(STATUS "  Includes: ${_kddw_include_dirs}")
else()
    message(FATAL_ERROR "KDDockWidgets target KDAB::kddockwidgets not found after find_package succeeded. "
                        "Check KDDockWidgets-qt6Config.cmake for proper target export.")
endif()

# ─────────────────────────────── C++ model library
add_library(flysight_model STATIC
  sessiondata.cpp            sessiondata.h
  dependencymanager.cpp      dependencymanager.h
  calculatedvalue.cpp        calculatedvalue.h
)
target_include_directories(flysight_model PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(flysight_model PUBLIC Qt${QT_VERSION_MAJOR}::Core)

# ─────────────────────────────── application
set(PROJECT_SOURCES
  main.cpp
  mainwindow.cpp             mainwindow.h          mainwindow.ui
  sessionmodel.h             sessionmodel.cpp
  plotwidget.h               plotwidget.cpp
  plotviewsettingsmodel.cpp  plotviewsettingsmodel.h
  dataimporter.cpp           dataimporter.h
  logbookview.h              logbookview.cpp
  plottool/pantool.h
  plottool/plottool.h
  plottool/selecttool.cpp    plottool/selecttool.h
  plottool/zoomtool.h
  plottool/setexittool.h     plottool/setexittool.cpp
  plottool/setgroundtool.h   plottool/setgroundtool.cpp
  preferences/preferencesmanager.h
  preferences/preferencekeys.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  preferences/legendsettingspage.h  preferences/legendsettingspage.cpp
  preferences/mapsettingspage.h     preferences/mapsettingspage.cpp
  preferences/markerssettingspage.h preferences/markerssettingspage.cpp
  preferences/plotssettingspage.h  preferences/plotssettingspage.cpp
  units/unitdefinitions.h
  units/unitconverter.h            units/unitconverter.cpp
  dependencykey.h
  crosshairmanager.h         crosshairmanager.cpp
  graphinfo.h
  pluginhost.h               pluginhost.cpp
  plotregistry.cpp           plotregistry.h
  imugnssekf.h               imugnssekf.cpp
  python_output_redirector.h
  legendwidget.cpp           legendwidget.h
  legendtablemodel.cpp       legendtablemodel.h
  plotmodel.cpp              plotmodel.h
  cursormodel.cpp            cursormodel.h
  legendpresenter.cpp        legendpresenter.h
  mapwidget.cpp              mapwidget.h
  mappreferencesbridge.cpp   mappreferencesbridge.h
  videowidget.cpp            videowidget.h
  trackmapmodel.cpp          trackmapmodel.h
  mapcursordotmodel.cpp      mapcursordotmodel.h
  mapcursorproxy.cpp         mapcursorproxy.h
  plotrangemodel.cpp         plotrangemodel.h
  markerregistry.cpp         markerregistry.h
  markermodel.cpp            markermodel.h
  resources.qrc
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  qt_add_executable(FlySightViewer WIN32 MANUAL_FINALIZATION ${PROJECT_SOURCES}
    legendwidget.h
    legendwidget.cpp
    legendtablemodel.h
    legendtablemodel.cpp)
else()
  add_executable(FlySightViewer WIN32 ${PROJECT_SOURCES})
endif()

target_link_libraries(FlySightViewer PRIVATE
  flysight_model
  flysight_headers
  qcustomplot
  flysight_gtsam
  flysight_tbb

  KDAB::kddockwidgets

  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::PrintSupport
  Qt${QT_VERSION_MAJOR}::QuickWidgets
  Qt${QT_VERSION_MAJOR}::Quick
  Qt${QT_VERSION_MAJOR}::Qml
  Qt${QT_VERSION_MAJOR}::Location
  Qt${QT_VERSION_MAJOR}::Positioning
  Qt${QT_VERSION_MAJOR}::Multimedia
  Qt${QT_VERSION_MAJOR}::MultimediaWidgets

  pybind11::embed
  Boost::boost
)

flysight_set_output_to_build_root(FlySightViewer)
flysight_msvc_fix_python_debug_autolink(FlySightViewer)

# ─────────────────────────────── pybind11 bridge (.pyd)
pybind11_add_module(flysight_cpp_bridge
  cpp_bridge.cpp
  dependencykey_bindings.cpp
  sessiondata_bindings.cpp
)

target_link_libraries(flysight_cpp_bridge PRIVATE
  flysight_model
  flysight_headers
  Qt${QT_VERSION_MAJOR}::Core
)

flysight_set_output_to_build_root(flysight_cpp_bridge)
flysight_msvc_fix_python_debug_autolink(flysight_cpp_bridge)

if(QT_VERSION_MAJOR EQUAL 6)
  qt_finalize_executable(FlySightViewer)
endif()

# ─────────────────────────────── install rules
include(GNUInstallDirs)

# Define cmake module directory for deployment scripts
set(FLYSIGHT_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/../cmake")

# On macOS, MACOSX_BUNDLE must be set BEFORE install(TARGETS) so that CMake
# uses the BUNDLE destination instead of RUNTIME. qt_add_executable with
# MANUAL_FINALIZATION does not set this automatically.
if(APPLE)
  set_target_properties(FlySightViewer PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# On Windows, use a flat directory structure (all files in root)
# On other platforms, use standard GNU directory layout
if(WIN32)
  set(FLYSIGHT_INSTALL_BINDIR ".")
  set(FLYSIGHT_INSTALL_LIBDIR ".")
else()
  set(FLYSIGHT_INSTALL_BINDIR "${CMAKE_INSTALL_BINDIR}")
  set(FLYSIGHT_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
endif()

install(TARGETS FlySightViewer
  BUNDLE  DESTINATION .
  LIBRARY DESTINATION ${FLYSIGHT_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${FLYSIGHT_INSTALL_BINDIR}
)

# Platform-specific installation for pybind11 bridge module
# The module must be in site-packages for Python's import system to find it
if(APPLE)
    # macOS: Install to the app bundle's bundled Python site-packages
    # Path: FlySightViewer.app/Contents/Resources/python/lib/python3.XX/site-packages/
    install(TARGETS flysight_cpp_bridge
        LIBRARY DESTINATION "FlySightViewer.app/Contents/Resources/python/lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages"
        COMPONENT Runtime
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Linux: Installation handled via CreateAppDir.cmake with correct Python version path
    # See Task 8.8: The CreateAppDir.cmake will be updated to use site-packages
else()
    # Windows: Use the bundled Python's site-packages
    install(TARGETS flysight_cpp_bridge
        LIBRARY DESTINATION "python/Lib/site-packages"
        RUNTIME DESTINATION "python/Lib/site-packages"
    )
endif()

# Install Python SDK files to plugin directory (where PluginHost looks for it)
# On macOS, plugins go inside the app bundle so they're included in the DMG.
if(APPLE)
  set(_plugins_dest "FlySightViewer.app/Contents/Resources/python_plugins")
else()
  set(_plugins_dest "python_plugins")
endif()

install(FILES "${CMAKE_CURRENT_LIST_DIR}/../python_plugins/flysight_plugin_sdk.py"
  DESTINATION "${_plugins_dest}"
)

# Install bundled Python plugins
install(FILES
  "${CMAKE_CURRENT_LIST_DIR}/../python_plugins/pitot_tube.py"
  "${CMAKE_CURRENT_LIST_DIR}/../python_plugins/allan_variance.py"
  DESTINATION "${_plugins_dest}"
)

# ─────────────────────────────── Qt deployment (Qt 6.3+)
# On all platforms with Qt 6, use the built-in deployment API to bundle Qt libraries.
# We use the QML-aware variant (qt_generate_deploy_qml_app_script) because the app
# includes QML components (MapDock). This is a strict superset of
# qt_generate_deploy_app_script — it deploys all C++ runtime dependencies AND
# automatically discovers and deploys QML modules (QtLocation, QtPositioning, etc.)
# by scanning QML source files for import statements.
# This is the same approach used by QGroundControl and other major Qt mapping apps.
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  qt_generate_deploy_qml_app_script(
    TARGET FlySightViewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
  )
  install(SCRIPT ${deploy_script})
endif()

# ─────────────────────────────── Qt Plugin and QML Module Deployment
# Include unified Qt path discovery module
include("${FLYSIGHT_CMAKE_DIR}/QtPathDiscovery.cmake")

# Discover Qt resource directories (plugins, QML modules)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  find_qt6_resource_dirs()

  # ─────────────────────────────── Geoservices Plugin Deployment
  # These are not included by qt_generate_deploy_app_script by default
  if(QT6_PLUGINS_DIR AND EXISTS "${QT6_PLUGINS_DIR}/geoservices")
    # Platform-specific destination and file pattern
    if(WIN32)
      set(_geoservices_dest "plugins/geoservices")
      set(_geoservices_pattern "*.dll")
    elseif(APPLE)
      set(_geoservices_dest "FlySightViewer.app/Contents/PlugIns/geoservices")
      set(_geoservices_pattern "*.dylib")
    else()
      # Linux: Deploy to AppDir if available, otherwise standard plugins dir
      if(DEFINED FLYSIGHT_APPDIR_PATH)
        set(_geoservices_dest "${FLYSIGHT_APPDIR_PATH}/usr/plugins/geoservices")
      else()
        set(_geoservices_dest "plugins/geoservices")
      endif()
      set(_geoservices_pattern "*.so")
    endif()

    # Install geoservices plugins
    install(DIRECTORY "${QT6_PLUGINS_DIR}/geoservices"
      DESTINATION "${_geoservices_dest}/.."
      FILES_MATCHING PATTERN "${_geoservices_pattern}"
    )

    # Count plugins for verification at configure time
    file(GLOB _geo_plugins "${QT6_PLUGINS_DIR}/geoservices/${_geoservices_pattern}")
    list(LENGTH _geo_plugins _geo_count)
    message(STATUS "Geoservices: Found ${_geo_count} plugins to deploy")

    # Install-time verification
    install(CODE "
      set(_dest_dir \"\${CMAKE_INSTALL_PREFIX}/${_geoservices_dest}\")
      if(NOT EXISTS \"\${_dest_dir}\")
        message(WARNING \"Geoservices plugins not found in deployment at \${_dest_dir}\")
      else()
        file(GLOB _deployed_plugins \"\${_dest_dir}/*\")
        list(LENGTH _deployed_plugins _count)
        message(STATUS \"Deployed \${_count} geoservices plugins to \${_dest_dir}\")
      endif()
    ")
  else()
    message(WARNING "Qt geoservices plugins not found at ${QT6_PLUGINS_DIR}/geoservices")
    message(STATUS "  Searched from Qt prefix: ${QT6_PREFIX}")
    message(STATUS "  Maps functionality may not work at runtime")
  endif()

  # ─────────────────────────────── Additional Qt Plugin Categories
  # qt_generate_deploy_app_script handles most plugins (platforms, imageformats, etc.)
  # but some may need manual deployment. This section handles any additional categories.
  #
  # Note: Most plugins (platforms, imageformats, iconengines) are already deployed
  # by qt_generate_deploy_app_script. Only add categories here if they are confirmed
  # to be missing from the deployment.
  #
  # To add a plugin category, add it to the list below:
  set(ADDITIONAL_QT_PLUGIN_CATEGORIES
    # imageformats  # Usually handled by Qt deploy
    # platforms     # Usually handled by Qt deploy
    # iconengines   # Usually handled by Qt deploy
    # tls           # May be needed for HTTPS map tiles
  )

  # Deploy additional plugin categories if specified
  foreach(_plugin_category ${ADDITIONAL_QT_PLUGIN_CATEGORIES})
    set(_plugin_src "${QT6_PLUGINS_DIR}/${_plugin_category}")

    if(EXISTS "${_plugin_src}")
      # Platform-specific destination
      if(WIN32)
        set(_plugin_dest "plugins/${_plugin_category}")
        set(_plugin_pattern "*.dll")
      elseif(APPLE)
        set(_plugin_dest "FlySightViewer.app/Contents/PlugIns/${_plugin_category}")
        set(_plugin_pattern "*.dylib")
      else()
        if(DEFINED FLYSIGHT_APPDIR_PATH)
          set(_plugin_dest "${FLYSIGHT_APPDIR_PATH}/usr/plugins/${_plugin_category}")
        else()
          set(_plugin_dest "plugins/${_plugin_category}")
        endif()
        set(_plugin_pattern "*.so")
      endif()

      install(DIRECTORY "${_plugin_src}"
        DESTINATION "${_plugin_dest}/.."
        FILES_MATCHING PATTERN "${_plugin_pattern}"
      )
      message(STATUS "Additional Qt plugin category ${_plugin_category} will be deployed")
    else()
      message(STATUS "Qt plugin category ${_plugin_category} not found (may not be needed)")
    endif()
  endforeach()

  # ─────────────────────────────── QML Module & qt.conf Deployment
  # QtLocation, QtPositioning, and other QML modules are deployed automatically
  # by qt_generate_deploy_qml_app_script() above. It scans QML source files for
  # import statements and deploys the required modules. It also generates qt.conf.
  # No manual QML deployment or qt.conf generation is needed here.
endif()

# ─────────────────────────────── Windows Deployment
if(WIN32)
  # Bundle Python embeddable package
  include("${FLYSIGHT_CMAKE_DIR}/BundlePythonWindows.cmake")

  # Deploy third-party DLLs (TBB, GTSAM, KDDockWidgets, Boost)
  include("${FLYSIGHT_CMAKE_DIR}/DeployThirdPartyWindows.cmake")
endif()

# ─────────────────────────────── macOS App Bundle Deployment
if(APPLE)
  # ==========================================================================
  # macOS RPATH Configuration (Single Source of Truth)
  # ==========================================================================
  # All RPATH settings for macOS are consolidated here. Do NOT set RPATH
  # properties in DeployThirdPartyMacOS.cmake or other modules.
  #
  # Key concepts:
  # - Library ID: The "name" of a library, should be @rpath/libname.dylib
  # - RPATH entry: Search paths for @rpath/ references
  # - @executable_path: Relative to the main executable
  # - @loader_path: Relative to the binary doing the loading
  # ==========================================================================

  # Option to control RPATH behavior for CI vs development builds
  option(FLYSIGHT_USE_INSTALL_RPATH_AT_BUILD "Use install RPATH during build (for CI)" OFF)

  # RPATH configuration for the main executable
  # @executable_path/../Frameworks allows the executable to find dylibs in the Frameworks directory
  set_target_properties(FlySightViewer PROPERTIES
    BUILD_RPATH "@executable_path/../Frameworks"
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH ${FLYSIGHT_USE_INSTALL_RPATH_AT_BUILD}
    MACOSX_RPATH TRUE
  )

  # RPATH configuration for the pybind11 module
  # The module is installed to:
  #   FlySightViewer.app/Contents/Resources/python/lib/python3.XX/site-packages/flysight_cpp_bridge.cpython-3XX-darwin.so
  #
  # Path from module to Frameworks:
  #   site-packages/ -> python3.XX/ -> lib/ -> python/ -> Resources/ -> Contents/ -> Frameworks/
  #   That's 5 levels up: @loader_path/../../../../../Frameworks
  #
  # We also include @executable_path/../Frameworks as a fallback for when
  # the module is loaded from the main executable context.
  set_target_properties(flysight_cpp_bridge PROPERTIES
    BUILD_RPATH "@loader_path/../../../../../Frameworks;@executable_path/../Frameworks"
    INSTALL_RPATH "@loader_path/../../../../../Frameworks;@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH ${FLYSIGHT_USE_INSTALL_RPATH_AT_BUILD}
    MACOSX_RPATH TRUE
    INSTALL_NAME_DIR "@rpath"
  )

  # Set app bundle properties (MACOSX_BUNDLE itself is set earlier,
  # before install rules, so that CMake uses BUNDLE DESTINATION)
  set_target_properties(FlySightViewer PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.flysight.viewer"
    MACOSX_BUNDLE_BUNDLE_NAME "FlySight Viewer"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
  )

  # Include macOS-specific deployment modules
  include("${FLYSIGHT_CMAKE_DIR}/BundlePythonMacOS.cmake" OPTIONAL)
  if(COMMAND bundle_python_macos)
    bundle_python_macos(TARGET FlySightViewer)
  endif()

  # Deploy third-party libraries (TBB, GTSAM, KDDockWidgets)
  include("${FLYSIGHT_CMAKE_DIR}/DeployThirdPartyMacOS.cmake" OPTIONAL)
  if(COMMAND deploy_third_party_macos)
    deploy_third_party_macos(
      TARGET FlySightViewer
      TBB_ROOT "${TBB_ROOT}"
      GTSAM_ROOT "${GTSAM_ROOT}"
      KDDW_ROOT "${KDDW_ROOT}"
    )
  endif()

  # Run rpath fixing script after installation
  install(CODE "
    message(STATUS \"Fixing macOS rpaths...\")
    set(FIX_SCRIPT \"${FLYSIGHT_CMAKE_DIR}/fix_macos_rpaths.sh\")
    if(EXISTS \"\${FIX_SCRIPT}\")
      execute_process(
        COMMAND chmod +x \"\${FIX_SCRIPT}\"
        COMMAND bash \"\${FIX_SCRIPT}\" \"\${CMAKE_INSTALL_PREFIX}/FlySightViewer.app\"
        RESULT_VARIABLE _fix_result
      )
      if(NOT _fix_result EQUAL 0)
        message(WARNING \"rpath fixing script returned: \${_fix_result}\")
      endif()
    else()
      message(STATUS \"Skipping rpath fix (script not found)\")
    endif()
  " COMPONENT Runtime)

  # ─────────────────────────────── Code signing
  # After all path-fixing scripts have run, sign every Mach-O binary in the
  # bundle. This is required on modern macOS — binaries with invalidated or
  # missing signatures cannot be loaded.
  #
  # Uses Mach-O content detection (not file extension) so nothing is missed:
  #   - .so files (Python pybind11 modules in Resources/python/)
  #   - .dylib files (QML plugins in Resources/qml/)
  #   - extensionless framework binaries (QtCore.framework/Versions/A/QtCore)
  #
  # Signing identity is controlled by FLYSIGHT_CODESIGN_IDENTITY:
  #   - Default ("-"): ad-hoc signing for development
  #   - "Developer ID Application: ...": distribution signing with timestamp
  #     and hardened runtime (required for notarization)
  #
  # For distribution builds, set at configure time:
  #   cmake -B build ... -DFLYSIGHT_CODESIGN_IDENTITY="Developer ID Application: Name (XXXXXXXXXX)"

  set(FLYSIGHT_CODESIGN_IDENTITY "-" CACHE STRING
    "macOS code signing identity. Use '-' for ad-hoc (development) or 'Developer ID Application: ...' for distribution."
  )

  # Determine signing flags at configure time
  if(FLYSIGHT_CODESIGN_IDENTITY STREQUAL "-")
    set(_codesign_flags "--force --sign -")
    set(_codesign_mode "ad-hoc (development)")
  else()
    set(_codesign_flags "--force --sign \"${FLYSIGHT_CODESIGN_IDENTITY}\" --timestamp --options runtime")
    set(_codesign_mode "distribution (${FLYSIGHT_CODESIGN_IDENTITY})")
  endif()

  message(STATUS "macOS code signing: ${_codesign_mode}")

  install(CODE "
    set(_identity \"${FLYSIGHT_CODESIGN_IDENTITY}\")
    set(_bundle \"\${CMAKE_INSTALL_PREFIX}/FlySightViewer.app\")

    if(_identity STREQUAL \"-\")
      message(STATUS \"Code signing bundle (ad-hoc for development)...\")
      set(_sign_cmd codesign --force --sign -)
    else()
      message(STATUS \"Code signing bundle (distribution: \${_identity})...\")
      set(_sign_cmd codesign --force --sign \"\${_identity}\" --timestamp --options runtime)
    endif()

    # Find all regular files in the bundle
    execute_process(
      COMMAND find \"\${_bundle}/Contents\" -type f
      OUTPUT_VARIABLE _all_files
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REPLACE \"\\n\" \";\" _all_files \"\${_all_files}\")

    # Step 1: Sign each Mach-O binary (detected by content, not extension)
    set(_signed_count 0)
    foreach(_file \${_all_files})
      execute_process(
        COMMAND file \"\${_file}\"
        OUTPUT_VARIABLE _file_type
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(_file_type MATCHES \"Mach-O\")
        execute_process(
          COMMAND \${_sign_cmd} \"\${_file}\"
          ERROR_QUIET
        )
        math(EXPR _signed_count \"\${_signed_count} + 1\")
      endif()
    endforeach()

    # Step 2: Sign .framework bundles (Apple requires frameworks signed as bundles)
    file(GLOB _frameworks \"\${_bundle}/Contents/Frameworks/*.framework\")
    list(LENGTH _frameworks _fw_count)
    foreach(_fw \${_frameworks})
      execute_process(COMMAND \${_sign_cmd} \"\${_fw}\" ERROR_QUIET)
    endforeach()

    # Step 3: Sign the top-level app bundle last (inside-out order)
    execute_process(COMMAND \${_sign_cmd} \"\${_bundle}\" ERROR_QUIET)

    message(STATUS \"Signed \${_signed_count} Mach-O binaries, \${_fw_count} frameworks, and the app bundle\")
  " COMPONENT Runtime)
endif()

# ─────────────────────────────── Linux AppImage Deployment
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Set RPATH for the executable to find libraries relative to its location
  set_target_properties(FlySightViewer PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH FALSE
  )

  # Set RPATH for the pybind11 module
  set_target_properties(flysight_cpp_bridge PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH FALSE
  )

  # Python bundling for Linux (must be before CreateAppDir to define BUNDLE_PYTHON_VERSION)
  include("${FLYSIGHT_CMAKE_DIR}/BundlePythonLinux.cmake" OPTIONAL)

  # AppDir structure creation (uses BUNDLE_PYTHON_VERSION from BundlePythonLinux)
  include("${FLYSIGHT_CMAKE_DIR}/CreateAppDir.cmake" OPTIONAL)

  # Third-party library deployment
  include("${FLYSIGHT_CMAKE_DIR}/DeployThirdPartyLinux.cmake" OPTIONAL)

  # linuxdeployqt integration (optional enhancement)
  include("${FLYSIGHT_CMAKE_DIR}/RunLinuxDeployQt.cmake" OPTIONAL)

  # AppImage generation
  include("${FLYSIGHT_CMAKE_DIR}/CreateAppImage.cmake" OPTIONAL)
endif()

# ─────────────────────────────── packaging
# Determine architecture for package filename
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(FLYSIGHT_ARCH "x64")
else()
  set(FLYSIGHT_ARCH "x86")
endif()

# Determine platform name for package filename
if(WIN32)
  set(FLYSIGHT_PLATFORM "Windows")
elseif(APPLE)
  set(FLYSIGHT_PLATFORM "macOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(FLYSIGHT_PLATFORM "Linux")
else()
  set(FLYSIGHT_PLATFORM "${CMAKE_SYSTEM_NAME}")
endif()

# Platform-specific generator selection
if(APPLE)
  # Use DMG on macOS for a professional installer experience
  set(CPACK_GENERATOR DragNDrop)
  set(CPACK_DMG_FORMAT UDZO)  # Compressed DMG
  set(CPACK_DMG_VOLUME_NAME "FlySightViewer ${PROJECT_VERSION}")
  # Don't include top-level directory for DMG (app bundle goes directly in DMG)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
else()
  # Use ZIP on Windows and Linux (Linux AppImage handled separately)
  set(CPACK_GENERATOR ZIP)
  set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
endif()

set(CPACK_PACKAGE_NAME "FlySightViewer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "FlySightViewer-${PROJECT_VERSION}-${FLYSIGHT_PLATFORM}-${FLYSIGHT_ARCH}")
set(CPACK_PACKAGE_VENDOR "FlySight")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FlySight GPS Data Viewer")

# Include LICENSE file if it exists
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../LICENSE")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_LIST_DIR}/../LICENSE")
endif()

# macOS-specific app bundle settings
if(APPLE)
  set(CPACK_BUNDLE_NAME "FlySightViewer")
  set(CPACK_BUNDLE_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
  set(CPACK_BUNDLE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/FlySightViewer.icns")
endif()

include(CPack)
