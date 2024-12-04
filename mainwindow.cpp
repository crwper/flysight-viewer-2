#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "import.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QDebug>

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
        SessionData tempSessionData;

        if (importer.importFile(fileName, tempSessionData)) {
            // Merge tempSessionData into m_sessionDataList
            mergeSessionData(tempSessionData);
        } else {
            // Handle import failure (e.g., show a message to the user)
            QMessageBox::warning(this, tr("Import Failed"),
                                 tr("Failed to import file: %1").arg(fileName));
        }
    }

    // Update last used folder
    QString lastUsedFolder = QFileInfo(fileNames.last()).absolutePath();
    m_settings->setValue("folder", lastUsedFolder);
}

void MainWindow::mergeSessionData(const SessionData& newSession)
{
    // Ensure SESSION_ID exists in newSession
    if (!newSession.getVars().contains("SESSION_ID")) {
        qWarning() << "New SessionData does not contain SESSION_ID. Skipping merge.";
        return;
    }

    QString newSessionID = newSession.getVars().value("SESSION_ID");

    // Iterate through existing SessionData to find a match
    for (int i = 0; i < m_sessionDataList.size(); ++i) {
        SessionData &existingSession = m_sessionDataList[i];

        // Check if SESSION_ID matches
        if (existingSession.getVars().value("SESSION_ID") == newSessionID) {
            // Check that other variable values are the same
            QMap<QString, QString> existingVars = existingSession.getVars();
            QMap<QString, QString> newVars = newSession.getVars();

            bool varsMatch = true;
            for (auto it = newVars.constBegin(); it != newVars.constEnd(); ++it) {
                if (!existingVars.contains(it.key()) || existingVars.value(it.key()) != it.value()) {
                    varsMatch = false;
                    break;
                }
            }

            if (!varsMatch) {
                qWarning() << "Variable values do not match for SESSION_ID:" << newSessionID;
                // Optionally, handle this case (e.g., treat as separate SessionData)
                continue;
            }

            // Check for overlapping sensor/measurement key pairs
            const QMap<QString, QMap<QString, QVector<double>>> &existingSensors = existingSession.getSensors();
            const QMap<QString, QMap<QString, QVector<double>>> &newSensors = newSession.getSensors();

            bool noOverlap = true;
            for (auto sensorIt = newSensors.constBegin(); sensorIt != newSensors.constEnd(); ++sensorIt) {
                QString sensorName = sensorIt.key();
                if (existingSensors.contains(sensorName)) {
                    const QMap<QString, QVector<double>> &existingMeasurements = existingSensors.value(sensorName);
                    const QMap<QString, QVector<double>> &newMeasurements = sensorIt.value();

                    for (auto measureIt = newMeasurements.constBegin(); measureIt != newMeasurements.constEnd(); ++measureIt) {
                        QString measurementKey = measureIt.key();
                        if (existingMeasurements.contains(measurementKey)) {
                            // Overlapping sensor/measurement key found
                            noOverlap = false;
                            qWarning() << "Overlapping sensor/measurement key:" << sensorName << "/" << measurementKey
                                       << "for SESSION_ID:" << newSessionID;
                            break;
                        }
                    }
                }

                if (!noOverlap) break;
            }

            if (!noOverlap) {
                // Cannot merge due to overlapping keys
                continue;
            }

            // Merge variables (excluding SESSION_ID)
            for (auto it = newVars.constBegin(); it != newVars.constEnd(); ++it) {
                if (it.key() == "SESSION_ID") continue; // Skip SESSION_ID
                existingSession.getVars().insert(it.key(), it.value());
            }

            // Merge sensors and measurements
            for (auto sensorIt = newSensors.constBegin(); sensorIt != newSensors.constEnd(); ++sensorIt) {
                QString sensorName = sensorIt.key();
                const QMap<QString, QVector<double>> &newMeasurements = sensorIt.value();

                if (!existingSession.getSensors().contains(sensorName)) {
                    // If the sensor doesn't exist, add it entirely
                    existingSession.getSensors().insert(sensorName, newMeasurements);
                } else {
                    // Sensor exists, add new measurements
                    QMap<QString, QVector<double>> &existingMeasurements = existingSession.getSensors()[sensorName];
                    for (auto measureIt = newMeasurements.constBegin(); measureIt != newMeasurements.constEnd(); ++measureIt) {
                        existingMeasurements.insert(measureIt.key(), measureIt.value());
                    }
                }
            }

            // Merge successful
            qDebug() << "Merged SessionData with SESSION_ID:" << newSessionID;
            return;
        }
    }

    // If no matching SESSION_ID found, add as a new SessionData
    m_sessionDataList.append(newSession);
    qDebug() << "Added new SessionData with SESSION_ID:" << newSession.getVars().value("SESSION_ID");
}
