#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "import.h"
#include "plotspec.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_calculatedValueManager(new CalculatedValueManager())
{
    // Initialize settings object
    m_settings = new QSettings("FlySight", "Viewer", this);

    ui->setupUi(this);

    // Initialize calculated values
    initializeCalculatedValues();

    // Initialize plot specifications
    initializePlotSpecs();

    // Initialize the plot selection dock
    initializePlotSelectionDock();

    // Connect the search box to the filter function
    connect(ui->logbookSearchEdit, &QLineEdit::textChanged, this, &MainWindow::filterLogbookTree);

    // Connect selection changes to a slot
    connect(ui->logbookTreeWidget->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateDeleteActionState);

    // Connect the itemChanged signal to handle checkbox state changes
    connect(ui->logbookTreeWidget, &QTreeWidget::itemChanged,
            this, &MainWindow::onSessionItemChanged);

    // Connect plot selection changes
    connect(ui->plotSelectionTreeWidget, &QTreeWidget::itemClicked,
            this, &MainWindow::onPlotSelectionChanged);

    // Initialize the Delete action as disabled
    ui->actionDelete->setEnabled(false);

    // Initialize the plot
    ui->centralwidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // Enable user interactions
    ui->centralwidget->xAxis->setLabel("Time (s)");
    ui->centralwidget->legend->setVisible(false);

    // Optionally, set a default range or style
    ui->centralwidget->xAxis->setRange(0, 100); // Example range
    ui->centralwidget->yAxis->setRange(0, 1000); // Example range

    ui->centralwidget->replot();
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

    // After populating the tree, rebuild the plot
    rebuildPlot();
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

    // After removing items from the tree, rebuild the plot
    rebuildPlot();
}

void MainWindow::updateDeleteActionState(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(deselected); // Unused parameter

    // Retrieve all selected top-level items
    QList<QTreeWidgetItem*> selectedItems = ui->logbookTreeWidget->selectedItems();

    bool hasValidSelection = false;

    for (QTreeWidgetItem* item : selectedItems) {
        if (item->parent() == nullptr) { // Top-level item
            hasValidSelection = true;
            break; // At least one valid selection found
        }
    }

    // Enable Delete action if there is at least one top-level item selected
    ui->actionDelete->setEnabled(hasValidSelection);
}

void MainWindow::onSessionItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // Check if the item is a top-level session item
    if (item->parent() == nullptr) {
        // Reapply the current plot specification to update plots based on session visibility
        applyCurrentPlotSpec();

        // Replot to reflect changes
        ui->centralwidget->replot();
    }
}

void MainWindow::onPlotSelectionChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // Check if the item is a child (plot item)
    if (item->parent() != nullptr) {
        // Ensure only one plot is selected (radio button behavior)
        // Uncheck all other plot items
        for (int i = 0; i < ui->plotSelectionTreeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *categoryItem = ui->plotSelectionTreeWidget->topLevelItem(i);
            for (int j = 0; j < categoryItem->childCount(); ++j) {
                QTreeWidgetItem *plotItem = categoryItem->child(j);
                if (plotItem != item) {
                    plotItem->setCheckState(0, Qt::Unchecked);
                }
            }
        }

        // Check the selected item if not already checked
        if (item->checkState(0) != Qt::Checked) {
            item->setCheckState(0, Qt::Checked);
        }

        // Find the corresponding PlotSpec
        QString category = item->parent()->text(0);
        QString plotName = item->text(0);
        PlotSpec selectedPlot;

        bool found = false;
        for (const PlotSpec &plot : m_plotSpecs) {
            if (plot.category == category && plot.plotName == plotName) {
                selectedPlot = plot;
                found = true;
                break;
            }
        }

        if (found) {
            m_currentPlotSpec = selectedPlot;
            applyCurrentPlotSpec();
        }
    }
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
    // Block signals during import to prevent unnecessary plot updates
    ui->logbookTreeWidget->blockSignals(true);

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

        // Add checkbox to the session item
        sessionItem->setCheckState(0, Qt::Checked); // Default to checked (visible)
        sessionItem->setFlags(sessionItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        // Store SESSION_ID as data for easy access during deletion
        sessionItem->setData(0, Qt::UserRole, sessionID);

        // Add DEVICE_ID as a child
        QTreeWidgetItem *deviceItem = new QTreeWidgetItem(sessionItem);
        deviceItem->setText(0, QString("Device ID: %1").arg(deviceID));

        // Make the deviceItem non-selectable
        deviceItem->setFlags(deviceItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);

        // Add Variables as a child
        QTreeWidgetItem *varsItem = new QTreeWidgetItem(sessionItem);
        varsItem->setText(0, "Variables");
        varsItem->setExpanded(false); // Collapse by default

        // Make the varsItem non-selectable
        varsItem->setFlags(varsItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);

        // Iterate through variables
        for (auto varIt = session.getVars().constBegin(); varIt != session.getVars().constEnd(); ++varIt) {
            if (varIt.key() == "SESSION_ID" || varIt.key() == "DEVICE_ID") {
                continue; // Already displayed
            }
            QTreeWidgetItem *varItem = new QTreeWidgetItem(varsItem);
            varItem->setText(0, QString("%1: %2").arg(varIt.key(), varIt.value()));

            // Make the varItem non-selectable
            varItem->setFlags(varItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
        }

        // Add Sensors as a child
        QTreeWidgetItem *sensorsItem = new QTreeWidgetItem(sessionItem);
        sensorsItem->setText(0, "Sensors");
        sensorsItem->setExpanded(false); // Collapse by default

        // Make the sensorsItem non-selectable
        sensorsItem->setFlags(sensorsItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);

        // Iterate through sensors
        for (auto sensorIt = session.getSensors().constBegin(); sensorIt != session.getSensors().constEnd(); ++sensorIt) {
            QString sensorName = sensorIt.key();
            QTreeWidgetItem *sensorItem = new QTreeWidgetItem(sensorsItem);
            sensorItem->setText(0, sensorName);
            sensorItem->setExpanded(false); // Collapse by default

            // Make the sensorItem non-selectable
            sensorItem->setFlags(sensorItem->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);

            // Iterate through measurements
            for (auto measureIt = sensorIt.value().constBegin(); measureIt != sensorIt.value().constEnd(); ++measureIt) {
                QString measurementKey = measureIt.key();
                int dataPoints = measureIt.value().size();
                QTreeWidgetItem *measurementItem = new QTreeWidgetItem(sensorItem);
                measurementItem->setText(0, QString("%1: %2 data points").arg(measurementKey).arg(dataPoints));
            }
        }
    }

    // Resize columns to fit contents
    ui->logbookTreeWidget->resizeColumnToContents(0);

    // Unblock signals after population
    ui->logbookTreeWidget->blockSignals(false);
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

void MainWindow::rebuildPlot()
{
    // Delegate to applyCurrentPlotSpec to handle all plotting
    applyCurrentPlotSpec();
}

void MainWindow::initializePlotSpecs()
{
    m_plotSpecs = {
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
}

void MainWindow::initializePlotSelectionDock()
{
    // Initially populate the tree widget
    populatePlotSelectionTree();

    // Set the first plot as selected by default, if available
    if (!m_plotSpecs.isEmpty()) {
        const PlotSpec &defaultPlot = m_plotSpecs.first();
        // Find the corresponding tree item and set it as checked
        QList<QTreeWidgetItem*> categoryItems = ui->plotSelectionTreeWidget->findItems(defaultPlot.category, Qt::MatchExactly);
        if (!categoryItems.isEmpty()) {
            QTreeWidgetItem *categoryItem = categoryItems.first();
            for (int i = 0; i < categoryItem->childCount(); ++i) {
                QTreeWidgetItem *plotItem = categoryItem->child(i);
                if (plotItem->text(0) == defaultPlot.plotName) {
                    plotItem->setCheckState(0, Qt::Checked);
                    m_currentPlotSpec = defaultPlot;
                    applyCurrentPlotSpec();
                    break;
                }
            }
        }
    }
}

void MainWindow::populatePlotSelectionTree()
{
    ui->plotSelectionTreeWidget->clear();

    // Organize plots by category
    QMap<QString, QVector<PlotSpec>> categorizedPlots;
    for (const PlotSpec &plot : m_plotSpecs) {
        categorizedPlots[plot.category].append(plot);
    }

    // Iterate through categories and add plots
    for (auto it = categorizedPlots.constBegin(); it != categorizedPlots.constEnd(); ++it) {
        const QString &category = it.key();
        const QVector<PlotSpec> &plots = it.value();

        // Create a top-level item for the category
        QTreeWidgetItem *categoryItem = new QTreeWidgetItem(ui->plotSelectionTreeWidget);
        categoryItem->setText(0, category);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable); // Non-selectable

        // Add plots as child items with radio buttons
        for (const PlotSpec &plot : plots) {
            QTreeWidgetItem *plotItem = new QTreeWidgetItem(categoryItem);
            plotItem->setText(0, plot.plotName);
            plotItem->setFlags(plotItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            plotItem->setCheckState(0, Qt::Unchecked);
        }

        // Expand the category by default
        categoryItem->setExpanded(true);
    }

    // Resize columns to fit contents
    ui->plotSelectionTreeWidget->resizeColumnToContents(0);
}

void MainWindow::applyCurrentPlotSpec()
{
    // Clear existing plots
    ui->centralwidget->clearGraphs();
    m_plottedSessions.clear();

    qDebug() << "Applying plot spec:" << m_currentPlotSpec.plotName;

    // Update y-axis label
    QString yAxisLabel;
    if (m_currentPlotSpec.plotUnits.isEmpty()) {
        yAxisLabel = QString("%1").arg(m_currentPlotSpec.plotName);
    } else {
        yAxisLabel = QString("%1 (%2)").arg(m_currentPlotSpec.plotName, m_currentPlotSpec.plotUnits);
    }
    ui->centralwidget->yAxis->setLabel(yAxisLabel);

    // Iterate through all sessions and add plots based on the current PlotSpec and session checks
    for (int i = 0; i < ui->logbookTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *sessionItem = ui->logbookTreeWidget->topLevelItem(i);
        QString sessionID = sessionItem->data(0, Qt::UserRole).toString();
        bool isChecked = (sessionItem->checkState(0) == Qt::Checked);

        if (isChecked) {
            SessionData &session = m_sessionDataMap[sessionID];

            const QVector<double> &yData = m_calculatedValueManager->getMeasurement(session, m_currentPlotSpec.sensorID, m_currentPlotSpec.measurementID);

            if (yData.isEmpty()) {
                qWarning() << "No data available for plot:" << m_currentPlotSpec.plotName << "in session:" << sessionID;
                continue;
            }

            // Assume there is a "time" sensor for x-axis
            const QVector<double> &xData = m_calculatedValueManager->getMeasurement(session, m_currentPlotSpec.sensorID, "time");

            if (xData.isEmpty()) {
                qWarning() << "No 'time' data available for session:" << sessionID;
                continue;
            }

            if (xData.size() != yData.size()) {
                qWarning() << "Time and measurement data size mismatch for session:" << sessionID;
                continue;
            }

            // Create a new graph
            QCPGraph *graph = ui->centralwidget->addGraph();
            graph->setName(sessionID);

            // Assign the default color
            graph->setPen(QPen(m_currentPlotSpec.defaultColor));

            // Set data
            graph->setData(xData, yData);

            // Optional: Set line style, scatter style, etc.
            graph->setLineStyle(QCPGraph::lsLine);
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

            // Rescale axes to fit the new data
            ui->centralwidget->xAxis->rescale();
            ui->centralwidget->yAxis->rescale();

            // Store the graph pointer associated with this session
            m_plottedSessions.insert(sessionID, graph);

            qDebug() << "Plotted session:" << sessionID << "on plot:" << m_currentPlotSpec.plotName;
        }
    }

    // Replot to reflect changes
    ui->centralwidget->replot();
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
            qWarning() << "velN and velE size mismatch in session:" << session.getVars().value("SESSION_ID");
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
            qWarning() << "velH and velD size mismatch in session:" << session.getVars().value("SESSION_ID");
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
            qWarning() << "az, ay, or az size mismatch in session:" << session.getVars().value("SESSION_ID");
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
            qWarning() << "wz, wy, or wz size mismatch in session:" << session.getVars().value("SESSION_ID");
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
            qWarning() << "x, y, or z size mismatch in session:" << session.getVars().value("SESSION_ID");
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
