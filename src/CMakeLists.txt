cmake_minimum_required(VERSION 3.18)

project(FlySightViewer VERSION 0.1 LANGUAGES CXX)

# ─────────────────────────────── global settings
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Install into <build>/install by default so devs don’t need admin rights
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE PATH "Install path prefix." FORCE)
endif()

# Third-party root (editable in Qt Creator / CMake cache)
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../third-party"
    CACHE PATH "Path to third-party dependencies")

set(EIGEN_DIR      "${THIRD_PARTY_DIR}/Eigen"                 CACHE PATH "Eigen include dir")
set(QCUSTOMPLOT_ROOT "${THIRD_PARTY_DIR}/QCustomPlot"         CACHE PATH "QCustomPlot root")
set(QCUSTOMPLOT_DIR  "${QCUSTOMPLOT_ROOT}/QCustomPlot"        CACHE PATH "QCustomPlot sources dir")
set(GTSAM_ROOT     "${THIRD_PARTY_DIR}/GTSAM-install"         CACHE PATH "GTSAM install prefix")
set(TBB_ROOT       "${THIRD_PARTY_DIR}/oneTBB-install"        CACHE PATH "oneTBB install prefix")
set(KDDW_ROOT      "${THIRD_PARTY_DIR}/KDDockWidgets-install" CACHE PATH "KDDockWidgets install prefix")

# Make KDDW discoverable by find_package() without requiring external -DCMAKE_PREFIX_PATH=...
list(PREPEND CMAKE_PREFIX_PATH "${KDDW_ROOT}")

# Boost root (editable)
set(BOOST_ROOT "C:/Program Files/Boost/boost_1_87_0"
    CACHE PATH "Boost root directory")

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
find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development Development.Embed)

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
target_link_libraries(flysight_gtsam INTERFACE
  $<$<CONFIG:Debug>:${_GTSAM_LIB_DIR}/cephes-gtsamDebug.lib>
  $<$<CONFIG:Debug>:${_GTSAM_LIB_DIR}/gtsamDebug.lib>
  $<$<CONFIG:Debug>:${_GTSAM_LIB_DIR}/metis-gtsamDebug.lib>
  $<$<CONFIG:Debug>:${_GTSAM_LIB_DIR}/Geographic_d.lib>

  $<$<NOT:$<CONFIG:Debug>>:${_GTSAM_LIB_DIR}/cephes-gtsam.lib>
  $<$<NOT:$<CONFIG:Debug>>:${_GTSAM_LIB_DIR}/gtsam.lib>
  $<$<NOT:$<CONFIG:Debug>>:${_GTSAM_LIB_DIR}/metis-gtsam.lib>
  $<$<NOT:$<CONFIG:Debug>>:${_GTSAM_LIB_DIR}/Geographic.lib>
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
add_library(flysight_tbb INTERFACE)
target_include_directories(flysight_tbb INTERFACE "${TBB_ROOT}/include")

set(_TBB_LIB_DIR "${TBB_ROOT}/lib")
target_link_libraries(flysight_tbb INTERFACE
  $<$<CONFIG:Debug>:${_TBB_LIB_DIR}/tbb_debug.lib>
  $<$<CONFIG:Debug>:${_TBB_LIB_DIR}/tbb12_debug.lib>
  $<$<CONFIG:Debug>:${_TBB_LIB_DIR}/tbbmalloc_debug.lib>

  $<$<NOT:$<CONFIG:Debug>>:${_TBB_LIB_DIR}/tbb.lib>
  $<$<NOT:$<CONFIG:Debug>>:${_TBB_LIB_DIR}/tbb12.lib>
  $<$<NOT:$<CONFIG:Debug>>:${_TBB_LIB_DIR}/tbbmalloc.lib>
)

# Boost (need Serialization; use imported targets, no link_directories)
set(BOOST_ROOT "C:/Program Files/Boost/boost_1_87_0"
    CACHE PATH "Boost root directory")

# Where the compiled .lib files are (common default for b2 builds).
# If your libs are elsewhere, change this to that folder.
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib"
    CACHE PATH "Boost library directory")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost REQUIRED COMPONENTS serialization timer chrono system)

# KDDockWidgets (KDDW)
find_package(KDDockWidgets-qt6 CONFIG REQUIRED)

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
  qt_add_executable(FlySightViewer MANUAL_FINALIZATION ${PROJECT_SOURCES}
    legendwidget.h
    legendwidget.cpp
    legendtablemodel.h
    legendtablemodel.cpp)
else()
  add_executable(FlySightViewer ${PROJECT_SOURCES})
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

install(TARGETS flysight_cpp_bridge
  LIBRARY DESTINATION "python/Lib/site-packages"
  RUNTIME DESTINATION "python/Lib/site-packages"
)

# Install Python SDK files to plugin directory (where PluginHost looks for it)
install(FILES "${CMAKE_CURRENT_LIST_DIR}/../python_plugins/flysight_plugin_sdk.py"
  DESTINATION "python_plugins"
)

# Install bundled Python plugins
install(FILES
  "${CMAKE_CURRENT_LIST_DIR}/../python_plugins/pitot_tube.py"
  "${CMAKE_CURRENT_LIST_DIR}/../python_plugins/allan_variance.py"
  DESTINATION "python_plugins"
)

# ─────────────────────────────── Qt deployment (Qt 6.3+)
# On all platforms with Qt 6, use the built-in deployment API to bundle Qt libraries
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  qt_generate_deploy_app_script(
    TARGET FlySightViewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT ${deploy_script})
endif()

# ─────────────────────────────── Qt Location geoservices plugins
# These are not included by qt_generate_deploy_app_script by default
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  # Get Qt's plugin directory from the Qt6::Core target
  get_target_property(_qt6_core_location Qt6::Core LOCATION)
  get_filename_component(_qt6_lib_dir "${_qt6_core_location}" DIRECTORY)
  if(WIN32)
    # On Windows, Qt6Core.dll is in bin/, plugins are in ../plugins/
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_plugins_dir "${_qt6_prefix}/plugins")
    set(_geoservices_pattern "*.dll")
    set(_geoservices_dest "plugins")
  elseif(APPLE)
    # On macOS, Qt6Core is in lib/, plugins are in ../plugins/ or share/qt6/plugins
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_plugins_dir "${_qt6_prefix}/share/qt6/plugins")
    if(NOT EXISTS "${_qt6_plugins_dir}")
      set(_qt6_plugins_dir "${_qt6_prefix}/plugins")
    endif()
    set(_geoservices_pattern "*.dylib")
    set(_geoservices_dest "FlySightViewer.app/Contents/PlugIns")
  else()
    # On Linux, plugins are typically in lib/qt6/plugins or plugins/
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_plugins_dir "${_qt6_prefix}/plugins")
    if(NOT EXISTS "${_qt6_plugins_dir}")
      set(_qt6_plugins_dir "${_qt6_prefix}/lib/qt6/plugins")
    endif()
    set(_geoservices_pattern "*.so")
    set(_geoservices_dest "plugins")
  endif()

  if(EXISTS "${_qt6_plugins_dir}/geoservices")
    install(DIRECTORY "${_qt6_plugins_dir}/geoservices"
      DESTINATION "${_geoservices_dest}"
      FILES_MATCHING PATTERN "${_geoservices_pattern}"
    )
  else()
    message(WARNING "Qt geoservices plugins not found at ${_qt6_plugins_dir}/geoservices")
  endif()
endif()

# ─────────────────────────────── Qt QML modules deployment
# QtLocation and QtPositioning QML modules are required for MapDock.qml
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
  # Get Qt's QML directory from the Qt6::Core target location
  get_target_property(_qt6_core_location Qt6::Core LOCATION)
  get_filename_component(_qt6_lib_dir "${_qt6_core_location}" DIRECTORY)

  if(WIN32)
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_qml_dir "${_qt6_prefix}/qml")
    set(_qml_dest "qml")
  elseif(APPLE)
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_qml_dir "${_qt6_prefix}/qml")
    if(NOT EXISTS "${_qt6_qml_dir}")
      set(_qt6_qml_dir "${_qt6_prefix}/share/qt6/qml")
    endif()
    set(_qml_dest "FlySightViewer.app/Contents/Resources/qml")
  else()
    get_filename_component(_qt6_prefix "${_qt6_lib_dir}" DIRECTORY)
    set(_qt6_qml_dir "${_qt6_prefix}/qml")
    if(NOT EXISTS "${_qt6_qml_dir}")
      set(_qt6_qml_dir "${_qt6_prefix}/lib/qt6/qml")
    endif()
    set(_qml_dest "qml")
  endif()

  # Deploy required QML modules for map functionality
  foreach(_qml_module QtLocation QtPositioning)
    if(EXISTS "${_qt6_qml_dir}/${_qml_module}")
      install(DIRECTORY "${_qt6_qml_dir}/${_qml_module}"
        DESTINATION "${_qml_dest}"
      )
    else()
      message(WARNING "Qt QML module ${_qml_module} not found at ${_qt6_qml_dir}/${_qml_module}")
    endif()
  endforeach()
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
  # Set RPATH for the executable to find libraries in Frameworks
  set_target_properties(FlySightViewer PROPERTIES
    BUILD_RPATH "@executable_path/../Frameworks"
    INSTALL_RPATH "@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
    MACOSX_RPATH TRUE
  )

  # Set RPATH for the pybind11 module
  set_target_properties(flysight_cpp_bridge PROPERTIES
    BUILD_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks"
    INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks"
    BUILD_WITH_INSTALL_RPATH TRUE
    MACOSX_RPATH TRUE
  )

  # Set app bundle properties
  set_target_properties(FlySightViewer PROPERTIES
    MACOSX_BUNDLE TRUE
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

  # AppDir structure creation
  include("${FLYSIGHT_CMAKE_DIR}/CreateAppDir.cmake" OPTIONAL)

  # Python bundling for Linux
  include("${FLYSIGHT_CMAKE_DIR}/BundlePythonLinux.cmake" OPTIONAL)

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
