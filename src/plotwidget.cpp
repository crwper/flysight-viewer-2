#include "plotwidget.h"
#include "mainwindow.h"

PlotWidget::PlotWidget(SessionModel *model, QStandardItemModel *plotModel,
                       CalculatedValueManager* calcManager, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
    , m_calculatedValueManager(calcManager)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // Initial plot setup
    setupPlot();

    // Connect to model changes
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);
    connect(plotModel, &QStandardItemModel::modelReset, this, &PlotWidget::updatePlot);
}

void PlotWidget::updatePlot()
{
    // Clear existing plots and graphs
    customPlot->clearPlottables();
    m_plottedGraphs.clear();

    // Remove existing custom y-axes
    QList<QCPAxis*> axesToRemove = m_plotValueAxes.values();
    m_plotValueAxes.clear();

    for(auto axis : axesToRemove){
        customPlot->axisRect()->removeAxis(axis);
    }

    // Retrieve all sessions
    const QVector<SessionData>& sessions = model->getAllSessions();

    // Iterate through all checked plot values
    for(int row = 0; row < plotModel->rowCount(); ++row){
        QStandardItem* categoryItem = plotModel->item(row);
        for(int col = 0; col < categoryItem->rowCount(); ++col){
            QStandardItem* plotItem = categoryItem->child(col);
            if(plotItem->checkState() == Qt::Checked){
                // Retrieve plot specifications
                QColor color = plotItem->data(MainWindow::DefaultColorRole).value<QColor>();
                QString sensorID = plotItem->data(MainWindow::SensorIDRole).toString();
                QString measurementID = plotItem->data(MainWindow::MeasurementIDRole).toString();
                QString plotName = plotItem->data(Qt::DisplayRole).toString();
                QString plotUnits = plotItem->data(MainWindow::PlotUnitsRole).toString();

                // Unique identifier for plot value
                QString plotValueID = sensorID + "/" + measurementID;

                // Create y-axis for this plot value if not already created
                if(!m_plotValueAxes.contains(plotValueID)){
                    // Create a new y-axis
                    QCPAxis *newYAxis = customPlot->axisRect()->addAxis(QCPAxis::atLeft);

                    // Set axis label
                    newYAxis->setLabel(plotName + " (" + plotUnits + ")");

                    // Set axis color
                    newYAxis->setLabelColor(color);
                    newYAxis->setTickLabelColor(color);
                    newYAxis->setBasePen(QPen(color));
                    newYAxis->setTickPen(QPen(color));
                    newYAxis->setSubTickPen(QPen(color));

                    // Add to the map
                    m_plotValueAxes.insert(plotValueID, newYAxis);
                }

                QCPAxis* assignedYAxis = m_plotValueAxes.value(plotValueID);

                // Iterate through all sessions to plot
                for(const auto& session : sessions){
                    // Check for session visibility
                    if (session.getVars().value("VISIBLE") != "true") {
                        continue;
                    }

                    QString sessionID = session.getVars().value("SESSION_ID");

                    // Get yData using CalculatedValueManager
                    QVector<double> yData = m_calculatedValueManager->getMeasurement(const_cast<SessionData&>(session), sensorID, measurementID);

                    if(yData.isEmpty()){
                        qWarning() << "No data available for plot:" << plotName << "in session:" << sessionID;
                        continue;
                    }

                    // Assume there is a "time" measurement for x-axis
                    QVector<double> xData = m_calculatedValueManager->getMeasurement(const_cast<SessionData&>(session), sensorID, "time");

                    if(xData.isEmpty()){
                        qWarning() << "No 'time' data available for session:" << sessionID;
                        continue;
                    }

                    if(xData.size() != yData.size()){
                        qWarning() << "Time and measurement data size mismatch for session:" << sessionID;
                        continue;
                    }

                    // Create a new graph assigned to the specific y-axis
                    QCPGraph *graph = customPlot->addGraph(customPlot->xAxis, assignedYAxis);

                    // Assign the default color
                    graph->setPen(QPen(color));

                    // Set data
                    graph->setData(xData, yData);

                    // Set line style, scatter style, etc.
                    graph->setLineStyle(QCPGraph::lsLine);
                    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

                    // Add to the list of plotted graphs
                    m_plottedGraphs.append(graph);

                    qDebug() << "Plotted session:" << sessionID << "on plot:" << plotName;
                }
            }
        }
    }

    // Rescale each y-axis to fit its data
    for(auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it){
        it.value()->rescale();
    }

    // Rescale x-axis based on all data
    customPlot->xAxis->rescale();

    // Replot to display the updated graph
    customPlot->replot();
}

void PlotWidget::setupPlot()
{
    // Basic plot setup
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setVisible(false);

    // Enable interactions if needed
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}
