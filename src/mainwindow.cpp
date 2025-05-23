#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDockWidget>
#include <QStandardItem>
#include <QTreeView>
#include <QFileDialog>
#include <QProgressDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <QMouseEvent>
#include <QStandardPaths>

#include "dataimporter.h"
#include "dependencykey.h"
#include "plotwidget.h"
#include "pluginhost.h"
#include "preferences/preferencesdialog.h"
#include "preferences/preferencesmanager.h"
#include "sessiondata.h"

namespace FlySight {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(new QSettings("FlySight", "Viewer", this))
    , ui(new Ui::MainWindow)
    , model(new SessionModel(this))
    , plotModel (new QStandardItemModel(this))
    , m_currentXAxisKey(m_settings->value("plot/xAxisKey", SessionKeys::TimeFromExit).toString())
    , m_currentXAxisLabel(tr("Time from exit (s)"))
{
    ui->setupUi(this);

    // Initialize preferences
    initializePreferences();

    // Initialize calculated values
    initializeCalculatedAttributes();
    initializeCalculatedMeasurements();

    // Initialize plugin calculations
    QString defaultDir = QCoreApplication::applicationDirPath() + "/plugins";
    QString pluginDir = qEnvironmentVariable("FLYSIGHT_PLUGINS", defaultDir);

    registerBuiltInPlots();
    PluginHost::instance().initialise(pluginDir);

    // Add logbook view
    QDockWidget *logbookDock = new QDockWidget(tr("Logbook"), this);
    logbookView = new LogbookView(model, this);
    logbookDock->setWidget(logbookView);
    addDockWidget(Qt::RightDockWidgetArea, logbookDock);

    connect(logbookView, &LogbookView::showSelectedRequested, this, &MainWindow::on_action_ShowSelected_triggered);
    connect(logbookView, &LogbookView::hideSelectedRequested, this, &MainWindow::on_action_HideSelected_triggered);
    connect(logbookView, &LogbookView::hideOthersRequested, this, &MainWindow::on_action_HideOthers_triggered);
    connect(logbookView, &LogbookView::deleteRequested, this, &MainWindow::on_action_Delete_triggered);

    // Add plot widget
    plotWidget = new PlotWidget(model, plotModel, this);
    setCentralWidget(plotWidget);

    // Connect the newTimeRange signal to PlotWidget's setXAxisRange slot
    connect(this, &MainWindow::newTimeRange, plotWidget, &PlotWidget::setXAxisRange);

    // Setup plot values
    setupPlotValues();

    // Initialize the Plots menu
    initializeXAxisMenu();
    initializePlotsMenu();

    // Setup plot tools
    setupPlotTools();

    // Plot selection
    connect(plotWidget, &PlotWidget::sessionsSelected, logbookView, &LogbookView::selectSessions);

    // Handle plot tool changes
    connect(plotWidget, &PlotWidget::toolChanged, this, &MainWindow::onPlotWidgetToolChanged);

    // Handle x-axis changes
    connect(this, &MainWindow::xAxisKeyChanged, plotWidget, &PlotWidget::onXAxisKeyChanged);
    emit xAxisKeyChanged(m_currentXAxisKey, m_currentXAxisLabel);
}

MainWindow::~MainWindow()
{
    delete ui;
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

                // Merge tempSessionData into model
                model->mergeSessionData(tempSessionData);

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
                // Merge tempSessionData into model
                model->mergeSessionData(tempSessionData);

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

    QSet<int> selectedRowsSet;
    for (const QModelIndex &idx : selectedRows) {
        selectedRowsSet.insert(idx.row());
    }

    // Block signals to prevent multiple dataChanged emissions
    model->blockSignals(true);

    int totalRows = model->rowCount();
    for (int i = 0; i < totalRows; ++i) {
        bool visible = selectedRowsSet.contains(i);
        model->setData(model->index(i, SessionModel::Description),
                       visible ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
    }

    // Unblock signals
    model->blockSignals(false);

    // Emit a single dataChanged for the entire range or the rows you altered
    emit model->dataChanged(model->index(0, 0),
                            model->index(totalRows - 1, model->columnCount() - 1));

    // Emit modelChanged() if your logic requires it
    emit model->modelChanged();
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

void MainWindow::registerBuiltInPlots()
{
    // Angular spread for grouped colours
    const int group_a = 40;

    // use the exact vector you already have…
    QVector<PlotValue> defaults = {
        // Category: GNSS
        {"GNSS", "Elevation", "m", Qt::black, "GNSS", "z"},
        {"GNSS", "Horizontal speed", "m/s", Qt::red, "GNSS", "velH"},
        {"GNSS", "Vertical speed", "m/s", Qt::green, "GNSS", "velD"},
        {"GNSS", "Total speed", "m/s", Qt::blue, "GNSS", "vel"},
        {"GNSS", "Vertical acceleration", "m/s^2", Qt::green, "GNSS", "accD"},
        {"GNSS", "Horizontal accuracy", "m", Qt::darkRed, "GNSS", "hAcc"},
        {"GNSS", "Vertical accuracy", "m", Qt::darkGreen, "GNSS", "vAcc"},
        {"GNSS", "Speed accuracy", "m/s", Qt::darkBlue, "GNSS", "sAcc"},
        {"GNSS", "Number of satellites", "", Qt::darkMagenta, "GNSS", "numSV"},

        // Category: IMU
        {"IMU", "Acceleration X", "g", QColor::fromHsl(360 - group_a, 255, 128), "IMU", "ax"},
        {"IMU", "Acceleration Y", "g", QColor::fromHsl(0, 255, 128), "IMU", "ay"},
        {"IMU", "Acceleration Z", "g", QColor::fromHsl(group_a, 255, 128), "IMU", "az"},
        {"IMU", "Total acceleration", "g", QColor::fromHsl(0, 255, 128), "IMU", "aTotal"},

        {"IMU", "Rotation X", "deg/s", QColor::fromHsl(120 - group_a, 255, 128), "IMU", "wx"},
        {"IMU", "Rotation Y", "deg/s", QColor::fromHsl(120, 255, 128), "IMU", "wy"},
        {"IMU", "Rotation Z", "deg/s", QColor::fromHsl(120 + group_a, 255, 128), "IMU", "wz"},
        {"IMU", "Total rotation", "deg/s", QColor::fromHsl(120, 255, 128), "IMU", "wTotal"},

        {"IMU", "Temperature", "°C", QColor::fromHsl(45, 255, 128), "IMU", "temperature"},

        // Category: Magnetometer
        {"Magnetometer", "Magnetic field X", "gauss", QColor::fromHsl(240 - group_a, 255, 128), "MAG", "x"},
        {"Magnetometer", "Magnetic field Y", "gauss", QColor::fromHsl(240, 255, 128), "MAG", "y"},
        {"Magnetometer", "Magnetic field Z", "gauss", QColor::fromHsl(240 + group_a, 255, 128), "MAG", "z"},
        {"Magnetometer", "Total magnetic field", "gauss", QColor::fromHsl(240, 255, 128), "MAG", "total"},

        {"Magnetometer", "Temperature", "°C", QColor::fromHsl(135, 255, 128), "MAG", "temperature"},

        // Category: Barometer
        {"Barometer", "Air pressure", "Pa", QColor::fromHsl(0, 0, 64), "BARO", "pressure"},
        {"Barometer", "Temperature", "°C", QColor::fromHsl(225, 255, 128), "BARO", "temperature"},

        // Category: Humidity
        {"Humidity", "Humidity", "%", QColor::fromHsl(0, 0, 128), "HUM", "humidity"},
        {"Humidity", "Temperature", "°C", QColor::fromHsl(315, 255, 128), "HUM", "temperature"},

        // Category: Battery
        {"Battery", "Battery voltage", "V", QColor::fromHsl(30, 255, 128), "VBAT", "voltage"},

        // Category: GNSS time
        {"GNSS time", "Time of week", "s", QColor::fromHsl(0, 0, 64), "TIME", "tow"},
        {"GNSS time", "Week number", "", QColor::fromHsl(0, 0, 128), "TIME", "week"},
    };

    for (auto &pv : defaults)
        PlotRegistry::instance().registerPlot(pv);
}

void MainWindow::setupPlotValues()
{
    // Create the dock widget
    plotDock = new QDockWidget(tr("Plot Selection"), this);
    plotTreeView = new QTreeView(plotDock);
    plotDock->setWidget(plotTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, plotDock);

    // Attach the model
    plotTreeView->setModel(plotModel);
    plotTreeView->setHeaderHidden(true);
    plotTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Populate the model with plot values and track the first checked item
    QVector<PlotValue> plotValues = PlotRegistry::instance().allPlots();
    populatePlotModel(plotModel, plotValues);

    // Connect to clicked signal to handle plot selection
    connect(plotTreeView, &QTreeView::clicked, this, [this](const QModelIndex &index) {
        if (!index.isValid())
            return;
        // Check if item is checkable
        QVariant checkStateVar = plotModel->data(index, Qt::CheckStateRole);
        if (!checkStateVar.isValid())
            return;

        Qt::CheckState state = static_cast<Qt::CheckState>(checkStateVar.toInt());
        Qt::CheckState newState = (state == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
        plotModel->setData(index, newState, Qt::CheckStateRole);

        // Update views
        emit model->modelChanged();
    });

    // Emit the initial plot based on the checked items
    emit model->modelChanged();
}

void MainWindow::populatePlotModel(
    QStandardItemModel* plotModel,
    const QVector<PlotValue>& plotValues)
{
    // Create a map to keep track of category items
    QMap<QString, QStandardItem*> categoryItemsMap;

    for (const PlotValue& pv : plotValues) {
        // Check if the category already exists
        if (!categoryItemsMap.contains(pv.category)) {
            // Create a new category item
            QStandardItem* categoryItem = new QStandardItem(pv.category);
            categoryItem->setFlags(Qt::ItemIsEnabled); // Non-checkable

            // Append the category to the model
            plotModel->appendRow(categoryItem);

            // Add to the map
            categoryItemsMap.insert(pv.category, categoryItem);
        }

        // Create a new plot item
        QStandardItem* plotItem = new QStandardItem(pv.plotName);
        plotItem->setCheckable(true); // Make the plot checkable
        plotItem->setCheckState(Qt::Unchecked);

        plotItem->setFlags(plotItem->flags() & ~Qt::ItemIsEditable);
        plotItem->setFlags(plotItem->flags() & ~Qt::ItemIsUserCheckable);

        // Store data in the item
        plotItem->setData(pv.defaultColor, DefaultColorRole);
        plotItem->setData(pv.sensorID, SensorIDRole);
        plotItem->setData(pv.measurementID, MeasurementIDRole);
        plotItem->setData(pv.plotUnits, PlotUnitsRole);

        // Append the plot item under its category
        categoryItemsMap[pv.category]->appendRow(plotItem);
    }
}

void MainWindow::initializePreferences()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // Register preferences with keys and default values
    prefs.registerPreference("general/units", "Metric");
    prefs.registerPreference("general/logbookFolder", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    prefs.registerPreference("import/groundReferenceMode", "Automatic");
    prefs.registerPreference("import/fixedElevation", 0.0);
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
    QStringList all_sensors = {"GNSS", "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
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
}

void MainWindow::initializeXAxisMenu()
{
    // 1) pull in the saved key (default to TimeFromExit)
    const QString activeKeyInSettings = m_settings->value("plot/xAxisKey", SessionKeys::TimeFromExit).toString();
    m_currentXAxisKey = activeKeyInSettings; // Sync internal with settings initially

    // 2) define all of your possible axes in one place
    struct AxisChoice {
        QString        menuText;
        QKeySequence   shortcut;
        QString        key;
        QString        axisLabel;
    };
    const QVector<AxisChoice> choices = {
        { tr("Time from Exit"),
         QKeySequence(Qt::CTRL | Qt::Key_1),
         SessionKeys::TimeFromExit,
         tr("Time from exit (s)") },
        { tr("UTC Time"),
         QKeySequence(Qt::CTRL | Qt::Key_2),
         SessionKeys::Time,
         tr("Time (s)") }
    };

    // 3) create the submenu and an exclusive group
    QMenu* plotsMenu = ui->menuPlots;
    Q_ASSERT(plotsMenu);
    QMenu* xAxisMenu = plotsMenu->addMenu(tr("Horizontal Axis"));
    plotsMenu->addSeparator();

    QActionGroup* axisGroup = new QActionGroup(this);
    axisGroup->setExclusive(true);

    // 4) build each QAction in a loop
    bool initialActionSet = false;
    for (const AxisChoice& ch : choices) {
        QAction* a = xAxisMenu->addAction(ch.menuText);
        a->setCheckable(true);
        a->setShortcut(ch.shortcut);
        a->setData(ch.key);
        a->setProperty("axisLabel", ch.axisLabel);
        axisGroup->addAction(a);

        if (ch.key == m_currentXAxisKey) {
            a->setChecked(true);
            m_currentXAxisLabel = ch.axisLabel; // Set the correct initial label
            initialActionSet = true;
        }

        connect(a, &QAction::triggered, this, [this, a_captured = a]{
            const QString keyFromAction   = a_captured->data().toString();
            const QString labelFromAction = a_captured->property("axisLabel").toString();
            this->setXAxisKey(keyFromAction, labelFromAction);
        });
    }

    // 5) finally, apply the saved choice now that the menu is built
    if (!initialActionSet && !choices.isEmpty() && !axisGroup->actions().isEmpty()) {
        QAction* firstAction = axisGroup->actions().first();
        firstAction->setChecked(true);
        // Update internal state and settings to reflect this default
        setXAxisKey(firstAction->data().toString(), firstAction->property("axisLabel").toString());
        qWarning() << "MainWindow: No saved X-axis key matched choices, defaulted to:" << m_currentXAxisKey;
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

    // Add a separator before the "Show Plot Selection" action
    plotsMenu->addSeparator();

    // Create the "Show Plot Selection" action
    QAction *showPlotSelectionAction = new QAction(tr("Show Plot Selection"), this);
    showPlotSelectionAction->setCheckable(true);
    showPlotSelectionAction->setChecked(plotDock->isVisible());

    // Connect the action to toggle plotDock visibility
    connect(showPlotSelectionAction, &QAction::triggered, this, [this](bool checked){
        plotDock->setVisible(checked);
    });


    // Synchronize the action's check state with plotDock's visibility changes
    connect(plotDock, &QDockWidget::visibilityChanged, showPlotSelectionAction, &QAction::setChecked);

    // Add the "Show Plot Selection" action to the 'Plots' menu
    plotsMenu->addAction(showPlotSelectionAction);
}

void MainWindow::togglePlot(const QString &sensorID, const QString &measurementID)
{
    // Iterate through the plotModel to find the matching plot
    for(int row = 0; row < plotModel->rowCount(); ++row){
        QStandardItem *categoryItem = plotModel->item(row);
        for(int col = 0; col < categoryItem->rowCount(); ++col){
            QStandardItem *plotItem = categoryItem->child(col);
            if(plotItem->data(SensorIDRole).toString() == sensorID &&
                plotItem->data(MeasurementIDRole).toString() == measurementID){
                // Toggle the check state
                Qt::CheckState currentState = plotItem->checkState();
                Qt::CheckState newState = (currentState == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
                plotItem->setCheckState(newState);

                // Ensure the plot selection view is visible
                if(!plotDock->isVisible()){
                    plotDock->show();
                }

                // Emit modelChanged to update the plots
                emit model->modelChanged();

                return;
            }
        }
    }

    qWarning() << "Plot not found for sensorID:" << sensorID << "measurementID:" << measurementID;
}

void MainWindow::setSelectedTrackCheckState(Qt::CheckState state)
{
    // Get all selected rows from the logbook view
    QList<QModelIndex> selectedRows = logbookView->selectedRows();
    if (selectedRows.isEmpty())
        return;

    // Block signals to prevent multiple dataChanged emissions
    model->blockSignals(true);

    // Set check state for selected sessions
    for (const QModelIndex &idx : selectedRows) {
        model->setData(model->index(idx.row(), SessionModel::Description), state, Qt::CheckStateRole);
    }

    // Unblock signals
    model->blockSignals(false);

    // Emit a single dataChanged for the entire range or the rows you altered
    int totalRows = model->rowCount();
    emit model->dataChanged(model->index(0, 0),
                            model->index(totalRows - 1, model->columnCount() - 1));

    // Emit modelChanged() if your logic requires it
    emit model->modelChanged();
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
QString MainWindow::currentXAxisKey() const {
    return m_settings
        ->value("plot/xAxisKey", SessionKeys::TimeFromExit)
        .toString();
}

void MainWindow::setXAxisKey(const QString &key, const QString &label) {
    if (m_currentXAxisKey != key || m_currentXAxisLabel != label) {
        m_currentXAxisKey = key;
        m_currentXAxisLabel = label;
        m_settings->setValue("plot/xAxisKey", key); // Save preference
        qDebug() << "MainWindow: X-axis key changing to" << key << "with label" << label;
        emit xAxisKeyChanged(key, label); // Emit the signal
    }
}

} // namespace FlySight
