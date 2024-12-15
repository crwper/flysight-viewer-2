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

#include "import.h"
#include "plotwidget.h"
#include "sessiondata.h"

namespace FlySight {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(new QSettings("FlySight", "Viewer", this))
    , ui(new Ui::MainWindow)
    , model(new SessionModel(this))
    , plotModel (new QStandardItemModel(this))
{
    ui->setupUi(this);

    // Initialize calculated values
    initializeCalculatedValues();

    // Add logbook view
    QDockWidget *logbookDock = new QDockWidget(tr("Logbook"), this);
    logbookView = new LogbookView(model, this);
    logbookDock->setWidget(logbookView);
    addDockWidget(Qt::RightDockWidgetArea, logbookDock);

    // Add plot widget
    PlotWidget *plotWidget = new PlotWidget(model, plotModel, this);
    setCentralWidget(plotWidget);

    // Connect the newTimeRange signal to PlotWidget's setXAxisRange slot
    connect(this, &MainWindow::newTimeRange, plotWidget, &PlotWidget::setXAxisRange);

    // Setup plot values
    setupPlotValues();
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

    // Call the helper function without showing progress
    importFiles(fileNames, false);

    // Update last used folder
    QString lastUsedFolder = QFileInfo(fileNames.last()).absolutePath();
    m_settings->setValue("folder", lastUsedFolder);
}

void MainWindow::on_actionImportFolder_triggered()
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

    // Call the helper function with showing progress
    importFiles(filesToImport, filesToImport.size() > 5);
}

void MainWindow::importFiles(
    const QStringList &fileNames,
    bool showProgress
    )
{
    if (fileNames.isEmpty()) {
        return;
    }

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
                // Merge tempSessionData into model
                model->mergeSessionData(tempSessionData);

                // Collect min and max time from tempSessionData
                for (const QString &sensorKey : tempSessionData.sensorKeys()) {
                    QVector<double> times = tempSessionData.getMeasurement(sensorKey, SessionKeys::Time);
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
                failedImports.insert(filePath, errorMessage);
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
                    QVector<double> times = tempSessionData.getMeasurement(sensorKey, SessionKeys::Time);
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
                failedImports.insert(filePath, errorMessage);
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
        QMessageBox::warning(this, tr("Import Completed with Some Failures"), message);
    } else if (!failedImports.isEmpty()) {
        // List all failed imports
        QStringList failedList = failedImports.keys();
        QString message = tr("Import has been completed.");
        message += tr("\nHowever, some files failed to import:");
        message += "\n" + failedList.join("\n");
        QMessageBox::warning(this, tr("Import Completed with Some Failures"), message);
    } else if (showProgress) {
        // All imports successful
        QString message = tr("Import has been completed.");
        message += tr("\nAll files have been imported successfully.");
        QMessageBox::information(this, tr("Import Completed"), message);
    }
}

void MainWindow::on_action_Delete_triggered()
{

}

void MainWindow::setupPlotValues()
{
    // Create the dock widget
    plotDock = new QDockWidget(tr("Plot Values"), this);
    plotTreeView = new QTreeView(plotDock);
    plotDock->setWidget(plotTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, plotDock);

    // Attach the model
    plotTreeView->setModel(plotModel);
    plotTreeView->setHeaderHidden(true);
    plotTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Angular spread for grouped colours
    const int group_a = 40;

    QVector<PlotValue> plotValues = {
        // Category: GNSS
        {"GNSS", "Altitude", "m", Qt::black, "GNSS", "hMSL"},
        {"GNSS", "Horizontal speed", "m/s", Qt::red, "GNSS", "velH"},
        {"GNSS", "Vertical speed", "m/s", Qt::green, "GNSS", "velD"},
        {"GNSS", "Total speed", "m/s", Qt::blue, "GNSS", "vel"},
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

        // Add more categories and plots as needed
    };

    // Populate the model with plot values and track the first checked item
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

void MainWindow::initializeCalculatedValues()
{
    SessionData::registerCalculatedValue("GNSS", "velH", [](SessionData& session) -> QVector<double> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");

        if (velN.isEmpty() || velE.isEmpty()) {
            qWarning() << "Cannot calculate total_speed due to missing velN or velE";
            return QVector<double>();
        }

        if (velN.size() != velE.size()) {
            qWarning() << "velN and velE size mismatch in session:" << session.getVar(SessionKeys::SessionId);
            return QVector<double>();
        }

        QVector<double> velH;
        velH.reserve(velN.size());
        for(int i = 0; i < velN.size(); ++i){
            velH.append(std::sqrt(velN[i]*velN[i] + velE[i]*velE[i]));
        }
        return velH;
    });

    SessionData::registerCalculatedValue("GNSS", "vel", [](SessionData& session) -> QVector<double> {
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (velH.isEmpty() || velD.isEmpty()) {
            qWarning() << "Cannot calculate total_speed due to missing velH or velD";
            return QVector<double>();
        }

        if (velH.size() != velD.size()) {
            qWarning() << "velH and velD size mismatch in session:" << session.getVar(SessionKeys::SessionId);
            return QVector<double>();
        }

        QVector<double> vel;
        vel.reserve(velH.size());
        for(int i = 0; i < velH.size(); ++i){
            vel.append(std::sqrt(velH[i]*velH[i] + velD[i]*velD[i]));
        }
        return vel;
    });

    SessionData::registerCalculatedValue("IMU", "aTotal", [](SessionData& session) -> QVector<double> {
        QVector<double> ax = session.getMeasurement("IMU", "ax");
        QVector<double> ay = session.getMeasurement("IMU", "ay");
        QVector<double> az = session.getMeasurement("IMU", "az");

        if (ax.isEmpty() || ay.isEmpty() || az.isEmpty()) {
            qWarning() << "Cannot calculate aTotal due to missing ax, ay, or az";
            return QVector<double>();
        }

        if ((ax.size() != ay.size()) || (ax.size() != az.size())) {
            qWarning() << "az, ay, or az size mismatch in session:" << session.getVar(SessionKeys::SessionId);
            return QVector<double>();
        }

        QVector<double> aTotal;
        aTotal.reserve(ax.size());
        for(int i = 0; i < ax.size(); ++i){
            aTotal.append(std::sqrt(ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]));
        }
        return aTotal;
    });

    SessionData::registerCalculatedValue("IMU", "wTotal", [](SessionData& session) -> QVector<double> {
        QVector<double> wx = session.getMeasurement("IMU", "wx");
        QVector<double> wy = session.getMeasurement("IMU", "wy");
        QVector<double> wz = session.getMeasurement("IMU", "wz");

        if (wx.isEmpty() || wy.isEmpty() || wz.isEmpty()) {
            qWarning() << "Cannot calculate wTotal due to missing wx, wy, or wz";
            return QVector<double>();
        }

        if ((wx.size() != wy.size()) || (wx.size() != wz.size())) {
            qWarning() << "wz, wy, or wz size mismatch in session:" << session.getVar(SessionKeys::SessionId);
            return QVector<double>();
        }

        QVector<double> wTotal;
        wTotal.reserve(wx.size());
        for(int i = 0; i < wx.size(); ++i){
            wTotal.append(std::sqrt(wx[i]*wx[i] + wy[i]*wy[i] + wz[i]*wz[i]));
        }
        return wTotal;
    });

    SessionData::registerCalculatedValue("MAG", "total", [](SessionData& session) -> QVector<double> {
        QVector<double> x = session.getMeasurement("MAG", "x");
        QVector<double> y = session.getMeasurement("MAG", "y");
        QVector<double> z = session.getMeasurement("MAG", "z");

        if (x.isEmpty() || y.isEmpty() || z.isEmpty()) {
            qWarning() << "Cannot calculate total due to missing x, y, or z";
            return QVector<double>();
        }

        if ((x.size() != y.size()) || (x.size() != z.size())) {
            qWarning() << "x, y, or z size mismatch in session:" << session.getVar(SessionKeys::SessionId);
            return QVector<double>();
        }

        QVector<double> total;
        total.reserve(x.size());
        for(int i = 0; i < x.size(); ++i){
            total.append(std::sqrt(x[i]*x[i] + y[i]*y[i] + z[i]*z[i]));
        }
        return total;
    });

    // Helper lambda to compute _time for non-GNSS sensors
    auto compute_time = [](SessionData &session, const QString &sensorID) -> QVector<double> {
        // If GNSS, just return the GNSS time (already UTC)
        if (sensorID == "GNSS") {
            return session.getMeasurement("GNSS", "time");
        }

        // For non-GNSS sensors, we need TIME sensor data and a linear fit
        bool haveFit = session.hasVar(SessionKeys::TimeFitA) && session.hasVar(SessionKeys::TimeFitB);
        double a = 0.0, b = 0.0;
        if (!haveFit) {
            // Attempt to compute the fit
            if (!session.hasSensor("TIME") ||
                !session.hasMeasurement("TIME", "time") ||
                !session.hasMeasurement("TIME", "tow") ||
                !session.hasMeasurement("TIME", "week")) {
                // TIME sensor not available or incomplete data
                return {};
            }

            QVector<double> systemTime = session.getMeasurement("TIME", "time");
            QVector<double> tow = session.getMeasurement("TIME", "tow");
            QVector<double> week = session.getMeasurement("TIME", "week");

            int N = std::min({systemTime.size(), tow.size(), week.size()});
            if (N < 2) {
                // Not enough points for a linear fit
                return {};
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
                return {};
            }

            a = (N * sumSU - sumS * sumU) / denom;
            b = (sumU - a * sumS) / N;

            // Store fit parameters
            session.setVar(SessionKeys::TimeFitA, QString::number(a, 'g', 17));
            session.setVar(SessionKeys::TimeFitB, QString::number(b, 'g', 17));
        } else {
            // Already computed fit
            a = session.getVar(SessionKeys::TimeFitA).toDouble();
            b = session.getVar(SessionKeys::TimeFitB).toDouble();
        }

        // Now convert the sensor's 'time' measurement using the linear fit
        if (!session.hasMeasurement(sensorID, "time")) {
            return {};
        }

        QVector<double> sensorSystemTime = session.getMeasurement(sensorID, "time");
        QVector<double> result(sensorSystemTime.size());
        for (int i = 0; i < sensorSystemTime.size(); ++i) {
            result[i] = a * sensorSystemTime[i] + b;
        }
        return result;
    };

    // Register for GNSS
    SessionData::registerCalculatedValue("GNSS", SessionKeys::Time, [](SessionData &s) {
        return s.getMeasurement("GNSS", "time");
    });

    // Register for other sensors
    QStringList sensors = {"BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : sensors) {
        SessionData::registerCalculatedValue(sens, SessionKeys::Time, [compute_time, sens](SessionData &s) {
            return compute_time(s, sens);
        });
    }
}

} // namespace FlySight
