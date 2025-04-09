cmake_minimum_required(VERSION 3.16)

project(FlySightViewer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS PrintSupport)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(FlySightViewer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        sessionmodel.h sessionmodel.cpp
        plotwidget.h plotwidget.cpp
        sessiondata.cpp sessiondata.h
        dataimporter.cpp dataimporter.h
        logbookview.h logbookview.cpp
        calculatedvalue.h calculatedvalue.cpp
        plottool/pantool.h plottool/plottool.h plottool/selecttool.cpp plottool/selecttool.h plottool/zoomtool.h
        preferences/preferencesmanager.h
        preferences/preferencesdialog.h preferences/preferencesdialog.cpp
        preferences/generalsettingspage.h preferences/generalsettingspage.cpp
        preferences/importsettingspage.h preferences/importsettingspage.cpp
        dependencykey.h
        dependencymanager.h
        dependencymanager.cpp
        plottool/setexittool.h
        plottool/setgroundtool.h
        plottool/setexittool.cpp
        plottool/setgroundtool.cpp
        crosshairmanager.h crosshairmanager.cpp
        graphinfo.h
        imugnssekf.h imugnssekf.cpp
        gnssfactor.h
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

target_link_libraries(FlySightViewer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
target_link_libraries(FlySightViewer PRIVATE Qt${QT_VERSION_MAJOR}::PrintSupport)

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

target_include_directories(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/QCustomPlot
    ${CMAKE_SOURCE_DIR}/../third-party/Eigen
    ${CMAKE_SOURCE_DIR}/../third-party/GeographicLib/include
    ${CMAKE_SOURCE_DIR}/../third-party/GTSAM/install/include
    ${CMAKE_SOURCE_DIR}/../third-party/oneTBB/install/include
)

target_sources(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/QCustomPlot/QCustomPlot/qcustomplot.cpp
)

target_link_libraries(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/GeographicLib/lib/GeographicLib-i.lib
)

target_link_libraries(FlySightViewer PRIVATE
    $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/../third-party/GTSAM/install/lib/gtsamDebug.lib>
    $<$<CONFIG:Release>:${CMAKE_SOURCE_DIR}/../third-party/GTSAM/install/lib/gtsam.lib>
)

target_link_directories(FlySightViewer PRIVATE
    ${CMAKE_SOURCE_DIR}/../third-party/oneTBB/install/lib
)

find_package(Boost REQUIRED)

if(Boost_FOUND)
    message(STATUS "Boost found: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost library directory: ${Boost_LIBRARY_DIRS}")

    target_include_directories(FlySightViewer PRIVATE ${Boost_INCLUDE_DIRS})
    target_link_directories(FlySightViewer PRIVATE ${Boost_LIBRARY_DIRS})
else()
    message(FATAL_ERROR "Boost not found! Ensure BOOST_ROOT is set correctly.")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(FlySightViewer)
endif()
