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
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets PrintSupport)

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
  dataimporter.cpp           dataimporter.h
  logbookview.h              logbookview.cpp
  plottool/pantool.h
  plottool/plottool.h
  plottool/selecttool.cpp    plottool/selecttool.h
  plottool/zoomtool.h
  plottool/setexittool.h     plottool/setexittool.cpp
  plottool/setgroundtool.h   plottool/setgroundtool.cpp
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  dependencykey.h
  crosshairmanager.h         crosshairmanager.cpp
  graphinfo.h
  pluginhost.h               pluginhost.cpp
  plotregistry.cpp           plotregistry.h
  imugnssekf.h               imugnssekf.cpp
  python_output_redirector.h
  legendmanager.cpp          legendmanager.h
  legendwidget.cpp           legendwidget.h
  legendtablemodel.cpp       legendtablemodel.h
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

install(TARGETS FlySightViewer
  BUNDLE  DESTINATION .
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS flysight_cpp_bridge
  LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../python_src/"
  DESTINATION python/Lib/site-packages
  PATTERN "__pycache__" EXCLUDE
)

# ─────────────────────────────── packaging
set(CPACK_GENERATOR          ZIP)
set(CPACK_PACKAGE_FILE_NAME  "FlySightViewer-${PROJECT_VERSION}")
include(CPack)
