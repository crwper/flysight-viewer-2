#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDockWidget>
#include <QHeaderView>
#include <QStandardItem>
#include <QTreeView>
#include "import.h"
#include "plotwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_settings(new QSettings("FlySight", "Viewer", this))
    , ui(new Ui::MainWindow)
    , model(new SessionModel(this))
{
    ui->setupUi(this);

    // Add logbook view
    QDockWidget *logbookDock = new QDockWidget(tr("Logbook"), this);
    QTreeView *logbookView = new QTreeView(logbookDock);
    logbookDock->setWidget(logbookView);
    addDockWidget(Qt::RightDockWidgetArea, logbookDock);

    // Add plot view
    PlotWidget *plotWidget = new PlotWidget(model, this);
    setCentralWidget(plotWidget);

    logbookView->setModel(model);
    logbookView->setRootIsDecorated(false);
    logbookView->header()->setDefaultSectionSize(100);

    // Setup color selection
    setupColorSelection();

    // Connect color selection to plot widget
    connect(this, &MainWindow::colorSelected, plotWidget, &PlotWidget::setPlotColor);
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
            FSImport::FSDataImporter importer;
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
            FSImport::FSDataImporter importer;
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

void MainWindow::setupColorSelection()
{
    // Create the dock widget
    colorDock = new QDockWidget(tr("Colors"), this);
    colorTreeView = new QTreeView(colorDock);
    colorDock->setWidget(colorTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, colorDock);

    // Create the model
    colorModel = new QStandardItemModel(this);
    colorTreeView->setModel(colorModel);
    colorTreeView->setHeaderHidden(true); // Hide the header

    // Populate the model with categories and colors
    // "Cool colours" categoryq
    QStandardItem *coolItem = new QStandardItem("Cool colours");
    coolItem->setFlags(Qt::ItemIsEnabled); // Non-checkable
    colorModel->appendRow(coolItem);

    QStandardItem *blueItem = new QStandardItem("Blue");
    blueItem->setCheckable(true);
    blueItem->setData(QColor(Qt::blue), Qt::UserRole + 1); // Store color data
    coolItem->appendRow(blueItem);

    QStandardItem *greenItem = new QStandardItem("Green");
    greenItem->setCheckable(true);
    greenItem->setData(QColor(Qt::green), Qt::UserRole + 1);
    coolItem->appendRow(greenItem);

    // "Warm colours" category
    QStandardItem *warmItem = new QStandardItem("Warm colours");
    warmItem->setFlags(Qt::ItemIsEnabled); // Non-checkable
    colorModel->appendRow(warmItem);

    QStandardItem *redItem = new QStandardItem("Red");
    redItem->setCheckable(true);
    redItem->setData(QColor(Qt::red), Qt::UserRole + 1);
    warmItem->appendRow(redItem);

    QStandardItem *orangeItem = new QStandardItem("Magenta");
    orangeItem->setCheckable(true);
    orangeItem->setData(QColor(Qt::magenta), Qt::UserRole + 1);
    warmItem->appendRow(orangeItem);

    // Optionally, set a default checked color
    blueItem->setCheckState(Qt::Checked);

    // Connect to itemChanged signal to enforce mutual exclusivity
    connect(colorModel, &QStandardItemModel::itemChanged, this, [=](QStandardItem *item){
        if (item->isCheckable() && item->checkState() == Qt::Checked) {
            // Uncheck all other items
            QList<QStandardItem*> allItems = colorModel->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
            for(auto &otherItem : allItems) {
                if(otherItem != item && otherItem->isCheckable() && otherItem->checkState() == Qt::Checked){
                    otherItem->setCheckState(Qt::Unchecked);
                }
            }
            // Emit the selected color
            QColor selectedColor = item->data(Qt::UserRole + 1).value<QColor>();
            emit colorSelected(selectedColor);
        }
    });

    // Emit the initial selected color based on the default checked item
    QList<QStandardItem*> checkedItems = colorModel->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive, Qt::CheckStateRole);
    for(auto &item : checkedItems){
        if(item->checkState() == Qt::Checked){
            QColor selectedColor = item->data(Qt::UserRole + 1).value<QColor>();
            emit colorSelected(selectedColor);
            break;
        }
    }
}
