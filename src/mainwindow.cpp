#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QTreeView>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QCloseEvent>
#include <kddockwidgets/LayoutSaver.h>
#include <vector>

// --- FIX FOR GTSAM LINKING ERROR ---
// GTSAM exports std::vector<size_t> (aka unsigned __int64) in its DLL.
// Because mainwindow.cpp uses this type but doesn't include GTSAM headers,
// MSVC generates a local copy, causing a collision (LNK2005).
// This line forces the compiler to use the DLL version instead.
#ifdef _MSC_VER
template class __declspec(dllimport) std::vector<size_t>;
#endif
// -----------------------------------

#include "dataimporter.h"
#include "dependencykey.h"
#include "plotwidget.h"
#include "legendwidget.h"
#include "legendpresenter.h"
#include "mapwidget.h"
#include "videowidget.h"
#include "pluginhost.h"
#include "preferences/preferencesdialog.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
#include "sessiondata.h"
#include "imugnssekf.h"
#include "plotviewsettingsmodel.h"
#include "plotmodel.h"
#include "markermodel.h"
#include "markerregistry.h"
#include "cursormodel.h"
#include "plotrangemodel.h"
#include "units/unitconverter.h"

#include <GeographicLib/LocalCartesian.hpp>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>

namespace FlySight {

MainWindow::MainWindow(QWidget *parent)
    : KDDockWidgets::QtWidgets::MainWindow(
          QStringLiteral("MainWindow"),
          KDDockWidgets::MainWindowOptions{
              KDDockWidgets::MainWindowOption_ManualInit
          },
          parent)
    , m_settings(new QSettings("FlySight", "Viewer", this))
    , m_plotViewSettingsModel(new PlotViewSettingsModel(m_settings, this))
    , m_cursorModel(new CursorModel(this))
    , ui(new Ui::MainWindow)
    , model(new SessionModel(this))
    , plotModel (new PlotModel(this))
    , markerModel (new MarkerModel(this))
{
    ui->setupUi(this);
    manualInit();

    // Register built-in plots and markers BEFORE initializing preferences
    // so that we can dynamically register per-plot and per-marker preferences
    registerBuiltInPlots();
    registerBuiltInMarkers();

    // Initialize plugins (may register additional plots/markers)
    QString defaultDir = QCoreApplication::applicationDirPath() + "/plugins";
    QString pluginDir = qEnvironmentVariable("FLYSIGHT_PLUGINS", defaultDir);
    PluginHost::instance().initialise(pluginDir);

    // Initialize preferences (must come AFTER plots and markers are registered)
    initializePreferences();

    // Initialize calculated values
    initializeCalculatedAttributes();
    initializeCalculatedMeasurements();

    // Populate marker model before PlotWidget construction so markers can render immediately
    if (markerModel) {
        markerModel->setMarkers(MarkerRegistry::instance().allMarkers());
    }

    // Add logbook view
    logbookDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Logbook"));
    logbookView = new LogbookView(model, this);
    logbookDock->setWidget(logbookView);
    addDockWidget(logbookDock, KDDockWidgets::Location_OnRight);

    connect(logbookView, &LogbookView::showSelectedRequested, this, &MainWindow::on_action_ShowSelected_triggered);
    connect(logbookView, &LogbookView::hideSelectedRequested, this, &MainWindow::on_action_HideSelected_triggered);
    connect(logbookView, &LogbookView::hideOthersRequested, this, &MainWindow::on_action_HideOthers_triggered);
    connect(logbookView, &LogbookView::deleteRequested, this, &MainWindow::on_action_Delete_triggered);

    // Add legend view
    legendDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Legend"));
    legendWidget = new LegendWidget(legendDock);
    legendDock->setWidget(legendWidget);
    addDockWidget(legendDock, KDDockWidgets::Location_OnRight);

    // Initialize cursor entries
    if (m_cursorModel) {
        // Mouse
        CursorModel::Cursor mouse;
        mouse.id = QStringLiteral("mouse");
        mouse.label = tr("Mouse");
        mouse.type = CursorModel::CursorType::MouseHover;
        mouse.active = false;
        mouse.positionSpace = CursorModel::PositionSpace::PlotAxisCoord;
        mouse.positionValue = 0.0;
        mouse.axisKey = m_plotViewSettingsModel
            ? m_plotViewSettingsModel->xAxisKey()
            : SessionKeys::TimeFromExit;
        mouse.targetPolicy = CursorModel::TargetPolicy::Explicit;

        m_cursorModel->ensureCursor(mouse);

        // Video
        CursorModel::Cursor video;
        video.id = QStringLiteral("video");
        video.label = tr("Video");
        video.type = CursorModel::CursorType::VideoPlayback;
        video.active = false;
        video.positionSpace = CursorModel::PositionSpace::UtcSeconds;
        video.positionValue = 0.0;
        video.axisKey.clear(); // unused when positionSpace == UtcSeconds
        video.targetPolicy = CursorModel::TargetPolicy::AutoVisibleOverlap;

        m_cursorModel->ensureCursor(video);
    }

    // Create range model for synchronizing plot x-axis range with other docks
    m_rangeModel = new PlotRangeModel(this);

    // Add plot view
    plotsDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Plots"));
    plotWidget = new PlotWidget(model, plotModel, markerModel, m_plotViewSettingsModel, m_cursorModel, m_rangeModel, this);
    plotsDock->setWidget(plotWidget);
    addDockWidget(plotsDock, KDDockWidgets::Location_OnLeft);

    // Add map view (Qt Location)
    mapDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Map"));
    mapWidget = new MapWidget(model, m_cursorModel, m_rangeModel, mapDock);
    mapDock->setWidget(mapWidget);
    addDockWidget(mapDock, KDDockWidgets::Location_OnBottom);

    // LegendPresenter drives legend updates based on models + CursorModel
    m_legendPresenter = new LegendPresenter(model,
                                            plotModel,
                                            m_cursorModel,
                                            m_plotViewSettingsModel,
                                            legendWidget,
                                            this);

    // Connect the newTimeRange signal to PlotWidget's setXAxisRange slot
    connect(this, &MainWindow::newTimeRange, plotWidget, &PlotWidget::setXAxisRange);

    // Marker toggles must update without rebuilding graphs
    if (markerModel && plotWidget) {
        connect(markerModel, &QAbstractItemModel::modelReset,
                plotWidget,
                [this](auto...) {
                    if (plotWidget)
                        plotWidget->updateMarkersOnly();
                });

        connect(markerModel, &QAbstractItemModel::dataChanged,
                plotWidget,
                [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                    if (plotWidget)
                        plotWidget->updateMarkersOnly();
                });
    }

    // Setup plots and marker selection docks
    setupPlotSelectionDock();
    setupMarkerSelectionDock();

    // Add video view (dock exists even before a video is imported)
    videoDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Video"));
    videoWidget = new VideoWidget(model, m_cursorModel, videoDock);
    videoDock->setWidget(videoWidget);
    addDockWidget(videoDock, KDDockWidgets::Location_OnBottom);

    // Restore the previous dock layout (includes visibility/open/closed state).
    restoreDockLayout();

    // Initialize the Window menu (dock visibility)
    initializeWindowMenu();

    // Initialize the Plots menu
    initializeXAxisMenu();
    initializePlotsMenu();

    // Setup plot tools
    setupPlotTools();

    // Plot selection
    connect(plotWidget, &PlotWidget::sessionsSelected, logbookView, &LogbookView::selectSessions);

    // Handle plot tool changes
    connect(plotWidget, &PlotWidget::toolChanged, this, &MainWindow::onPlotWidgetToolChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveDockLayout();
    KDDockWidgets::QtWidgets::MainWindow::closeEvent(event);
}

void MainWindow::restoreDockLayout()
{
    if (!m_settings)
        return;

    const QByteArray layout = m_settings->value(QStringLiteral("ui/dockLayout")).toByteArray();
    if (layout.isEmpty())
        return;

    KDDockWidgets::LayoutSaver saver;
    if (!saver.restoreLayout(layout)) {
        qWarning() << "MainWindow::restoreDockLayout: Failed to restore dock layout, using defaults.";
        m_settings->remove(QStringLiteral("ui/dockLayout"));
    }
}

void MainWindow::saveDockLayout()
{
    if (!m_settings)
        return;

    KDDockWidgets::LayoutSaver saver;
    const QByteArray layout = saver.serializeLayout();
    if (layout.isEmpty())
        return;

    m_settings->setValue(QStringLiteral("ui/dockLayout"), layout);
}

void MainWindow::on_action_Import_triggered()
{
    // Get files to import
    QStringList fileNames = QFileDialog::getOpenFileNames(
        this,
        tr("Import Tracks"),
        m_settings->value("folder").toString(),
        tr("CSV Files (*.csv *.CSV)")
        );

    if (fileNames.isEmpty()) {
        return;
    }

    // Determine the base directory (the initial directory in the dialog)
    QString baseDir = m_settings->value("folder").toString();

    // Call the helper function
    importFiles(fileNames, fileNames.size() > 5, baseDir);

    // Update last used folder
    QString lastUsedFolder = QFileInfo(fileNames.last()).absolutePath();
    m_settings->setValue("folder", lastUsedFolder);
}

void MainWindow::on_action_ImportFolder_triggered()
{
    // Open the file dialog
    QFileDialog dialog(this, tr("Select Folder to Import"));
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setDirectory(m_settings->value("folder").toString());

    // Show dialog and get the selected directory
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // The path returned
    QString selectedDir;
    const QList<QString> selectedFiles = dialog.selectedFiles();
    if (!selectedFiles.isEmpty()) {
        selectedDir = selectedFiles.first();
    }

    // Directory the user navigated to
    QString enteredDir = dialog.directory().absolutePath();

    // Store the appropriate directory based on user interaction
    if (selectedDir != enteredDir) {
        // User selected a specific folder
        m_settings->setValue("folder", QFileInfo(selectedDir).absolutePath());
    } else {
        // User just confirmed the navigated directory
        m_settings->setValue("folder", enteredDir);
    }

    // Define file filters (adjust according to your file types)
    QStringList nameFilters;
    nameFilters << "*.csv" << "*.CSV";

    // Use QDirIterator to iterate through the folder and its subdirectories
    QDirIterator it(selectedDir, nameFilters, QDir::Files, QDirIterator::Subdirectories);

    // Collect all files to import
    QStringList filesToImport;
    while (it.hasNext()) {
        it.next();
        filesToImport << it.filePath();
    }

    // If no files found, inform the user and exit
    if (filesToImport.isEmpty()) {
        QMessageBox::information(this, tr("No Files Found"), tr("No files matching the filter were found in the selected folder."));
        return;
    }

    // Call the helper function
    importFiles(filesToImport, filesToImport.size() > 5, selectedDir);
}

void MainWindow::on_action_ImportVideo_triggered()
{
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    const QString startDir = m_settings->value("videoFolder", defaultDir).toString();

    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Import Video"),
        startDir,
        tr("Video Files (*.mp4 *.mov *.m4v *.avi *.mkv *.webm *.wmv *.mpg *.mpeg);;All Files (*)")
        );

    if (fileName.isEmpty()) {
        return;
    }

    // Update last used folder
    m_settings->setValue("videoFolder", QFileInfo(fileName).absolutePath());

    // Ensure the Video dock is visible when a video is imported.
    if (videoDock && !videoDock->isVisible()) {
        videoDock->show();
    }

    // Load/replace the video in the widget
    if (videoWidget) {
        videoWidget->loadVideo(fileName);
    }
}

void MainWindow::importFiles(
    const QStringList &fileNames,
    bool showProgress,
    const QString &baseDir
    )
{
    if (fileNames.isEmpty()) {
        return;
    }

    // honour whatever the user picked in the Horizontal-Axis menu
    const QString xAxisKey = currentXAxisKey();

    // Initialize a map to collect failed imports with error messages
    QMap<QString, QString> failedImports;

    // Collect successfully imported sessions for batch merge
    QList<SessionData> importedSessions;

    // Variables to track the overall min and max time from newly imported files
    double newMinTime = std::numeric_limits<double>::max();
    double newMaxTime = std::numeric_limits<double>::lowest();

    if (showProgress) {
        // Show progress dialog
        QProgressDialog progressDialog(tr("Importing files..."), tr("Cancel"), 0, fileNames.size(), this);
        progressDialog.setWindowModality(Qt::WindowModal);
        progressDialog.setMinimumDuration(0);

        int current = 0;
        for (const QString &filePath : fileNames) {
            // Update progress dialog
            progressDialog.setValue(current);
            if (progressDialog.wasCanceled()) {
                break; // Exit the loop if the user cancels
            }

            // Import the file
            DataImporter importer;
            SessionData tempSessionData;

            if (importer.importFile(filePath, tempSessionData)) {
                // Force groundElev calculation on the temp session
                tempSessionData.getAttribute(SessionKeys::GroundElev);

                // Collect for batch merge
                importedSessions.append(tempSessionData);

                // Collect min and max time from tempSessionData
                for (const QString &sensorKey : tempSessionData.sensorKeys()) {
                    QVector<double> times = tempSessionData.getMeasurement(sensorKey, xAxisKey);
                    if (!times.isEmpty()) {
                        double minTime = *std::min_element(times.begin(), times.end());
                        double maxTime = *std::max_element(times.begin(), times.end());
                        newMinTime = std::min(newMinTime, minTime);
                        newMaxTime = std::max(newMaxTime, maxTime);
                    }
                }
            } else {
                // Failed import with an error message
                QString errorMessage = importer.getLastError();
                qWarning() << "Failed to import file:" << filePath << "Error:" << errorMessage;

                // Compute relative path if baseDir is provided
                QString displayPath;
                if (!baseDir.isEmpty()) {
                    QDir dir(baseDir);
                    displayPath = dir.relativeFilePath(filePath);
                    // If the file is outside baseDir, fallback to absolute path or just the file name
                    if (displayPath == filePath) {
                        displayPath = QFileInfo(filePath).fileName(); // Alternatively, keep absolute path
                    }
                } else {
                    displayPath = filePath; // No baseDir provided, use absolute path
                }

                failedImports.insert(displayPath, errorMessage);
            }

            ++current;
        }

        // Finish progress dialog
        progressDialog.setValue(fileNames.size());
    } else {
        // No progress dialog
        for (const QString &filePath : fileNames) {
            DataImporter importer;
            SessionData tempSessionData;

            if (importer.importFile(filePath, tempSessionData)) {
                // Force groundElev calculation on the temp session
                tempSessionData.getAttribute(SessionKeys::GroundElev);

                // Collect for batch merge
                importedSessions.append(tempSessionData);

                // Collect min and max time from tempSessionData
                for (const QString &sensorKey : tempSessionData.sensorKeys()) {
                    QVector<double> times = tempSessionData.getMeasurement(sensorKey, xAxisKey);
                    if (!times.isEmpty()) {
                        double minTime = *std::min_element(times.begin(), times.end());
                        double maxTime = *std::max_element(times.begin(), times.end());
                        newMinTime = std::min(newMinTime, minTime);
                        newMaxTime = std::max(newMaxTime, maxTime);
                    }
                }
            } else {
                // Failed import with an error message
                QString errorMessage = importer.getLastError();
                qWarning() << "Failed to import file:" << filePath << "Error:" << errorMessage;

                // Compute relative path if baseDir is provided
                QString displayPath;
                if (!baseDir.isEmpty()) {
                    QDir dir(baseDir);
                    displayPath = dir.relativeFilePath(filePath);
                    // If the file is outside baseDir, fallback to absolute path or just the file name
                    if (displayPath == filePath) {
                        displayPath = QFileInfo(filePath).fileName(); // Alternatively, keep absolute path
                    }
                } else {
                    displayPath = filePath; // No baseDir provided, use absolute path
                }

                failedImports.insert(displayPath, errorMessage);
            }
        }
    }

    // Batch merge all imported sessions
    model->mergeSessions(importedSessions);

    // Emit the new time range if valid
    if (newMinTime != std::numeric_limits<double>::max() && newMaxTime != std::numeric_limits<double>::lowest()) {
        emit newTimeRange(newMinTime, newMaxTime);
    }

    // Display completion message
    if (failedImports.size() > 5) {
        QString message = tr("Import has been completed.");
        message += tr("\nHowever, %1 files failed to import.").arg(failedImports.size());
        // Optionally, list the first few failed files
        QStringList failedList = failedImports.keys();
        int displayCount = qMin(failedList.size(), 10); // Limit to first 10 for brevity
        QString displayedFailedList = failedList.mid(0, displayCount).join("\n");
        if (failedList.size() > displayCount) {
            displayedFailedList += tr("\n...and %1 more.").arg(failedList.size() - displayCount);
        }
        message += tr("\nFailed Files:\n") + displayedFailedList;
        QMessageBox::warning(this, tr("Import Completed with Some Failures"), message);
    } else if (!failedImports.isEmpty()) {
        // List all failed imports
        QStringList failedList = failedImports.keys();
        QString message = tr("Import has been completed.");
        message += tr("\nHowever, some files failed to import:");
        message += "\n" + failedList.join("\n");
        QMessageBox::warning(this, tr("Import Completed with Some Failures"), message);
    }
}

void MainWindow::on_action_Pan_triggered()
{
    plotWidget->setCurrentTool(PlotWidget::Tool::Pan);
    qDebug() << "Switched to Pan tool";
}

void MainWindow::on_action_Zoom_triggered()
{
    plotWidget->setCurrentTool(PlotWidget::Tool::Zoom);
    qDebug() << "Switched to Zoom tool";
}

void MainWindow::on_action_Select_triggered()
{
    plotWidget->setCurrentTool(PlotWidget::Tool::Select);
    qDebug() << "Switched to Select tool";
}

void MainWindow::on_action_SetExit_triggered()
{
    plotWidget->setCurrentTool(PlotWidget::Tool::SetExit);
    qDebug() << "Switched to Set Exit tool";
}

void MainWindow::on_action_SetGround_triggered()
{
    plotWidget->setCurrentTool(PlotWidget::Tool::SetGround);
    qDebug() << "Switched to Set Ground tool";
}

void MainWindow::on_action_ShowSelected_triggered()
{
    setSelectedTrackCheckState(Qt::Checked);
}

void MainWindow::on_action_HideSelected_triggered()
{
    setSelectedTrackCheckState(Qt::Unchecked);
}

void MainWindow::on_action_HideOthers_triggered()
{
    QList<QModelIndex> selectedRows = logbookView->selectedRows();
    if (selectedRows.isEmpty())
        return;

    // Hide all rows, then remove selected rows from the map
    QMap<int, bool> visibility;
    int totalRows = model->rowCount();
    for (int i = 0; i < totalRows; ++i) {
        visibility.insert(i, false);
    }
    for (const QModelIndex &idx : selectedRows) {
        visibility.remove(idx.row());
    }
    model->setRowsVisibility(visibility);
}

void MainWindow::on_action_Delete_triggered()
{
    // Get selected rows from the logbook view
    QList<QModelIndex> selectedRows = logbookView->selectedRows();

    if (selectedRows.isEmpty()) {
        QMessageBox::information(this, tr("Delete Tracks"), tr("No tracks selected for deletion."));
        return;
    }

    // Collect SESSION_IDs of the selected sessions
    QList<QString> sessionIdsToRemove;
    for (const QModelIndex &index : selectedRows) {
        if (!index.isValid())
            continue;

        // Assuming the SESSION_ID is stored as an attribute in SessionData
        // You might need to adjust this based on your actual implementation
        const SessionData &session = model->getAllSessions().at(index.row());
        QString sessionId = session.getAttribute(SessionKeys::SessionId).toString();
        sessionIdsToRemove.append(sessionId);
    }

    // Confirm deletion with the user
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(
        this,
        tr("Delete Tracks"),
        tr("Are you sure you want to delete the selected %1 track(s)?").arg(sessionIdsToRemove.size()),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes) {
        return; // User canceled the deletion
    }

    // Proceed to remove the sessions from the model
    bool success = model->removeSessions(sessionIdsToRemove);

    if (success) {
        QMessageBox::information(
            this,
            tr("Delete Tracks"),
            tr("Selected track(s) have been successfully deleted.")
            );
    } else {
        QMessageBox::warning(
            this,
            tr("Delete Tracks"),
            tr("Failed to delete the selected track(s).")
            );
    }
}

void MainWindow::on_action_Preferences_triggered()
{
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // User clicked OK
    }
}

void MainWindow::on_action_Exit_triggered()
{
    close();  // Close the main window
}

void MainWindow::onPlotWidgetToolChanged(PlotWidget::Tool t)
{
    switch (t) {
    case PlotWidget::Tool::Pan:
        ui->action_Pan->setChecked(true);
        break;
    case PlotWidget::Tool::Zoom:
        ui->action_Zoom->setChecked(true);
        break;
    case PlotWidget::Tool::Select:
        ui->action_Select->setChecked(true);
        break;
    case PlotWidget::Tool::SetExit:
        ui->action_SetExit->setChecked(true);
        break;
    case PlotWidget::Tool::SetGround:
        ui->action_SetGround->setChecked(true);
        break;
    }
}

void MainWindow::registerBuiltInMarkers()
{
    QVector<MarkerDefinition> defaults = {
        // Category: Reference
        {"Reference", "Exit", QColor(0, 122, 204), SessionKeys::ExitTime},
        {"Reference", "Start", QColor(0, 153, 51), SessionKeys::StartTime},
    };

    for (auto &md : defaults)
        MarkerRegistry::instance().registerMarker(md);
}

void MainWindow::registerBuiltInPlots()
{
    // Angular spread for grouped colours
    const int group_a = 40;

    // use the exact vector you already have…
    QVector<PlotValue> defaults = {
        // Category: GNSS
        {"GNSS", "Elevation", "m", Qt::black, "GNSS", "z", "altitude"},
        {"GNSS", "Horizontal speed", "m/s", Qt::red, "GNSS", "velH", "speed"},
        {"GNSS", "Vertical speed", "m/s", Qt::green, "GNSS", "velD", "vertical_speed"},
        {"GNSS", "Total speed", "m/s", Qt::blue, "GNSS", "vel", "speed"},
        {"GNSS", "Vertical acceleration", "m/s^2", Qt::green, "GNSS", "accD", "acceleration"},
        {"GNSS", "Horizontal accuracy", "m", Qt::darkRed, "GNSS", "hAcc", "distance"},
        {"GNSS", "Vertical accuracy", "m", Qt::darkGreen, "GNSS", "vAcc", "distance"},
        {"GNSS", "Speed accuracy", "m/s", Qt::darkBlue, "GNSS", "sAcc", "speed"},
        {"GNSS", "Number of satellites", "", Qt::darkMagenta, "GNSS", "numSV", "count"},

        // Category: IMU
        {"IMU", "Acceleration X", "g", QColor::fromHsl(360 - group_a, 255, 128), "IMU", "ax", "acceleration"},
        {"IMU", "Acceleration Y", "g", QColor::fromHsl(0, 255, 128), "IMU", "ay", "acceleration"},
        {"IMU", "Acceleration Z", "g", QColor::fromHsl(group_a, 255, 128), "IMU", "az", "acceleration"},
        {"IMU", "Total acceleration", "g", QColor::fromHsl(0, 255, 128), "IMU", "aTotal", "acceleration"},

        {"IMU", "Rotation X", "deg/s", QColor::fromHsl(120 - group_a, 255, 128), "IMU", "wx", "rotation"},
        {"IMU", "Rotation Y", "deg/s", QColor::fromHsl(120, 255, 128), "IMU", "wy", "rotation"},
        {"IMU", "Rotation Z", "deg/s", QColor::fromHsl(120 + group_a, 255, 128), "IMU", "wz", "rotation"},
        {"IMU", "Total rotation", "deg/s", QColor::fromHsl(120, 255, 128), "IMU", "wTotal", "rotation"},

        {"IMU", "Temperature", QString::fromUtf8("\302\260C"), QColor::fromHsl(45, 255, 128), "IMU", "temperature", "temperature"},

        // Category: Magnetometer
        {"Magnetometer", "Magnetic field X", "gauss", QColor::fromHsl(240 - group_a, 255, 128), "MAG", "x", "magnetic_field"},
        {"Magnetometer", "Magnetic field Y", "gauss", QColor::fromHsl(240, 255, 128), "MAG", "y", "magnetic_field"},
        {"Magnetometer", "Magnetic field Z", "gauss", QColor::fromHsl(240 + group_a, 255, 128), "MAG", "z", "magnetic_field"},
        {"Magnetometer", "Total magnetic field", "gauss", QColor::fromHsl(240, 255, 128), "MAG", "total", "magnetic_field"},

        {"Magnetometer", "Temperature", QString::fromUtf8("\302\260C"), QColor::fromHsl(135, 255, 128), "MAG", "temperature", "temperature"},

        // Category: Barometer
        {"Barometer", "Air pressure", "Pa", QColor::fromHsl(0, 0, 64), "BARO", "pressure", "pressure"},
        {"Barometer", "Temperature", QString::fromUtf8("\302\260C"), QColor::fromHsl(225, 255, 128), "BARO", "temperature", "temperature"},

        // Category: Humidity
        {"Humidity", "Humidity", "%", QColor::fromHsl(0, 0, 128), "HUM", "humidity", "percentage"},
        {"Humidity", "Temperature", QString::fromUtf8("\302\260C"), QColor::fromHsl(315, 255, 128), "HUM", "temperature", "temperature"},

        // Category: Battery
        {"Battery", "Battery voltage", "V", QColor::fromHsl(30, 255, 128), "VBAT", "voltage", "voltage"},

        // Category: GNSS time
        {"GNSS time", "Time of week", "s", QColor::fromHsl(0, 0, 64), "TIME", "tow", "time"},
        {"GNSS time", "Week number", "", QColor::fromHsl(0, 0, 128), "TIME", "week", "count"},

        // Category: Sensor fusion
        {"Sensor fusion", "North position", "m", QColor::fromHsl(240 - group_a, 255, 128), SessionKeys::ImuGnssEkf, "posN", "distance"},
        {"Sensor fusion", "East position", "m", QColor::fromHsl(240, 255, 128), SessionKeys::ImuGnssEkf, "posE", "distance"},
        {"Sensor fusion", "Down position", "m", QColor::fromHsl(240 + group_a, 255, 128), SessionKeys::ImuGnssEkf, "posD", "distance"},

        {"Sensor fusion", "North velocity", "m/s", QColor::fromHsl(180 - group_a, 255, 128), SessionKeys::ImuGnssEkf, "velN", "speed"},
        {"Sensor fusion", "East velocity", "m/s", QColor::fromHsl(180, 255, 128), SessionKeys::ImuGnssEkf, "velE", "speed"},
        {"Sensor fusion", "Down velocity", "m/s", QColor::fromHsl(180 + group_a, 255, 128), SessionKeys::ImuGnssEkf, "velD", "speed"},

        {"Sensor fusion", "Horizontal acceleration", "g", Qt::magenta, SessionKeys::ImuGnssEkf, "accH", "acceleration"},
        {"Sensor fusion", "Vertical acceleration", "g", Qt::cyan, SessionKeys::ImuGnssEkf, "accD", "acceleration"},

        {"Sensor fusion", "X rotation", "deg", QColor::fromHsl(120 - group_a, 255, 128), SessionKeys::ImuGnssEkf, "roll", "angle"},
        {"Sensor fusion", "Y rotation", "deg", QColor::fromHsl(120, 255, 128), SessionKeys::ImuGnssEkf, "pitch", "angle"},
        {"Sensor fusion", "Z rotation", "deg", QColor::fromHsl(120 + group_a, 255, 128), SessionKeys::ImuGnssEkf, "yaw", "angle"},
    };

    for (auto &pv : defaults)
        PlotRegistry::instance().registerPlot(pv);
}

void MainWindow::setupPlotSelectionDock()
{
    // Create the dock widget
    plotSelectionDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Plot Selection"));
    plotTreeView = new QTreeView(plotSelectionDock);
    plotSelectionDock->setWidget(plotTreeView);
    addDockWidget(plotSelectionDock, KDDockWidgets::Location_OnLeft);

    // Attach the model
    plotTreeView->setModel(plotModel);
    plotTreeView->setHeaderHidden(true);
    plotTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Let PlotModel own the category/plot tree
    if (plotModel) {
        plotModel->setPlots(PlotRegistry::instance().allPlots());
    }
}

void MainWindow::setupMarkerSelectionDock()
{
    // Create the marker selection dock widget
    markerDock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Marker Selection"));
    markerTreeView = new QTreeView(markerDock);
    markerDock->setWidget(markerTreeView);
    addDockWidget(markerDock, KDDockWidgets::Location_OnLeft);

    // Attach the model
    markerTreeView->setModel(markerModel);
    markerTreeView->setHeaderHidden(true);
    markerTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Populate marker tree from registry
    if (markerModel) {
        markerModel->setMarkers(MarkerRegistry::instance().allMarkers());
    }
}

void MainWindow::initializePreferences()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // ========================================================================
    // General Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::GeneralUnits, QStringLiteral("Metric"));
    prefs.registerPreference(PreferenceKeys::GeneralLogbookFolder,
                             QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    // ========================================================================
    // Import Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::ImportGroundReferenceMode, QStringLiteral("Automatic"));
    prefs.registerPreference(PreferenceKeys::ImportFixedElevation, 0.0);

    // ========================================================================
    // Global Plot Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::PlotsLineThickness, 1.0);
    prefs.registerPreference(PreferenceKeys::PlotsTextSize, 9);
    prefs.registerPreference(PreferenceKeys::PlotsCrosshairColor, QVariant::fromValue(QColor(Qt::gray)));
    prefs.registerPreference(PreferenceKeys::PlotsCrosshairThickness, 1.0);
    prefs.registerPreference(PreferenceKeys::PlotsYAxisPadding, 0.05);

    // ========================================================================
    // Per-Plot Preferences (dynamically registered from PlotRegistry)
    // ========================================================================
    const QVector<PlotValue> allPlots = PlotRegistry::instance().allPlots();
    for (const PlotValue &pv : allPlots) {
        // Color preference (default from PlotValue.defaultColor)
        prefs.registerPreference(
            PreferenceKeys::plotColorKey(pv.sensorID, pv.measurementID),
            QVariant::fromValue(pv.defaultColor)
        );

        // Y-axis mode preference (auto, fixed)
        prefs.registerPreference(
            PreferenceKeys::plotYAxisModeKey(pv.sensorID, pv.measurementID),
            QStringLiteral("auto")
        );

        // Y-axis min/max values (used when mode is "fixed")
        prefs.registerPreference(
            PreferenceKeys::plotYAxisMinKey(pv.sensorID, pv.measurementID),
            0.0
        );
        prefs.registerPreference(
            PreferenceKeys::plotYAxisMaxKey(pv.sensorID, pv.measurementID),
            100.0
        );
    }

    // ========================================================================
    // Per-Marker Preferences (dynamically registered from MarkerRegistry)
    // ========================================================================
    const QVector<MarkerDefinition> allMarkers = MarkerRegistry::instance().allMarkers();
    for (const MarkerDefinition &md : allMarkers) {
        // Color preference (default from MarkerDefinition.color)
        prefs.registerPreference(
            PreferenceKeys::markerColorKey(md.attributeKey),
            QVariant::fromValue(md.color)
        );
    }

    // ========================================================================
    // Legend Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::LegendTextSize, 9);

    // ========================================================================
    // Map Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::MapLineThickness, 3.0);
    prefs.registerPreference(PreferenceKeys::MapMarkerSize, 10);
    prefs.registerPreference(PreferenceKeys::MapTrackOpacity, 0.85);
}

void MainWindow::initializeCalculatedAttributes()
{
    SessionData::registerCalculatedAttribute(
        SessionKeys::ExitTime,
        {
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "sAcc"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        // Find the first timestamp where vertical speed drops below a threshold
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> sAcc = session.getMeasurement("GNSS", "sAcc");
        QVector<double> accD = session.getMeasurement("GNSS", "accD");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size()) {
            qWarning() << "Insufficient data to calculate exit time.";
            return std::nullopt;
        }

        const double vThreshold = 10.0; // Vertical speed threshold in m/s
        const double maxAccuracy = 1.0; // Maximum speed acccuracy in m/s
        const double minAcceleration = 2.5; // Minimum vertical accleration in m/s^2

        for (int i = 1; i < velD.size(); ++i) {
            // Get interpolation coefficient
            const double a = (vThreshold - velD[i - 1]) / (velD[i] - velD[i - 1]);

            // Check vertical speed
            if (a < 0 || 1 < a) continue;

            // Check accuracy
            const double acc = sAcc[i - 1] + a * (sAcc[i] - sAcc[i - 1]);
            if (acc > maxAccuracy) continue;

            // Check acceleration
            const double az = accD[i - 1] + a * (accD[i] - accD[i - 1]);
            if (az < minAcceleration) continue;

            // Determine exit
            const double tExit = time[i - 1] + a * (time[i] - time[i - 1]) - vThreshold / az;
            return QDateTime::fromMSecsSinceEpoch((qint64)(tExit * 1000.0), QTimeZone::utc());
        }

        qWarning() << "Exit time could not be determined based on current data.";
        return QDateTime::fromMSecsSinceEpoch((qint64)(time.back() * 1000.0), QTimeZone::utc());
    });

    // Register for all sensors
    QStringList all_sensors = {"GNSS", "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : all_sensors) {
        SessionData::registerCalculatedAttribute(
            SessionKeys::StartTime,
            {
                DependencyKey::measurement(sens, SessionKeys::Time)
            },
            [sens](SessionData &session) -> std::optional<QVariant> {
            // Retrieve GNSS/time measurement
            QVector<double> times = session.getMeasurement(sens, SessionKeys::Time);
            if (times.isEmpty()) {
                qWarning() << "No " << sens << "/time data available to calculate start time.";
                return std::nullopt;
            }

            double startTime = *std::min_element(times.begin(), times.end());
            return QDateTime::fromMSecsSinceEpoch((qint64)(startTime * 1000.0), QTimeZone::utc());
        });

        SessionData::registerCalculatedAttribute(
            SessionKeys::Duration,
            {
                DependencyKey::measurement(sens, SessionKeys::Time)
            },
            [sens](SessionData &session) -> std::optional<QVariant> {
            QVector<double> times = session.getMeasurement(sens, SessionKeys::Time);
            if (times.isEmpty()) {
                qWarning() << "No " << sens << "/time data available to calculate duration.";
                return std::nullopt;
            }

            double minTime = *std::min_element(times.begin(), times.end());
            double maxTime = *std::max_element(times.begin(), times.end());
            double durationSec = maxTime - minTime;
            if (durationSec < 0) {
                qWarning() << "Invalid " << sens << "/time data (max < min).";
                return std::nullopt;
            }

            return durationSec;
        });
    }

    SessionData::registerCalculatedAttribute(
        SessionKeys::GroundElev,
        {
            DependencyKey::measurement("GNSS", "hMSL")
        },
        [](SessionData &session) -> std::optional<QVariant> {
        PreferencesManager &prefs = PreferencesManager::instance();
        QString mode = prefs.getValue("import/groundReferenceMode").toString();
        double fixedElevation = prefs.getValue("import/fixedElevation").toDouble();

        if (mode == "Fixed") {
            // Always return the fixed elevation from preferences
            return fixedElevation;
        } else if (mode == "Automatic") {
            // Use some GNSS/hMSL measurement from session
            QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
            if (!hMSL.isEmpty()) {
                // e.g. use the last hMSL sample
                double groundElev = hMSL.last();
                return groundElev;
            } else {
                // Not enough data to compute
                // Return no value => the calculation fails for now
                return std::nullopt;
            }
        } else {
            // Possibly a fallback or “no ground reference” if mode is unknown
            return std::nullopt;
        }
    });
}

void MainWindow::initializeCalculatedMeasurements()
{
    SessionData::registerCalculatedMeasurement(
        "GNSS", "z",
        {
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::attribute(SessionKeys::GroundElev)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");

        bool ok;
        double groundElev = session.getAttribute(SessionKeys::GroundElev).toDouble(&ok);
        if (!ok) {
            qWarning() << "Cannot calculate z due to missing groundElev";
            return std::nullopt;
        }

        if (hMSL.isEmpty()) {
            qWarning() << "Cannot calculate z due to missing hMSL";
            return std::nullopt;
        }

        QVector<double> z;
        z.reserve(hMSL.size());
        for(int i = 0; i < hMSL.size(); ++i){
            z.append(hMSL[i] - groundElev);
        }
        return z;
    });

    SessionData::registerCalculatedMeasurement(
        "GNSS", "velH",
        {
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");

        if (velN.isEmpty() || velE.isEmpty()) {
            qWarning() << "Cannot calculate velH due to missing velN or velE";
            return std::nullopt;
        }

        if (velN.size() != velE.size()) {
            qWarning() << "velN and velE size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
            return std::nullopt;
        }

        QVector<double> velH;
        velH.reserve(velN.size());
        for(int i = 0; i < velN.size(); ++i){
            velH.append(std::sqrt(velN[i]*velN[i] + velE[i]*velE[i]));
        }
        return velH;
    });

    SessionData::registerCalculatedMeasurement(
        "GNSS", "vel",
        {
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", "velD")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (velH.isEmpty() || velD.isEmpty()) {
            qWarning() << "Cannot calculate vel due to missing velH or velD";
            return std::nullopt;
        }

        if (velH.size() != velD.size()) {
            qWarning() << "velH and velD size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
            return std::nullopt;
        }

        QVector<double> vel;
        vel.reserve(velH.size());
        for(int i = 0; i < velH.size(); ++i){
            vel.append(std::sqrt(velH[i]*velH[i] + velD[i]*velD[i]));
        }
        return vel;
    });

    SessionData::registerCalculatedMeasurement(
        "IMU", "aTotal",
        {
            DependencyKey::measurement("IMU", "ax"),
            DependencyKey::measurement("IMU", "ay"),
            DependencyKey::measurement("IMU", "az")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> ax = session.getMeasurement("IMU", "ax");
        QVector<double> ay = session.getMeasurement("IMU", "ay");
        QVector<double> az = session.getMeasurement("IMU", "az");

        if (ax.isEmpty() || ay.isEmpty() || az.isEmpty()) {
            qWarning() << "Cannot calculate aTotal due to missing ax, ay, or az";
            return std::nullopt;
        }

        if ((ax.size() != ay.size()) || (ax.size() != az.size())) {
            qWarning() << "az, ay, or az size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
            return std::nullopt;
        }

        QVector<double> aTotal;
        aTotal.reserve(ax.size());
        for(int i = 0; i < ax.size(); ++i){
            aTotal.append(std::sqrt(ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]));
        }
        return aTotal;
    });

    SessionData::registerCalculatedMeasurement(
        "IMU", "wTotal",
        {
            DependencyKey::measurement("IMU", "wx"),
            DependencyKey::measurement("IMU", "wy"),
            DependencyKey::measurement("IMU", "wz")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> wx = session.getMeasurement("IMU", "wx");
        QVector<double> wy = session.getMeasurement("IMU", "wy");
        QVector<double> wz = session.getMeasurement("IMU", "wz");

        if (wx.isEmpty() || wy.isEmpty() || wz.isEmpty()) {
            qWarning() << "Cannot calculate wTotal due to missing wx, wy, or wz";
            return std::nullopt;
        }

        if ((wx.size() != wy.size()) || (wx.size() != wz.size())) {
            qWarning() << "wz, wy, or wz size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
            return std::nullopt;
        }

        QVector<double> wTotal;
        wTotal.reserve(wx.size());
        for(int i = 0; i < wx.size(); ++i){
            wTotal.append(std::sqrt(wx[i]*wx[i] + wy[i]*wy[i] + wz[i]*wz[i]));
        }
        return wTotal;
    });

    SessionData::registerCalculatedMeasurement(
        "MAG", "total",
        {
            DependencyKey::measurement("MAG", "x"),
            DependencyKey::measurement("MAG", "y"),
            DependencyKey::measurement("MAG", "z")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> x = session.getMeasurement("MAG", "x");
        QVector<double> y = session.getMeasurement("MAG", "y");
        QVector<double> z = session.getMeasurement("MAG", "z");

        if (x.isEmpty() || y.isEmpty() || z.isEmpty()) {
            qWarning() << "Cannot calculate total due to missing x, y, or z";
            return std::nullopt;
        }

        if ((x.size() != y.size()) || (x.size() != z.size())) {
            qWarning() << "x, y, or z size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
            return std::nullopt;
        }

        QVector<double> total;
        total.reserve(x.size());
        for(int i = 0; i < x.size(); ++i){
            total.append(std::sqrt(x[i]*x[i] + y[i]*y[i] + z[i]*z[i]));
        }
        return total;
    });

    // Helper lambda to compute _time for non-GNSS sensors
    auto compute_time = [](SessionData &session, const QString &sensorKey) -> std::optional<QVector<double>> {
        // Check that sensor is allowed
        const QStringList allowedSensors = { "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT" };
        if (!allowedSensors.contains(sensorKey)) {
            return std::nullopt;
        }

        // We need TIME sensor data and a linear fit
        bool haveFit = session.hasAttribute(SessionKeys::TimeFitA) && session.hasAttribute(SessionKeys::TimeFitB);
        double a = 0.0, b = 0.0;
        if (!haveFit) {
            // Attempt to compute the fit
            if (!session.hasMeasurement("TIME", "time") ||
                !session.hasMeasurement("TIME", "tow") ||
                !session.hasMeasurement("TIME", "week")) {
                // TIME sensor not available or incomplete data
                return std::nullopt;
            }

            QVector<double> systemTime = session.getMeasurement("TIME", "time");
            QVector<double> tow = session.getMeasurement("TIME", "tow");
            QVector<double> week = session.getMeasurement("TIME", "week");

            int N = std::min({systemTime.size(), tow.size(), week.size()});
            if (N < 2) {
                // Not enough points for a linear fit
                return std::nullopt;
            }

            // Compute UTC time
            QVector<double> utcTime(N);
            for (int i = 0; i < N; ++i) {
                utcTime[i] = week[i] * 604800 + tow[i] + 315964800;
            }

            // Perform a linear fit: utcTime = a*systemTime + b
            double sumS = 0.0, sumU = 0.0, sumSS = 0.0, sumSU = 0.0;
            for (int i = 0; i < N; ++i) {
                double S = systemTime[i];
                double U = utcTime[i];
                sumS += S;
                sumU += U;
                sumSS += S * S;
                sumSU += S * U;
            }

            double denom = (N * sumSS - sumS * sumS);
            if (denom == 0.0) {
                // Degenerate fit
                return std::nullopt;
            }

            a = (N * sumSU - sumS * sumU) / denom;
            b = (sumU - a * sumS) / N;

            // Store fit parameters
            session.setAttribute(SessionKeys::TimeFitA, QString::number(a, 'g', 17));
            session.setAttribute(SessionKeys::TimeFitB, QString::number(b, 'g', 17));
        } else {
            // Already computed fit
            a = session.getAttribute(SessionKeys::TimeFitA).toDouble();
            b = session.getAttribute(SessionKeys::TimeFitB).toDouble();
        }

        // Now convert the sensor's 'time' measurement using the linear fit
        if (!session.hasMeasurement(sensorKey, "time")) {
            return std::nullopt;
        }

        QVector<double> sensorSystemTime = session.getMeasurement(sensorKey, "time");
        QVector<double> result(sensorSystemTime.size());
        for (int i = 0; i < sensorSystemTime.size(); ++i) {
            result[i] = a * sensorSystemTime[i] + b;
        }
        return result;
    };

    // Register for GNSS
    const QStringList gnss_sensors = {"GNSS"};
    for (const QString &sens : gnss_sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::Time,
            {
                DependencyKey::measurement(sens, "time")
            },
            [sens](SessionData& session) -> std::optional<QVector<double>> {
                QVector<double> sensTime = session.getMeasurement(sens, "time");

                if (sensTime.isEmpty()) {
                    qWarning() << "Cannot calculate time from epoch";
                    return std::nullopt;
                }

                return sensTime;
            });
    }

    // Register for other sensors
    const QStringList sensors = {"BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::Time,
            {
                DependencyKey::measurement(sens, "time")
            },
            [compute_time, sens](SessionData &s) -> std::optional<QVector<double>> {
            return compute_time(s, sens);
        });
    }

    // Helper lambda to compute time from exit
    auto compute_time_from_exit = [](SessionData &session, const QString &sensorKey) -> std::optional<QVector<double>> {
        // Get raw time first to force calculation if needed
        QVector<double> rawTime = session.getMeasurement(sensorKey, SessionKeys::Time);

        // Then get exit time attribute
        QVariant var = session.getAttribute(SessionKeys::ExitTime);
        if (!var.canConvert<QDateTime>()) {
            return std::nullopt;
        }

        QDateTime dt = var.toDateTime();
        if (!dt.isValid()) {
            return std::nullopt;
        }

        // If you need the exit time as a double (seconds since epoch):
        double exitTime = dt.toMSecsSinceEpoch() / 1000.0;

        // Now calculate the difference
        QVector<double> result(rawTime.size());
        for (int i = 0; i < rawTime.size(); ++i) {
            result[i] = rawTime[i] - exitTime;
        }
        return result;
    };

    // Register for all sensors
    QStringList all_sensors = {"GNSS", "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT", SessionKeys::ImuGnssEkf};
    for (const QString &sens : all_sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::TimeFromExit,
            {
                DependencyKey::measurement(sens, SessionKeys::Time),
                DependencyKey::attribute(SessionKeys::ExitTime)
            },
            [compute_time_from_exit, sens](SessionData &s) {
            return compute_time_from_exit(s, sens);
        });
    }
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accD",
        {
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (velD.isEmpty()) {
            qWarning() << "Cannot calculate accD due to missing velD";
            return std::nullopt;
        }

        if (time.size() != velD.size()) {
            qWarning() << "Cannot calculate accD because time and velD size mismatch.";
            return std::nullopt;
        }

        // If there's fewer than two samples, we cannot compute acceleration.
        if (velD.size() < 2) {
            qWarning() << "Not enough data points to calculate accD.";
            return std::nullopt;
        }

        QVector<double> accD;
        accD.reserve(velD.size());

        // For the first sample (i = 0), use forward difference:
        // a[0] = (velD[1] - velD[0]) / (time[1] - time[0])
        {
            double dt = time[1] - time[0];
            if (dt == 0.0) {
                qWarning() << "Zero time difference encountered between indices 0 and 1.";
                return std::nullopt;
            }
            double a = (velD[1] - velD[0]) / dt;
            accD.append(a);
        }

        // For the interior points (1 <= i <= velD.size()-2), use centered difference:
        // a[i] = (velD[i+1] - velD[i-1]) / (time[i+1] - time[i-1])
        for (int i = 1; i < velD.size() - 1; ++i) {
            double dt = time[i+1] - time[i-1];
            if (dt == 0.0) {
                qWarning() << "Zero time difference encountered for indices" << i-1 << "and" << i+1;
                return std::nullopt;
            }
            double a = (velD[i+1] - velD[i-1]) / dt;
            accD.append(a);
        }

        // For the last sample (i = velD.size()-1), use backward difference:
        // a[last] = (velD[last] - velD[last-1]) / (time[last] - time[last-1])
        {
            int last = velD.size() - 1;
            double dt = time[last] - time[last-1];
            if (dt == 0.0) {
                qWarning() << "Zero time difference encountered at the end indices:" << last-1 << "and" << last;
                return std::nullopt;
            }
            double a = (velD[last] - velD[last-1]) / dt;
            accD.append(a);
        }

        return accD;
    });

    // Define output mappings
    struct FusionOutputMapping {
        QString key;
        QVector<double> FusionOutput::*member;
    };

    static const std::vector<FusionOutputMapping> fusion_outputs = {
        {SessionKeys::Time, &FusionOutput::time},
        {"posN", &FusionOutput::posN},
        {"posE", &FusionOutput::posE},
        {"posD", &FusionOutput::posD},
        {"velN", &FusionOutput::velN},
        {"velE", &FusionOutput::velE},
        {"velD", &FusionOutput::velD},
        {"accN", &FusionOutput::accN},
        {"accE", &FusionOutput::accE},
        {"accD", &FusionOutput::accD},
        {"roll", &FusionOutput::roll},
        {"pitch", &FusionOutput::pitch},
        {"yaw", &FusionOutput::yaw}
    };

    auto compute_imu_gnss_ekf = [](SessionData &session, const QString &outputKey) -> std::optional<QVector<double>> {
        QVector<double> gnssTime = session.getMeasurement("GNSS", SessionKeys::Time);
        QVector<double> lat = session.getMeasurement("GNSS", "lat");
        QVector<double> lon = session.getMeasurement("GNSS", "lon");
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> hAcc = session.getMeasurement("GNSS", "hAcc");
        QVector<double> vAcc = session.getMeasurement("GNSS", "vAcc");
        QVector<double> sAcc = session.getMeasurement("GNSS", "sAcc");

        QVector<double> imuTime = session.getMeasurement("IMU", SessionKeys::Time);
        QVector<double> ax = session.getMeasurement("IMU", "ax");
        QVector<double> ay = session.getMeasurement("IMU", "ay");
        QVector<double> az = session.getMeasurement("IMU", "az");
        QVector<double> wx = session.getMeasurement("IMU", "wx");
        QVector<double> wy = session.getMeasurement("IMU", "wy");
        QVector<double> wz = session.getMeasurement("IMU", "wz");

        if (gnssTime.isEmpty() || lat.isEmpty() || lon.isEmpty() || hMSL.isEmpty() ||
            velN.isEmpty() || velE.isEmpty() || velD.isEmpty() || hAcc.isEmpty() ||
            vAcc.isEmpty() || sAcc.isEmpty() || imuTime.isEmpty() || ax.isEmpty() ||
            ay.isEmpty() || az.isEmpty() || wx.isEmpty() || wy.isEmpty() || wz.isEmpty()) {
            qWarning() << "Cannot calculate EKF due to missing data";
            return std::nullopt;
        }

        // Run the fusion
        FusionOutput out = runFusion(
            gnssTime, lat, lon, hMSL, velN, velE, velD, hAcc, vAcc, sAcc,
            imuTime, ax, ay, az, wx, wy, wz);

        std::optional<QVector<double>> result;

        // Iterate over all outputs and either store or return the requested one
        for (const auto &entry : fusion_outputs) {
            if (entry.key == outputKey) {
                result = out.*(entry.member);  // This is the requested key, return it
            } else {
                session.setCalculatedMeasurement(SessionKeys::ImuGnssEkf, entry.key, out.*(entry.member));
            }
        }

        return result;
    };

    // Register for all outputs dynamically using `fusion_outputs`
    for (const auto &entry : fusion_outputs) {
        SessionData::registerCalculatedMeasurement(
            SessionKeys::ImuGnssEkf, entry.key,
            {
                DependencyKey::measurement("GNSS", SessionKeys::Time),
                DependencyKey::measurement("GNSS", "lat"),
                DependencyKey::measurement("GNSS", "lon"),
                DependencyKey::measurement("GNSS", "hMSL"),
                DependencyKey::measurement("GNSS", "velN"),
                DependencyKey::measurement("GNSS", "velE"),
                DependencyKey::measurement("GNSS", "velD"),
                DependencyKey::measurement("GNSS", "hAcc"),
                DependencyKey::measurement("GNSS", "vAcc"),
                DependencyKey::measurement("GNSS", "sAcc"),
                DependencyKey::measurement("IMU", SessionKeys::Time),
                DependencyKey::measurement("IMU", "ax"),
                DependencyKey::measurement("IMU", "ay"),
                DependencyKey::measurement("IMU", "az"),
                DependencyKey::measurement("IMU", "wx"),
                DependencyKey::measurement("IMU", "wy"),
                DependencyKey::measurement("IMU", "wz")
            },
            [compute_imu_gnss_ekf, key = entry.key](SessionData &s) {
                return compute_imu_gnss_ekf(s, key);
            });
    }

    SessionData::registerCalculatedMeasurement(
        SessionKeys::ImuGnssEkf, "accH",
        {
            DependencyKey::measurement(SessionKeys::ImuGnssEkf, "accN"),
            DependencyKey::measurement(SessionKeys::ImuGnssEkf, "accE")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
            QVector<double> accN = session.getMeasurement(SessionKeys::ImuGnssEkf, "accN");
            QVector<double> accE = session.getMeasurement(SessionKeys::ImuGnssEkf, "accE");

            if (accN.isEmpty() || accE.isEmpty()) {
                qWarning() << "Cannot calculate accH due to missing accN or accE";
                return std::nullopt;
            }

            if (accN.size() != accE.size()) {
                qWarning() << "accN and accE size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
                return std::nullopt;
            }

            QVector<double> accH;
            accH.reserve(accN.size());
            for(int i = 0; i < accN.size(); ++i){
                accH.append(std::sqrt(accN[i]*accN[i] + accE[i]*accE[i]));
            }
            return accH;
        });

    // Define the keys we want to expose
    struct SimpOutputMapping {
        QString key;
        int vectorIndex; // 0=lat, 1=lon, 2=alt, 3=time
    };

    // We will generate 4 vectors. 
    // We register the calculation trigger on "lat", but it populates all 4.
    static const std::vector<SimpOutputMapping> simp_outputs = {
        {"lat", 0}, {"lon", 1}, {"hMSL", 2}, {SessionKeys::Time, 3}
    };

    auto compute_simplified_track = [](SessionData &session, const QString &outputKey) -> std::optional<QVector<double>> {
        // 1. Gather Dependencies
        QVector<double> rawLat = session.getMeasurement("GNSS", "lat");
        QVector<double> rawLon = session.getMeasurement("GNSS", "lon");
        QVector<double> rawAlt = session.getMeasurement("GNSS", "hMSL");
        QVector<double> rawTime = session.getMeasurement("GNSS", SessionKeys::Time);

        if (rawLat.isEmpty() || rawLat.size() != rawLon.size() || 
            rawLat.size() != rawAlt.size() || rawLat.size() != rawTime.size()) {
            return std::nullopt;
        }

        // 2. Setup Geometry Types
        namespace bg = boost::geometry;
        using PointXY = bg::model::d2::point_xy<double>;
        using LineString = bg::model::linestring<PointXY>;

        // 3. Project to Local Cartesian (Meters)
        // Center projection on the first point
        GeographicLib::LocalCartesian proj(rawLat[0], rawLon[0], rawAlt[0]);
        
        LineString pathInMeters;
        pathInMeters.reserve(rawLat.size());

        // We store the original index to retrieve the correct timestamp/altitude later
        // Map: PointIndex -> OriginalIndex
        std::vector<size_t> indexMap; 
        indexMap.reserve(rawLat.size());

        for (int i = 0; i < rawLat.size(); ++i) {
            double x, y, z;
            proj.Forward(rawLat[i], rawLon[i], rawAlt[i], x, y, z);
            
            // Note: RDP is 2D simplification. If vertical simplifcation is critical, 
            // you need a 3D point type, but usually 2D (ground track) is sufficient for maps.
            bg::append(pathInMeters, PointXY(x, y)); 
        }

        // 4. Run RDP Simplification
        // Epsilon: 0.5 meters (Sensor noise floor)
        LineString simplifiedPath;
        bg::simplify(pathInMeters, simplifiedPath, 0.5); 

        // 5. Unproject & Reconstruct
        // WARNING: RDP destroys indices. However, Boost's simplified points 
        // are a subset of original points (usually). 
        // BUT: Floating point errors during projection/unprojection might make 
        // strict equality checks on lat/lon risky for finding the original timestamp.
        
        // ROBUST APPROACH: 
        // Since we need Time and Alt synced, we should ideally simplify the 3D structure 
        // or map back to the nearest original point. 
        // For strict RDP, the points in `simplifiedPath` correspond exactly to specific 
        // indices in `pathInMeters`. 
        
        // To keep this snippet simple, let's reverse project the X/Y to get Lat/Lon,
        // and linearly interpolate Time/Alt based on the cumulative distance or 
        // simply perform a nearest-neighbor search on the original raw data 
        // to recover the timestamp. 
        
        // Let's use a simplified strategy: 
        // Re-project the simplified XY back to Lat/Lon.
        // For Time/Alt, we must rely on the fact that RDP preserves vertices.
        // We can iterate the original list to find the matching points.

        QVector<double> outLat, outLon, outAlt, outTime;
        outLat.reserve(simplifiedPath.size());
        outLon.reserve(simplifiedPath.size());
        outAlt.reserve(simplifiedPath.size());
        outTime.reserve(simplifiedPath.size());

        size_t rawIdx = 0;
        for (const auto& pt : simplifiedPath) {
            // Find this point in the original path (it must exist, in order)
            // We search forward from the last found index.
            // We compare in Projected Meter Space to avoid float fuzziness.
            
            double targetX = pt.x();
            double targetY = pt.y();
            
            // Simple tolerance search
            for (; rawIdx < rawLat.size(); ++rawIdx) {
                double x, y, z;
                proj.Forward(rawLat[rawIdx], rawLon[rawIdx], rawAlt[rawIdx], x, y, z);
                
                if (std::abs(x - targetX) < 1e-3 && std::abs(y - targetY) < 1e-3) {
                    // Match found
                    outLat.append(rawLat[rawIdx]);
                    outLon.append(rawLon[rawIdx]);
                    outAlt.append(rawAlt[rawIdx]);
                    outTime.append(rawTime[rawIdx]);
                    break;
                }
            }
        }
        
        // 6. Store outputs in SessionData (Cache)
        // Store everything *except* the one we are about to return 
        // (to avoid double-setting logic if you want, though setCalculatedMeasurement is safe)
        
        session.setCalculatedMeasurement("Simplified", "lat", outLat);
        session.setCalculatedMeasurement("Simplified", "lon", outLon);
        session.setCalculatedMeasurement("Simplified", "hMSL", outAlt);
        session.setCalculatedMeasurement("Simplified", SessionKeys::Time, outTime);

        // 7. Return the specific requested vector
        if (outputKey == "lat") return outLat;
        if (outputKey == "lon") return outLon;
        if (outputKey == "hMSL") return outAlt;
        if (outputKey == SessionKeys::Time) return outTime;

        return std::nullopt;
    };

    // Register the dependency
    for (const auto &entry : simp_outputs) {
        SessionData::registerCalculatedMeasurement(
            "Simplified", entry.key,
            {
                // Dependency on RAW GNSS data
                DependencyKey::measurement("GNSS", "lat"),
                DependencyKey::measurement("GNSS", "lon"),
                DependencyKey::measurement("GNSS", "hMSL"),
                DependencyKey::measurement("GNSS", SessionKeys::Time)
            },
            [compute_simplified_track, key = entry.key](SessionData &s) {
                return compute_simplified_track(s, key);
            });
    }
}

void MainWindow::initializeXAxisMenu()
{
    const QString activeKeyInSettings = currentXAxisKey();

    struct AxisChoice {
        QString menuText;
        QString key;
        QString axisLabel;
    };

    QVector<AxisChoice> choices = {
        {tr("Time from exit (s)"), SessionKeys::TimeFromExit, tr("Time from exit (s)")},
        {tr("UTC time (s)"),      SessionKeys::Time,        tr("Time (s)")}
    };

    QMenu *plotsMenu = ui->menuPlots;
    Q_ASSERT(plotsMenu);

    QMenu *xAxisMenu = plotsMenu->addMenu(tr("Horizontal Axis"));
    plotsMenu->addSeparator();

    QActionGroup *axisGroup = new QActionGroup(this);
    axisGroup->setExclusive(true);

    bool initialActionSet = false;
    for (const AxisChoice &ch : choices) {
        QAction *a = xAxisMenu->addAction(ch.menuText);
        a->setCheckable(true);
        a->setData(ch.key);
        a->setProperty("axisLabel", ch.axisLabel);
        axisGroup->addAction(a);

        // If this matches what’s in settings, check it now
        if (ch.key == activeKeyInSettings) {
            a->setChecked(true);
            setXAxisKey(ch.key, ch.axisLabel);
            initialActionSet = true;
        }

        connect(a, &QAction::triggered, this, [this, ch]() {
            setXAxisKey(ch.key, ch.axisLabel);
        });
    }

    // If no action matched or settings missing, default to first choice
    if (!initialActionSet && !axisGroup->actions().isEmpty()) {
        QAction *firstAction = axisGroup->actions().first();
        firstAction->setChecked(true);
        setXAxisKey(firstAction->data().toString(),
                    firstAction->property("axisLabel").toString());
    }
}

void MainWindow::initializePlotsMenu()
{
    // Access the 'Plots' menu from the UI
    QMenu *plotsMenu = ui->menuPlots;

    // Define the list of plots to include in the 'Plots' menu, including separators
    QVector<PlotMenuItem> plotsMenuItems = {
        // First Group: GNSS-related plots
        PlotMenuItem("Elevation", QKeySequence(Qt::Key_E), "GNSS", "z"),

        // Separator
        PlotMenuItem(PlotMenuItemType::Separator),

        PlotMenuItem("Horizontal Speed", QKeySequence(Qt::Key_H), "GNSS", "velH"),
        PlotMenuItem("Vertical Speed", QKeySequence(Qt::Key_V), "GNSS", "velD"),
        PlotMenuItem("Total Speed", QKeySequence(Qt::Key_S), "GNSS", "vel"),

        // Separator
        PlotMenuItem(PlotMenuItemType::Separator),

        PlotMenuItem("Horizontal Accuracy", QKeySequence(Qt::SHIFT | Qt::Key_H), "GNSS", "hAcc"),
        PlotMenuItem("Vertical Accuracy", QKeySequence(Qt::SHIFT | Qt::Key_V), "GNSS", "vAcc"),
        PlotMenuItem("Speed Accuracy", QKeySequence(Qt::SHIFT | Qt::Key_S), "GNSS", "sAcc"),

        // Separator
        PlotMenuItem(PlotMenuItemType::Separator),

        PlotMenuItem("Number of Satellites", QKeySequence(Qt::SHIFT | Qt::Key_N), "GNSS", "numSV"),

        // Add more groups and plots as needed
    };

    // Iterate over the list and create corresponding actions
    for(const PlotMenuItem &item : plotsMenuItems){
        if(item.type == PlotMenuItemType::Separator){
            plotsMenu->addSeparator();
            continue; // Move to the next item
        }

        QAction *action = new QAction(item.menuText, this);
        action->setShortcut(item.shortcut);

        // Combine sensorID and measurementID into a single string for data storage
        QString actionData = item.sensorID + "|" + item.measurementID;
        action->setData(actionData);

        // Connect each action to a lambda that calls togglePlot with appropriate parameters
        connect(action, &QAction::triggered, this, [this, action]() {
            QString data = action->data().toString();
            QStringList parts = data.split('|');
            if(parts.size() == 2){
                QString sensorID = parts.at(0);
                QString measurementID = parts.at(1);
                togglePlot(sensorID, measurementID);
            }
        });

        // Add the action to the 'Plots' menu
        plotsMenu->addAction(action);
    }

    // Add separator and units toggle at the end
    ui->menu_Tools->addSeparator();
    QAction *toggleUnitsAction = new QAction(tr("Toggle Units"), this);
    toggleUnitsAction->setShortcut(QKeySequence(Qt::Key_U));
    connect(toggleUnitsAction, &QAction::triggered,
            this, &MainWindow::on_action_ToggleUnits_triggered);
    ui->menu_Tools->addAction(toggleUnitsAction);
}

void MainWindow::initializeWindowMenu()
{
    QMenu *windowMenu = ui->menuWindow;
    Q_ASSERT(windowMenu);

    auto addDockAction = [windowMenu](KDDockWidgets::QtWidgets::DockWidget *dock, const QString &text) {
        if (!dock)
            return;

        QAction *a = dock->toggleAction();
        a->setText(text);
        windowMenu->addAction(a);
    };

    addDockAction(logbookDock, tr("Logbook"));
    addDockAction(plotsDock, tr("Plots"));
    addDockAction(legendDock, tr("Legend"));
    addDockAction(mapDock, tr("Map"));
    addDockAction(plotSelectionDock, tr("Plot Selection"));
    addDockAction(markerDock, tr("Marker Selection"));
    addDockAction(videoDock, tr("Video"));
}

void MainWindow::togglePlot(const QString &sensorID, const QString &measurementID)
{
    if (!plotModel) {
        return;
    }

    plotModel->togglePlot(sensorID, measurementID);

    // Ensure the plot selection view is visible
    if (!plotSelectionDock->isVisible()) {
        plotSelectionDock->show();
    }
}

void MainWindow::setSelectedTrackCheckState(Qt::CheckState state)
{
    // Get all selected rows from the logbook view
    QList<QModelIndex> selectedRows = logbookView->selectedRows();
    if (selectedRows.isEmpty())
        return;

    QMap<int, bool> visibility;
    for (const QModelIndex &idx : selectedRows) {
        visibility.insert(idx.row(), state == Qt::Checked);
    }
    model->setRowsVisibility(visibility);
}

void MainWindow::setupPlotTools()
{
    // Set up the tool action group
    toolActionGroup = new QActionGroup(this);
    toolActionGroup->setExclusive(true);

    // Add tool actions to the group
    toolActionGroup->addAction(ui->action_Pan);
    toolActionGroup->addAction(ui->action_Zoom);
    toolActionGroup->addAction(ui->action_Select);
    toolActionGroup->addAction(ui->action_SetExit);
    toolActionGroup->addAction(ui->action_SetGround);

    // Set Pan as the default checked tool
    ui->action_Pan->setChecked(true);
}

// Accessors for persisting the current x-axis measurement key
QString MainWindow::currentXAxisKey() const
{
    if (!m_plotViewSettingsModel)
        return SessionKeys::TimeFromExit;

    return m_plotViewSettingsModel->xAxisKey();
}

void MainWindow::setXAxisKey(const QString &key, const QString &label)
{
    if (!m_plotViewSettingsModel)
        return;

    m_plotViewSettingsModel->setXAxis(key, label);
}

void MainWindow::on_action_ToggleUnits_triggered()
{
    UnitConverter &converter = UnitConverter::instance();
    QStringList systems = converter.availableSystems();

    if (systems.isEmpty())
        return;

    // Find current system index
    QString current = converter.currentSystem();
    int currentIndex = systems.indexOf(current);

    // Cycle to next system (wrap around)
    int nextIndex = (currentIndex + 1) % systems.size();
    QString nextSystem = systems.at(nextIndex);

    // Apply the change
    converter.setSystem(nextSystem);
}

} // namespace FlySight
