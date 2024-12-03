#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QFileDialog>

#include "import.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    // Initialize settings object
    m_settings = new QSettings("FlySight", "Viewer", this);

    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionImport_triggered()
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

    // Sort files from oldest to newest
    fileNames.sort();

    // Import each file
    for (const QString &fileName : fileNames) {
        FSImport::FSDataImporter importer;
        SessionData sessionData;

        if (importer.importFile(fileName, sessionData)) {
            // Access variables
            QString firmwareVersion = sessionData.getVars().value("FIRMWARE_VER");

            // Access messages (raw data)
            if (sessionData.getSensors().contains("GNSS")) {
                const auto& gnssData = sessionData["GNSS"];
                const QVector<double>& times = gnssData["time"];
                const QVector<double>& latitudes = gnssData["lat"];

                // Process data
                // ...
            }
        } else {
            // Handle import failure
        }
    }

    QString lastUsedFolder = QFileInfo(fileNames.last()).absolutePath();
    m_settings->setValue("folder", lastUsedFolder);
}

