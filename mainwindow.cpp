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

    // Connect the search box to the filter function
    connect(ui->logbookSearchEdit, &QLineEdit::textChanged, this, &MainWindow::filterLogbookTree);
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
            // Merge tempSessionData into m_sessionDataMap
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

    // Populate the logbookTreeWidget after importing
    populateLogbookTreeWidget();
}

void MainWindow::on_actionDelete_triggered()
{
    // Get the selected items
    QList<QTreeWidgetItem*> selectedItems = ui->logbookTreeWidget->selectedItems();

    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select at least one session to delete."));
        return;
    }

    // To store unique SESSION_IDs to delete
    QSet<QString> sessionIDsToDelete;

    // Iterate through selected items and collect SESSION_IDs
    for (QTreeWidgetItem* item : selectedItems) {
        // Check if the item is a top-level session item
        if (item->parent() == nullptr) {
            // Retrieve SESSION_ID from item data
            QVariant data = item->data(0, Qt::UserRole);
            if (data.isValid()) {
                QString sessionID = data.toString();
                sessionIDsToDelete.insert(sessionID);
            }
        }
    }

    if (sessionIDsToDelete.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Selection"), tr("Please select top-level session items to delete."));
        return;
    }

    // Confirm deletion
    QString message;
    if (sessionIDsToDelete.size() == 1) {
        message = tr("Are you sure you want to delete the selected session?");
    } else {
        message = tr("Are you sure you want to delete the selected %1 sessions?").arg(sessionIDsToDelete.size());
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Confirm Deletion"),
        message,
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply != QMessageBox::Yes) {
        return; // User canceled deletion
    }

    // Delete sessions from m_sessionDataMap
    for (const QString &sessionID : sessionIDsToDelete) {
        if (m_sessionDataMap.contains(sessionID)) {
            m_sessionDataMap.remove(sessionID);
            qDebug() << "Deleted session with SESSION_ID:" << sessionID;
        }
    }

    // Remove the corresponding items from the tree widget
    // It's safer to iterate in reverse order to avoid indexing issues
    QList<QTreeWidgetItem*> itemsToRemove;
    for (QTreeWidgetItem* item : selectedItems) {
        if (item->parent() == nullptr) {
            itemsToRemove.append(item);
        }
    }

    for (QTreeWidgetItem* item : itemsToRemove) {
        delete item; // This removes the item from the tree
    }

    QMessageBox::information(this, tr("Deletion Successful"),
                             tr("Selected session(s) have been deleted."));
}

void MainWindow::mergeSessionData(const SessionData& newSession)
{
    // Ensure SESSION_ID exists
    if (!newSession.getVars().contains("SESSION_ID")) {
        QMessageBox::critical(this, tr("Import failed"), tr("No session ID found"));
        return;
    }

    QString newSessionID = newSession.getVars().value("SESSION_ID");

    // Check if SESSION_ID exists in the map
    if (m_sessionDataMap.contains(newSessionID)) {
        SessionData &existingSession = m_sessionDataMap[newSessionID];

        // Retrieve variables from both sessions
        QMap<QString, QString> existingVars = existingSession.getVars();
        QMap<QString, QString> newVars = newSession.getVars();

        // Check for exact match of variable sets
        bool varsMatch = false;
        if (existingVars.size() == newVars.size()) {
            varsMatch = true; // Assume match until proven otherwise
            for (auto it = newVars.constBegin(); it != newVars.constEnd(); ++it) {
                if (!existingVars.contains(it.key()) || existingVars.value(it.key()) != it.value()) {
                    varsMatch = false;
                    break;
                }
            }
        }

        if (!varsMatch) {
            qWarning() << "Variable sets do not match for SESSION_ID:" << newSessionID;
            // Assign a unique SESSION_ID by appending a suffix
            int suffix = 1;
            QString uniqueSessionID = newSessionID;
            while (m_sessionDataMap.contains(uniqueSessionID)) {
                uniqueSessionID = QString("%1_%2").arg(newSessionID).arg(suffix++);
            }
            SessionData uniqueSession = newSession;
            uniqueSession.setVar("SESSION_ID", uniqueSessionID);
            m_sessionDataMap.insert(uniqueSessionID, uniqueSession);
            qDebug() << "Added new SessionData with unique SESSION_ID:" << uniqueSessionID;
            return;
        }

        // Retrieve sensors from both sessions
        const QMap<QString, QMap<QString, QVector<double>>> &existingSensors = existingSession.getSensors();
        const QMap<QString, QMap<QString, QVector<double>>> &newSensors = newSession.getSensors();

        // Check for overlapping sensors
        bool noOverlap = true;
        for (auto sensorIt = newSensors.constBegin(); sensorIt != newSensors.constEnd(); ++sensorIt) {
            QString sensorName = sensorIt.key();
            const QMap<QString, QVector<double>> &newMeasurements = sensorIt.value();

            if (existingSensors.contains(sensorName)) {
                const QMap<QString, QVector<double>> &existingMeasurements = existingSensors.value(sensorName);

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
            qWarning() << "Cannot merge SessionData due to overlapping sensor/measurement keys for SESSION_ID:" << newSessionID;
            // Assign a unique SESSION_ID by appending a suffix
            int suffix = 1;
            QString uniqueSessionID = newSessionID;
            while (m_sessionDataMap.contains(uniqueSessionID)) {
                uniqueSessionID = QString("%1_%2").arg(newSessionID).arg(suffix++);
            }
            SessionData uniqueSession = newSession;
            uniqueSession.setVar("SESSION_ID", uniqueSessionID);
            m_sessionDataMap.insert(uniqueSessionID, uniqueSession);
            qDebug() << "Added new SessionData with unique SESSION_ID:" << uniqueSessionID;
            return;
        }

        // Merge sensors and measurements
        for (auto sensorIt = newSensors.constBegin(); sensorIt != newSensors.constEnd(); ++sensorIt) {
            QString sensorName = sensorIt.key();
            const QMap<QString, QVector<double>> &newMeasurements = sensorIt.value();

            if (!existingSession.getSensors().contains(sensorName)) {
                // Add the entire sensor if it doesn't exist
                existingSession.getSensors().insert(sensorName, newMeasurements);
            } else {
                // Merge new measurements into existing sensor
                QMap<QString, QVector<double>> &existingMeasurements = existingSession.getSensors()[sensorName];
                for (auto measureIt = newMeasurements.constBegin(); measureIt != newMeasurements.constEnd(); ++measureIt) {
                    QString measurementKey = measureIt.key();
                    const QVector<double> &data = measureIt.value();
                    existingMeasurements.insert(measurementKey, data);
                }
            }
        }

        // Log successful merge
        qDebug() << "Merged SessionData with SESSION_ID:" << newSessionID;
    } else {
        // SESSION_ID does not exist, add as new SessionData
        m_sessionDataMap.insert(newSessionID, newSession);
        qDebug() << "Added new SessionData with SESSION_ID:" << newSessionID;
    }
}

void MainWindow::populateLogbookTreeWidget()
{
    // Clear existing items
    ui->logbookTreeWidget->clear();

    // Use a const_iterator to iterate without detaching
    QMap<QString, SessionData>::const_iterator it;
    for (it = m_sessionDataMap.constBegin(); it != m_sessionDataMap.constEnd(); ++it) {
        const SessionData &session = it.value();

        // Retrieve SESSION_ID and DEVICE_ID
        QString sessionID = session.getVars().value("SESSION_ID", "Unknown Session");
        QString deviceID = session.getVars().value("DEVICE_ID", "Unknown Device");

        // Create a top-level item for the session
        QTreeWidgetItem *sessionItem = new QTreeWidgetItem(ui->logbookTreeWidget);
        sessionItem->setText(0, QString("Session ID: %1").arg(sessionID));
        sessionItem->setExpanded(true); // Expand by default

        // Store SESSION_ID as data for easy access during deletion
        sessionItem->setData(0, Qt::UserRole, sessionID);

        // Add DEVICE_ID as a child
        QTreeWidgetItem *deviceItem = new QTreeWidgetItem(sessionItem);
        deviceItem->setText(0, QString("Device ID: %1").arg(deviceID));

        // Add Variables as a child
        QTreeWidgetItem *varsItem = new QTreeWidgetItem(sessionItem);
        varsItem->setText(0, "Variables");
        varsItem->setExpanded(false); // Collapse by default

        // Iterate through variables
        for (auto varIt = session.getVars().constBegin(); varIt != session.getVars().constEnd(); ++varIt) {
            if (varIt.key() == "SESSION_ID" || varIt.key() == "DEVICE_ID") {
                continue; // Already displayed
            }
            QTreeWidgetItem *varItem = new QTreeWidgetItem(varsItem);
            varItem->setText(0, QString("%1: %2").arg(varIt.key(), varIt.value()));
        }

        // Add Sensors as a child
        QTreeWidgetItem *sensorsItem = new QTreeWidgetItem(sessionItem);
        sensorsItem->setText(0, "Sensors");
        sensorsItem->setExpanded(false); // Collapse by default

        // Iterate through sensors
        for (auto sensorIt = session.getSensors().constBegin(); sensorIt != session.getSensors().constEnd(); ++sensorIt) {
            QString sensorName = sensorIt.key();
            QTreeWidgetItem *sensorItem = new QTreeWidgetItem(sensorsItem);
            sensorItem->setText(0, sensorName);
            sensorItem->setExpanded(false); // Collapse by default

            // Iterate through measurements
            for (auto measureIt = sensorIt.value().constBegin(); measureIt != sensorIt.value().constEnd(); ++measureIt) {
                QString measurementKey = measureIt.key();
                int dataPoints = measureIt.value().size();
                QTreeWidgetItem *measurementItem = new QTreeWidgetItem(sensorItem);
                measurementItem->setText(0, QString("%1: %2 data points").arg(measurementKey).arg(dataPoints));
            }
        }
    }

    // Optionally, resize columns to fit contents
    ui->logbookTreeWidget->resizeColumnToContents(0);
}

void MainWindow::filterLogbookTree(const QString &filterText)
{
    // Convert filter text to lowercase for case-insensitive matching
    QString filter = filterText.toLower();

    // Iterate through top-level items (sessions)
    for (int i = 0; i < ui->logbookTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *sessionItem = ui->logbookTreeWidget->topLevelItem(i);
        bool sessionVisible = false;

        // Check if SESSION_ID or DEVICE_ID matches the filter
        if (sessionItem->text(0).toLower().contains(filter)) {
            sessionVisible = true;
        } else {
            // Check child items (Variables and Sensors)
            for (int j = 0; j < sessionItem->childCount(); ++j) {
                QTreeWidgetItem *childItem = sessionItem->child(j);
                if (childItem->text(0).toLower().contains(filter)) {
                    sessionVisible = true;
                    break;
                }

                // Optionally, check deeper levels (e.g., measurements)
                for (int k = 0; k < childItem->childCount(); ++k) {
                    QTreeWidgetItem *grandChildItem = childItem->child(k);
                    if (grandChildItem->text(0).toLower().contains(filter)) {
                        sessionVisible = true;
                        break;
                    }
                }

                if (sessionVisible) break;
            }
        }

        // Show or hide the session item
        sessionItem->setHidden(!sessionVisible);
    }
}
