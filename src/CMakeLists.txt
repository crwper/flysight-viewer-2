cmake_minimum_required(VERSION 3.16)

project(FlySightViewer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS PrintSupport)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/pybind11
    ${CMAKE_BINARY_DIR}/pybind11-build
)

add_library(flysight_model STATIC
  sessiondata.cpp sessiondata.h
  dependencymanager.cpp dependencymanager.h
  calculatedvalue.cpp calculatedvalue.h
)
target_include_directories(flysight_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flysight_model PUBLIC Qt${QT_VERSION_MAJOR}::Core)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(FlySightViewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        sessionmodel.h sessionmodel.cpp
        plotwidget.h plotwidget.cpp
        qcustomplot/qcustomplot.cpp qcustomplot/qcustomplot.h
        dataimporter.cpp dataimporter.h
        logbookview.h logbookview.cpp
        plottool/pantool.h plottool/plottool.h plottool/selecttool.cpp plottool/selecttool.h plottool/zoomtool.h
        preferences/preferencesmanager.h
        preferences/preferencesdialog.h preferences/preferencesdialog.cpp
        preferences/generalsettingspage.h preferences/generalsettingspage.cpp
        preferences/importsettingspage.h preferences/importsettingspage.cpp
        dependencykey.h
        plottool/setexittool.h
        plottool/setgroundtool.h
        plottool/setexittool.cpp
        plottool/setgroundtool.cpp
        crosshairmanager.h crosshairmanager.cpp
        graphinfo.h
        pluginhost.h
        pluginhost.cpp
        plotregistry.cpp
        plotregistry.h
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET FlySightViewer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(FlySightViewer SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(FlySightViewer
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(FlySightViewer
    PRIVATE flysight_model
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    PRIVATE Qt${QT_VERSION_MAJOR}::PrintSupport
    PRIVATE pybind11::embed
)

# Define the separate Python extension module target
pybind11_add_module(flysight_cpp_bridge
  cpp_bridge.cpp
  dependencykey_bindings.cpp
  sessiondata_bindings.cpp
)

# Link the bridge module against Qt since it uses Qt headers indirectly
target_link_libraries(flysight_cpp_bridge
    PRIVATE flysight_model                 # ← resolve all SessionData symbols
    PRIVATE Qt${QT_VERSION_MAJOR}::Core    # for QVariant, QVector, QMap, etc.
)

# Place the output module (.pyd on Windows) next to the executable
# CMAKE_RUNTIME_OUTPUT_DIRECTORY is usually where the .exe goes in multi-config generators
# For single-config generators (like Makefiles, Ninja), CMAKE_BINARY_DIR might be sufficient
# Using CMAKE_RUNTIME_OUTPUT_DIRECTORY is generally safer if defined.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
set_target_properties(flysight_cpp_bridge PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.FlySightViewer)
endif()
set_target_properties(FlySightViewer PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS FlySightViewer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Optional: Also install the python module if you install the application
install(TARGETS flysight_cpp_bridge
    LIBRARY DESTINATION ${CMAKE_INSTALL_BINDIR} # Install .pyd next to .exe
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(FlySightViewer)
endif()
