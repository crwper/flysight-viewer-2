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

#include "dataimporter.h"
#include "dependencykey.h"
#include "pluginhost.h"
#include "ui/docks/DockRegistry.h"
#include "ui/docks/DockFeature.h"
#include "ui/docks/AppContext.h"
#include "ui/docks/logbook/LogbookDockFeature.h"
#include "ui/docks/plot/PlotDockFeature.h"
#include "ui/docks/plotselection/PlotSelectionDockFeature.h"
#include "ui/docks/video/VideoDockFeature.h"
#include "ui/docks/plot/PlotWidget.h"
#include "ui/docks/logbook/LogbookView.h"
#include "ui/docks/video/VideoWidget.h"
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
#include "measuremodel.h"
#include "units/unitconverter.h"
#include "calculations/calculatedvalueregistry.h"

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
#ifdef Q_OS_MACOS
    QString defaultDir = QCoreApplication::applicationDirPath() + "/../Resources/python_plugins";
#else
    QString defaultDir = QCoreApplication::applicationDirPath() + "/python_plugins";
#endif
    QString pluginDir = qEnvironmentVariable("FLYSIGHT_PLUGINS", defaultDir);
    PluginHost::instance().initialise(pluginDir);

    // Initialize preferences (must come AFTER plots and markers are registered)
    initializePreferences();

    // Initialize calculated values
    CalculatedValueRegistry::instance().registerBuiltInCalculations();

    // Populate marker model before PlotWidget construction so markers can render immediately
    if (markerModel) {
        markerModel->setMarkers(MarkerRegistry::instance().allMarkers());
    }

    // Create range model for synchronizing plot x-axis range with other docks
    m_rangeModel = new PlotRangeModel(this);

    // Create measure model for measure tool data
    m_measureModel = new MeasureModel(this);

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

    // Create all docks via registry
    AppContext ctx;
    ctx.sessionModel = model;
    ctx.plotModel = plotModel;
    ctx.markerModel = markerModel;
    ctx.cursorModel = m_cursorModel;
    ctx.rangeModel = m_rangeModel;
    ctx.plotViewSettings = m_plotViewSettingsModel;
    ctx.measureModel = m_measureModel;
    ctx.settings = m_settings;

    m_features = DockRegistry::createAll(ctx, this);

    for (auto* feature : m_features) {
        addDockWidget(feature->dock(), feature->defaultLocation());
    }

    // Find feature instances for signal connections
    auto* logbookFeature = findFeature<LogbookDockFeature>();
    auto* plotFeature = findFeature<PlotDockFeature>();
    auto* videoFeature = findFeature<VideoDockFeature>();

    // Connect logbook signals
    if (logbookFeature) {
        connect(logbookFeature, &LogbookDockFeature::showSelectedRequested,
                this, &MainWindow::on_action_ShowSelected_triggered);
        connect(logbookFeature, &LogbookDockFeature::hideSelectedRequested,
                this, &MainWindow::on_action_HideSelected_triggered);
        connect(logbookFeature, &LogbookDockFeature::hideOthersRequested,
                this, &MainWindow::on_action_HideOthers_triggered);
        connect(logbookFeature, &LogbookDockFeature::deleteRequested,
                this, &MainWindow::on_action_Delete_triggered);
    }

    // Connect plot signals
    if (plotFeature) {
        auto* plotWidget = plotFeature->plotWidget();
        if (plotWidget) {
            connect(this, &MainWindow::newTimeRange, plotWidget, &PlotWidget::setXAxisRange);
            connect(plotFeature, &PlotDockFeature::toolChanged, this, &MainWindow::onPlotWidgetToolChanged);

            // Cross-dock connections
            if (logbookFeature && logbookFeature->logbookView()) {
                connect(plotFeature, &PlotDockFeature::sessionsSelected,
                        logbookFeature->logbookView(), &LogbookView::selectSessions);
            }
        }
    }

    // Let PlotModel own the category/plot tree (must be done after plugin initialization)
    if (plotModel) {
        plotModel->setPlots(PlotRegistry::instance().allPlots());
    }

    // Restore the previous dock layout (includes visibility/open/closed state).
    restoreDockLayout();

    // Initialize the Window menu (dock visibility)
    initializeWindowMenu();

    // Initialize the Plots menu
    initializeXAxisMenu();
    initializePlotsMenu();

    // Setup plot tools
    setupPlotTools();
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
    auto* videoFeature = findFeature<VideoDockFeature>();
    if (videoFeature && videoFeature->dock() && !videoFeature->dock()->isVisible()) {
        videoFeature->dock()->show();
    }

    // Load/replace the video in the widget
    if (videoFeature && videoFeature->videoWidget()) {
        videoFeature->videoWidget()->loadVideo(fileName);
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
    auto* plotFeature = findFeature<PlotDockFeature>();
    if (plotFeature && plotFeature->plotWidget()) {
        plotFeature->plotWidget()->setCurrentTool(PlotWidget::Tool::Pan);
        qDebug() << "Switched to Pan tool";
    }
}

void MainWindow::on_action_Zoom_triggered()
{
    auto* plotFeature = findFeature<PlotDockFeature>();
    if (plotFeature && plotFeature->plotWidget()) {
        plotFeature->plotWidget()->setCurrentTool(PlotWidget::Tool::Zoom);
        qDebug() << "Switched to Zoom tool";
    }
}

void MainWindow::on_action_Measure_triggered()
{
    auto* plotFeature = findFeature<PlotDockFeature>();
    if (plotFeature && plotFeature->plotWidget()) {
        plotFeature->plotWidget()->setCurrentTool(PlotWidget::Tool::Measure);
        qDebug() << "Switched to Measure tool";
    }
}

void MainWindow::on_action_Select_triggered()
{
    auto* plotFeature = findFeature<PlotDockFeature>();
    if (plotFeature && plotFeature->plotWidget()) {
        plotFeature->plotWidget()->setCurrentTool(PlotWidget::Tool::Select);
        qDebug() << "Switched to Select tool";
    }
}

void MainWindow::on_action_SetExit_triggered()
{
    auto* plotFeature = findFeature<PlotDockFeature>();
    if (plotFeature && plotFeature->plotWidget()) {
        plotFeature->plotWidget()->setCurrentTool(PlotWidget::Tool::SetExit);
        qDebug() << "Switched to Set Exit tool";
    }
}

void MainWindow::on_action_SetGround_triggered()
{
    auto* plotFeature = findFeature<PlotDockFeature>();
    if (plotFeature && plotFeature->plotWidget()) {
        plotFeature->plotWidget()->setCurrentTool(PlotWidget::Tool::SetGround);
        qDebug() << "Switched to Set Ground tool";
    }
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
    auto* logbookFeature = findFeature<LogbookDockFeature>();
    if (!logbookFeature || !logbookFeature->logbookView())
        return;

    QList<QModelIndex> selectedRows = logbookFeature->logbookView()->selectedRows();
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
    auto* logbookFeature = findFeature<LogbookDockFeature>();
    if (!logbookFeature || !logbookFeature->logbookView())
        return;

    // Get selected rows from the logbook view
    QList<QModelIndex> selectedRows = logbookFeature->logbookView()->selectedRows();

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
    case PlotWidget::Tool::Measure:
        ui->action_Measure->setChecked(true);
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

    // Add Zoom to Extent action at the top of Tools menu
    QAction *zoomToExtentAction = new QAction(tr("Zoom to Extent"), this);
    zoomToExtentAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Z));
    connect(zoomToExtentAction, &QAction::triggered, this, [this]() {
        auto* plotFeature = findFeature<PlotDockFeature>();
        if (plotFeature && plotFeature->plotWidget()) {
            plotFeature->plotWidget()->zoomToExtent();
        }
    });
    ui->menu_Tools->insertAction(ui->action_Pan, zoomToExtentAction);
    ui->menu_Tools->insertSeparator(ui->action_Pan);

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

    for (auto* feature : m_features) {
        QAction* a = feature->toggleAction();
        if (a) {
            a->setText(feature->title());
            windowMenu->addAction(a);
        }
    }
}

void MainWindow::togglePlot(const QString &sensorID, const QString &measurementID)
{
    if (!plotModel) {
        return;
    }

    plotModel->togglePlot(sensorID, measurementID);

    // Ensure the plot selection view is visible
    auto* plotSelectionFeature = findFeature<PlotSelectionDockFeature>();
    if (plotSelectionFeature && plotSelectionFeature->dock()) {
        if (!plotSelectionFeature->dock()->isVisible()) {
            plotSelectionFeature->dock()->show();
        }
    }
}

void MainWindow::setSelectedTrackCheckState(Qt::CheckState state)
{
    auto* logbookFeature = findFeature<LogbookDockFeature>();
    if (!logbookFeature || !logbookFeature->logbookView())
        return;

    // Get all selected rows from the logbook view
    QList<QModelIndex> selectedRows = logbookFeature->logbookView()->selectedRows();
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
    toolActionGroup->addAction(ui->action_Measure);
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
