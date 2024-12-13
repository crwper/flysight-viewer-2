#include "plotwidget.h"
#include "mainwindow.h"

#include <algorithm>

namespace FlySight {

PlotWidget::PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // Initial plot setup
    setupPlot();

    // Connect to model changes
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);
    connect(plotModel, &QStandardItemModel::modelReset, this, &PlotWidget::updatePlot);

    // Connect xAxis range changes to the new slot
    connect(
        customPlot->xAxis,
        QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
        this,
        &PlotWidget::onXAxisRangeChanged
        );
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
                    if (!session.isVisible()) {
                        continue;
                    }

                    // Get yData directly from session
                    QVector<double> yData = const_cast<SessionData&>(session).getMeasurement(sensorID, measurementID);

                    if(yData.isEmpty()){
                        qWarning() << "No data available for plot:" << plotName << "in session:" << session.getVar(SessionKeys::SessionId);
                        continue;
                    }

                    // Assume there is a "time" measurement for x-axis
                    QVector<double> xData = const_cast<SessionData&>(session).getMeasurement(sensorID, SessionKeys::Time);

                    if(xData.isEmpty()){
                        qWarning() << "No 'time' data available for session:" << session.getVar(SessionKeys::SessionId);
                        continue;
                    }

                    if(xData.size() != yData.size()){
                        qWarning() << "Time and measurement data size mismatch for session:" << session.getVar(SessionKeys::SessionId);
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

                    qDebug() << "Plotted session:" << session.getVar(SessionKeys::SessionId) << "on plot:" << plotName;
                }
            }
        }
    }

    // Replot to display the updated graph
    onXAxisRangeChanged(customPlot->xAxis->range());
}

void PlotWidget::onXAxisRangeChanged(const QCPRange &newRange)
{
    if (m_updatingYAxis)
        return; // Prevent recursion

    m_updatingYAxis = true;

    for(auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it){
        QCPAxis* yAxis = it.value();

        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();

        // Iterate through all graphs and find those assigned to this yAxis
        for(int i = 0; i < customPlot->graphCount(); ++i){
            QCPGraph* graph = customPlot->graph(i);

            if(graph->valueAxis() != yAxis){
                continue;
            }

            // Efficiently find data within the new x range
            QCPDataContainer<QCPGraphData>::const_iterator itLower = graph->data()->findBegin(newRange.lower, false);
            QCPDataContainer<QCPGraphData>::const_iterator itUpper = graph->data()->findEnd(newRange.upper, false);

            for(auto it = itLower; it != itUpper; ++it){
                double y = it->value;
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
            }

            // Interpolate at newRange.lower
            double yLower = interpolateY(graph, newRange.lower);
            if (!std::isnan(yLower)) {
                yMin = std::min(yMin, yLower);
                yMax = std::max(yMax, yLower);
            }

            // Interpolate at newRange.upper
            double yUpper = interpolateY(graph, newRange.upper);
            if (!std::isnan(yUpper)) {
                yMin = std::min(yMin, yUpper);
                yMax = std::max(yMax, yUpper);
            }
        }

        if(yMin < yMax){
            // Add 5% padding to the y-axis range for better visualization
            double padding = (yMax - yMin) * 0.05;
            if(padding == 0){
                padding = 1.0; // Fallback padding
            }

            yAxis->setRange(yMin - padding, yMax + padding);
        }
    }

    customPlot->replot();

    m_updatingYAxis = false;
}

double PlotWidget::interpolateY(const QCPGraph* graph, double x) {
    auto itLower = graph->data()->findBegin(x, false);
    if (itLower == graph->data()->constBegin() || itLower == graph->data()->constEnd()) {
        return std::numeric_limits<double>::quiet_NaN(); // Cannot interpolate
    }

    auto itPrev = itLower;
    --itPrev;

    double x1 = itPrev->key;
    double y1 = itPrev->value;
    double x2 = itLower->key;
    double y2 = itLower->value;

    if (x2 == x1) {
        return std::numeric_limits<double>::quiet_NaN(); // Avoid division by zero
    }

    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

void PlotWidget::setXAxisRange(double min, double max)
{
    // Directly set the x-axis range
    customPlot->xAxis->setRange(min, max);
}

void PlotWidget::setupPlot()
{
    // Basic plot setup
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setVisible(false);

    // Enable interactions if needed
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}

} // namespace FlySight
