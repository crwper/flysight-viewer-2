#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDockWidget>
#include <QHeaderView>
#include <QStandardItem>
#include <QTreeView>
#include "import.h"
#include "plotwidget.h"

namespace FlySight {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(new QSettings("FlySight", "Viewer", this))
    , ui(new Ui::MainWindow)
    , model(new SessionModel(this))
    , plotModel (new QStandardItemModel(this))
    , m_calculatedValueManager(new CalculatedValueManager())
{
    ui->setupUi(this);

    // Initialize calculated values
    initializeCalculatedValues();

    // Add logbook view
    QDockWidget *logbookDock = new QDockWidget(tr("Logbook"), this);
    QTreeView *logbookView = new QTreeView(logbookDock);
    logbookDock->setWidget(logbookView);
    addDockWidget(Qt::RightDockWidgetArea, logbookDock);

    logbookView->setModel(model);
    logbookView->setRootIsDecorated(false);
    logbookView->header()->setDefaultSectionSize(100);

    // Add plot widget
    PlotWidget *plotWidget = new PlotWidget(model, plotModel, m_calculatedValueManager, this);
    setCentralWidget(plotWidget);

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
    // Prompt the user to select a folder
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        tr("Select Folder to Import"),
        m_settings->value("folder").toString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

    // If the user cancels the dialog, exit the function
    if (folderPath.isEmpty()) {
        return;
    }

    // Define file filters (adjust according to your file types)
    QStringList nameFilters;
    nameFilters << "*.csv" << "*.CSV"; // Example: CSV files

    // Use QDirIterator to iterate through the folder and its subdirectories
    QDirIterator it(folderPath, nameFilters, QDir::Files, QDirIterator::Subdirectories);

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
    importFiles(filesToImport, true);

    // Update last used folder
    m_settings->setValue("folder", folderPath);
}

// Helper Function Implementation
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
            } else {
                // Failed import with an error message
                QString errorMessage = importer.getLastError();
                qWarning() << "Failed to import file:" << filePath << "Error:" << errorMessage;
                failedImports.insert(filePath, errorMessage);
            }
        }
    }

    // Display completion message
    if (failedImports.size() > 10) {
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
    plotTreeView->setHeaderHidden(true); // Hide the header

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
        {"IMU", "Acceleration X", "g", QColor::fromHsv(330, 255, 255, 128), "IMU", "ax"},
        {"IMU", "Acceleration Y", "g", QColor::fromHsv(0, 255, 255, 128), "IMU", "ay"},
        {"IMU", "Acceleration Z", "g", QColor::fromHsv(30, 255, 255, 128), "IMU", "az"},
        {"IMU", "Total acceleration", "g", QColor::fromHsv(0, 255, 255), "IMU", "aTotal"},

        {"IMU", "Rotation X", "deg/s", QColor::fromHsv(90, 255, 255, 128), "IMU", "wx"},
        {"IMU", "Rotation Y", "deg/s", QColor::fromHsv(120, 255, 255, 128), "IMU", "wy"},
        {"IMU", "Rotation Z", "deg/s", QColor::fromHsv(150, 255, 255, 128), "IMU", "wz"},
        {"IMU", "Total rotation", "deg/s", QColor::fromHsv(120, 255, 255), "IMU", "wTotal"},

        {"IMU", "Temperature", "°C", QColor::fromHsv(0, 255, 255, 128), "IMU", "temperature"},

        // Category: Magnetometer
        {"Magnetometer", "Magnetic field X", "gauss", QColor::fromHsv(210, 255, 255, 128), "MAG", "x"},
        {"Magnetometer", "Magnetic field Y", "gauss", QColor::fromHsv(240, 255, 255, 128), "MAG", "y"},
        {"Magnetometer", "Magnetic field Z", "gauss", QColor::fromHsv(270, 255, 255, 128), "MAG", "z"},
        {"Magnetometer", "Total magnetic field", "gauss", QColor::fromHsv(240, 255, 255), "MAG", "total"},

        {"Magnetometer", "Temperature", "°C", QColor::fromHsv(90, 255, 255, 128), "MAG", "temperature"},

        // Category: Barometer
        {"Barometer", "Air pressure", "Pa", QColor::fromHsv(0, 0, 64, 255), "BARO", "pressure"},
        {"Barometer", "Temperature", "°C", QColor::fromHsv(180, 255, 255, 128), "BARO", "temperature"},

        // Category: Humidity
        {"Humidity", "Humidity", "%%", QColor::fromHsv(0, 0, 128, 255), "HUM", "humidity"},
        {"Humidity", "Temperature", "°C", QColor::fromHsv(270, 255, 255, 128), "HUM", "temperature"},

        // Category: Battery
        {"Battery", "Battery voltage", "V", QColor::fromHsv(300, 255, 255, 255), "VBAT", "voltage"},

        // Category: GNSS time
        {"GNSS time", "Time of week", "s", QColor::fromHsv(0, 0, 64, 255), "TIME", "tow"},
        {"GNSS time", "Week number", "", QColor::fromHsv(0, 0, 128, 255), "TIME", "week"},

        // Add more categories and plots as needed
    };

    // Populate the model with plot values and track the first checked item
    populatePlotModel(plotModel, plotValues);

    // Connect to itemChanged signal to handle plot selection
    connect(plotModel, &QStandardItemModel::itemChanged, this, [=](QStandardItem *item){
        if (item->isCheckable()) {
            // Emit the modelChanged signal to trigger plot updates
            emit model->modelChanged();
        }
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
    m_calculatedValueManager->registerCalculatedValue("GNSS", "velH", [](SessionData& session, CalculatedValueManager& calcManager) -> QVector<double> {
        QVector<double> velN = calcManager.getMeasurement(session, "GNSS", "velN");
        QVector<double> velE = calcManager.getMeasurement(session, "GNSS", "velE");

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

    m_calculatedValueManager->registerCalculatedValue("GNSS", "vel", [](SessionData& session, CalculatedValueManager& calcManager) -> QVector<double> {
        QVector<double> velH = calcManager.getMeasurement(session, "GNSS", "velH");
        QVector<double> velD = calcManager.getMeasurement(session, "GNSS", "velD");

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

    m_calculatedValueManager->registerCalculatedValue("IMU", "aTotal", [](SessionData& session, CalculatedValueManager& calcManager) -> QVector<double> {
        QVector<double> ax = calcManager.getMeasurement(session, "IMU", "ax");
        QVector<double> ay = calcManager.getMeasurement(session, "IMU", "ay");
        QVector<double> az = calcManager.getMeasurement(session, "IMU", "az");

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

    m_calculatedValueManager->registerCalculatedValue("IMU", "wTotal", [](SessionData& session, CalculatedValueManager& calcManager) -> QVector<double> {
        QVector<double> wx = calcManager.getMeasurement(session, "IMU", "wx");
        QVector<double> wy = calcManager.getMeasurement(session, "IMU", "wy");
        QVector<double> wz = calcManager.getMeasurement(session, "IMU", "wz");

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

    m_calculatedValueManager->registerCalculatedValue("MAG", "total", [](SessionData& session, CalculatedValueManager& calcManager) -> QVector<double> {
        QVector<double> x = calcManager.getMeasurement(session, "MAG", "x");
        QVector<double> y = calcManager.getMeasurement(session, "MAG", "y");
        QVector<double> z = calcManager.getMeasurement(session, "MAG", "z");

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

    // You can register more calculated values here following the same pattern
}

} // namespace FlySight
