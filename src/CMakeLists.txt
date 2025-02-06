cmake_minimum_required(VERSION 3.18)      # need ≥3.18 for file(ARCHIVE_EXTRACT)

project(FlySightViewer VERSION 0.1 LANGUAGES CXX)

# ───────────────────────────────  global settings
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

# Let CMake find extra helper modules (e.g. DownloadEmbeddablePython.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# ───────────────────────────────  Qt & third-party
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR}        REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}        REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR}        REQUIRED COMPONENTS PrintSupport)

# pybind11 (submodule)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../third-party/pybind11"
                 "${CMAKE_BINARY_DIR}/pybind11-build")

# ───────────────────────────────  C++ model library
add_library(flysight_model STATIC
    sessiondata.cpp            sessiondata.h
    dependencymanager.cpp      dependencymanager.h
    calculatedvalue.cpp        calculatedvalue.h
)
target_include_directories(flysight_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries  (flysight_model PUBLIC Qt${QT_VERSION_MAJOR}::Core)

# ───────────────────────────────  application sources
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
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(FlySightViewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(FlySightViewer SHARED ${PROJECT_SOURCES})
    else()
        add_executable(FlySightViewer ${PROJECT_SOURCES})
    endif()
endif()

target_link_libraries(FlySightViewer
    PRIVATE flysight_model
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    PRIVATE Qt${QT_VERSION_MAJOR}::PrintSupport
    PRIVATE pybind11::embed
)

target_include_directories(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/QCustomPlot
    ${CMAKE_SOURCE_DIR}/../third-party/Eigen
    ${CMAKE_SOURCE_DIR}/../third-party/GeographicLib/include
)

target_sources(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/QCustomPlot/QCustomPlot/qcustomplot.cpp
)

target_link_libraries(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/GeographicLib/lib/GeographicLib-i.lib
)

# ───────────────────────────────  pybind11 bridge (.pyd)
pybind11_add_module(flysight_cpp_bridge
    cpp_bridge.cpp
    dependencykey_bindings.cpp
    sessiondata_bindings.cpp
)
target_link_libraries(flysight_cpp_bridge
    PRIVATE flysight_model
    PRIVATE Qt${QT_VERSION_MAJOR}::Core
)

# Ensure the .pyd drops next to the .exe in multi-config generators
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

# ───────────────────────────────  install rules
include(GNUInstallDirs)

install(TARGETS FlySightViewer
        BUNDLE  DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(TARGETS flysight_cpp_bridge
        LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Pure-Python sources tracked in git
install(DIRECTORY "${CMAKE_SOURCE_DIR}/../python_src/"
        DESTINATION python/Lib/site-packages
        PATTERN "__pycache__" EXCLUDE)

# Optional embedded CPython bundle
option(FLYSIGHT_BUNDLE_PYTHON "Bundle embeddable Python runtime" ON)
if(FLYSIGHT_BUNDLE_PYTHON)
    include(DownloadEmbeddablePython)      # provided in cmake/ below
endif()

# Deploy Qt runtime into the install tree (Qt ≥ 6)
if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    include("${QT_DEPLOY_SUPPORT}")
    qt_generate_deploy_app_script(
        TARGET        FlySightViewer
        OUTPUT_SCRIPT FlySightDeployScript
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT "${FlySightDeployScript}")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(FlySightViewer)
endif()

# ───────────────────────────────  packaging
set(CPACK_GENERATOR          ZIP)
set(CPACK_PACKAGE_FILE_NAME  "FlySightViewer-${PROJECT_VERSION}")
include(CPack)
