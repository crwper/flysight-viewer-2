#include "plotwidget.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPixmap>
#include <QBitmap>
#include <QPainter>
#include <QDebug>

#include "plottool/plottool.h"
#include "plottool/pantool.h"
#include "plottool/zoomtool.h"
#include "plottool/selecttool.h"
#include "plottool/setexittool.h"
#include "plottool/setgroundtool.h"

namespace FlySight {

// Constructor
PlotWidget::PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
    , isCursorOverPlot(false)
{
    // set up the layout with the custom plot
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // initialize the plot and crosshairs
    setupPlot();
    setupCrosshairs();

    // create the plot context for tools
    PlotContext ctx;
    ctx.widget = this;
    ctx.plot = customPlot;
    ctx.graphMap = &m_graphInfoMap;
    ctx.model = model;

    // instantiate tools for interacting with the plot
    m_panTool = std::make_unique<PanTool>(ctx);
    m_zoomTool = std::make_unique<ZoomTool>(ctx);
    m_selectTool = std::make_unique<SelectTool>(ctx);
    m_setExitTool = std::make_unique<SetExitTool>(ctx);
    m_setGroundTool = std::make_unique<SetGroundTool>(ctx);

    m_currentTool = m_panTool.get();
    m_primaryTool = Tool::Pan;

    // install an event filter to capture mouse events
    customPlot->installEventFilter(this);

    // set up cursors for crosshair functionality
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    transparentCursor = QCursor(pixmap);
    originalCursor = customPlot->cursor();

    // connect signals to slots for updates and interactions
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);
    connect(plotModel, &QStandardItemModel::modelReset, this, &PlotWidget::updatePlot);
    connect(customPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &PlotWidget::onXAxisRangeChanged);
    connect(model, &SessionModel::hoveredSessionChanged, this, &PlotWidget::onHoveredSessionChanged);

    // update the plot with initial data
    updatePlot();
}

// Public Methods
void PlotWidget::setCurrentTool(Tool tool)
{
    // Switch to the appropriate tool based on the provided enum
    switch (tool) {
    case Tool::Pan:
        m_currentTool = m_panTool.get();
        break;
    case Tool::Zoom:
        m_currentTool = m_zoomTool.get();
        break;
    case Tool::Select:
        m_currentTool = m_selectTool.get();
        break;
    case Tool::SetExit:
        m_currentTool = m_setExitTool.get();
        break;
    case Tool::SetGround:
        m_currentTool = m_setGroundTool.get();
        break;
    }

    // Update previous primary tool
    if (m_currentTool->isPrimary()) {
        m_primaryTool = tool;
    }

    // Finally, notify
    emit toolChanged(tool);
}

void PlotWidget::revertToPrimaryTool()
{
    setCurrentTool(m_primaryTool);
}

void PlotWidget::setXAxisRange(double min, double max)
{
    // directly set the x-axis range on the plot
    customPlot->xAxis->setRange(min, max);
}

void PlotWidget::handleSessionsSelected(const QList<QString> &sessionIds)
{
    // emit the sessionsSelected signal with the provided session IDs
    qDebug() << "Selected sessions:" << sessionIds;
    emit sessionsSelected(sessionIds);
}

// Slots
void PlotWidget::updatePlot()
{
    // Clear existing graphs and axes
    customPlot->clearPlottables();
    m_graphInfoMap.clear();

    QList<QCPAxis *> axesToRemove = m_plotValueAxes.values();
    m_plotValueAxes.clear();
    for (auto axis : axesToRemove) {
        customPlot->axisRect()->removeAxis(axis);
    }

    // Retrieve all session data
    const QVector<SessionData> &sessions = model->getAllSessions();
    QString hoveredSessionId = model->hoveredSessionId(); // Retrieve hovered session ID

    // Iterate over the plot model to add graphs for checked items
    for (int row = 0; row < plotModel->rowCount(); ++row) {
        QStandardItem *categoryItem = plotModel->item(row);
        for (int col = 0; col < categoryItem->rowCount(); ++col) {
            QStandardItem *plotItem = categoryItem->child(col);
            if (plotItem->checkState() == Qt::Checked) {
                // Retrieve metadata for the graph
                QColor color = plotItem->data(MainWindow::DefaultColorRole).value<QColor>();
                QString sensorID = plotItem->data(MainWindow::SensorIDRole).toString();
                QString measurementID = plotItem->data(MainWindow::MeasurementIDRole).toString();
                QString plotName = plotItem->data(Qt::DisplayRole).toString();
                QString plotUnits = plotItem->data(MainWindow::PlotUnitsRole).toString();

                QString plotValueID = sensorID + "/" + measurementID;

                // Create a new y-axis if one doesn't exist for this plot value
                if (!m_plotValueAxes.contains(plotValueID)) {
                    QCPAxis *newYAxis = customPlot->axisRect()->addAxis(QCPAxis::atLeft);
                    if (!plotUnits.isEmpty()) {
                        newYAxis->setLabel(plotName + " (" + plotUnits + ")");
                    } else {
                        newYAxis->setLabel(plotName);
                    }
                    newYAxis->setLabelColor(color);
                    newYAxis->setTickLabelColor(color);
                    newYAxis->setBasePen(QPen(color));
                    newYAxis->setTickPen(QPen(color));
                    newYAxis->setSubTickPen(QPen(color));
                    m_plotValueAxes.insert(plotValueID, newYAxis);
                }

                QCPAxis *assignedYAxis = m_plotValueAxes.value(plotValueID);

                // Add graphs for each visible session
                for (const auto &session : sessions) {
                    if (!session.isVisible()) {
                        continue;
                    }

                    QVector<double> yData = const_cast<SessionData &>(session).getMeasurement(sensorID, measurementID);
                    if (yData.isEmpty()) {
                        qWarning() << "No data available for plot:" << plotName << "in session:" << session.getAttribute(SessionKeys::SessionId);
                        continue;
                    }

                    QVector<double> xData = const_cast<SessionData &>(session).getMeasurement(sensorID, SessionKeys::TimeFromExit);
                    if (xData.isEmpty() || xData.size() != yData.size()) {
                        qWarning() << "Time and measurement data size mismatch for session:" << session.getAttribute(SessionKeys::SessionId);
                        continue;
                    }

                    GraphInfo info;
                    info.sessionId = session.getAttribute(SessionKeys::SessionId).toString();
                    info.sensorId = sensorID;
                    info.measurementId = measurementID;
                    info.defaultPen = QPen(QColor(color));

                    QCPGraph *graph = customPlot->addGraph(customPlot->xAxis, assignedYAxis);
                    graph->setPen(determineGraphPen(info, hoveredSessionId));
                    graph->setData(xData, yData);
                    graph->setLineStyle(QCPGraph::lsLine);
                    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));
                    graph->setLayer(determineGraphLayer(info, hoveredSessionId));

                    m_graphInfoMap.insert(graph, info);
                }
            }
        }
    }

    // Adjust y-axis ranges based on the updated x-axis range
    onXAxisRangeChanged(customPlot->xAxis->range());
}

void PlotWidget::onXAxisRangeChanged(const QCPRange &newRange)
{
    if (m_updatingYAxis) return;

    m_updatingYAxis = true;

    // iterate through all y-axes and adjust their ranges
    for (auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it) {
        QCPAxis* yAxis = it.value();
        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();

        for (int i = 0; i < customPlot->graphCount(); ++i) {
            QCPGraph* graph = customPlot->graph(i);
            if (graph->valueAxis() != yAxis) continue;

            auto itLower = graph->data()->findBegin(newRange.lower, false);
            auto itUpper = graph->data()->findEnd(newRange.upper, false);
            for (auto it = itLower; it != itUpper; ++it) {
                double y = it->value;
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
            }

            double yLower = interpolateY(graph, newRange.lower);
            double yUpper = interpolateY(graph, newRange.upper);
            if (!std::isnan(yLower)) {
                yMin = std::min(yMin, yLower);
                yMax = std::max(yMax, yLower);
            }
            if (!std::isnan(yUpper)) {
                yMin = std::min(yMin, yUpper);
                yMax = std::max(yMax, yUpper);
            }
        }

        if (yMin < yMax) {
            double padding = (yMax - yMin) * 0.05;
            padding = (padding == 0) ? 1.0 : padding;
            yAxis->setRange(yMin - padding, yMax + padding);
        }
    }

    if (isCursorOverPlot) {
        QPoint cursorPos = customPlot->mapFromGlobal(QCursor::pos());
        updateCrosshairs(cursorPos);
    }

    customPlot->replot();
    m_updatingYAxis = false;
}

void PlotWidget::onHoveredSessionChanged(const QString &sessionId)
{
    // update graph appearance based on the hovered session
    for (auto it = m_graphInfoMap.cbegin(); it != m_graphInfoMap.cend(); ++it) {
        QCPGraph *graph = it.key();
        const GraphInfo &info = it.value();

        graph->setLayer(determineGraphLayer(info, sessionId));
        graph->setPen(determineGraphPen(info, sessionId));
    }

    customPlot->replot();
}

// Protected Methods
bool PlotWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != customPlot || !m_currentTool) return QWidget::eventFilter(obj, event);

    // handle mouse and leave events using the current tool
    switch (event->type()) {
    case QEvent::MouseMove:
        handleCrosshairMouseMove(static_cast<QMouseEvent*>(event));
        return m_currentTool->mouseMoveEvent(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonPress:
        return m_currentTool->mousePressEvent(static_cast<QMouseEvent*>(event));
    case QEvent::MouseButtonRelease:
        return m_currentTool->mouseReleaseEvent(static_cast<QMouseEvent*>(event));
    case QEvent::Leave:
        handleCrosshairLeave(event);
        m_currentTool->leaveEvent(event);
        return false;
    default:
        return QWidget::eventFilter(obj, event);
    }
}

// Initialization

void PlotWidget::setupPlot()
{
    // configure basic plot settings
    customPlot->xAxis->setLabel("Time from exit (s)");
    customPlot->yAxis->setVisible(false);

    // enable interactions for range dragging and zooming
    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);

    // restrict range interactions to horizontal only
    customPlot->axisRect()->setRangeDrag(Qt::Horizontal);
    customPlot->axisRect()->setRangeZoom(Qt::Horizontal);

    // create a dedicated layer for highlighted graphs
    customPlot->addLayer("highlighted", customPlot->layer("main"), QCustomPlot::limAbove);
}

void PlotWidget::setupCrosshairs()
{
    // create horizontal and vertical crosshairs
    crosshairH = new QCPItemLine(customPlot);
    crosshairH->setPen(QPen(Qt::gray, 1));
    crosshairH->setVisible(false);

    crosshairV = new QCPItemLine(customPlot);
    crosshairV->setPen(QPen(Qt::gray, 1));
    crosshairV->setVisible(false);

    customPlot->replot();
}

// Crosshair Management
void PlotWidget::handleCrosshairMouseMove(QMouseEvent *mouseEvent)
{
    // determine whether the cursor is over the plot area
    bool currentlyOverPlot = isCursorOverPlotArea(mouseEvent->pos());

    if (currentlyOverPlot && !isCursorOverPlot) {
        isCursorOverPlot = true;
        enableCrosshairs(true);
    } else if (!currentlyOverPlot && isCursorOverPlot) {
        isCursorOverPlot = false;
        enableCrosshairs(false);
    }

    if (isCursorOverPlot) updateCrosshairs(mouseEvent->pos());
}

void PlotWidget::handleCrosshairLeave(QEvent *event)
{
    Q_UNUSED(event);
    // disable crosshairs when the cursor leaves the plot area
    if (isCursorOverPlot) {
        isCursorOverPlot = false;
        enableCrosshairs(false);
    }
}

void PlotWidget::enableCrosshairs(bool enable)
{
    // toggle visibility of crosshairs and update the cursor
    crosshairH->setVisible(enable);
    crosshairV->setVisible(enable);

    customPlot->setCursor(enable ? transparentCursor : originalCursor);
    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotWidget::updateCrosshairs(const QPoint &pos)
{
    // update crosshair positions based on cursor coordinates
    double x = customPlot->xAxis->pixelToCoord(pos.x());
    double y = customPlot->yAxis->pixelToCoord(pos.y());

    crosshairH->start->setCoords(customPlot->xAxis->range().lower, y);
    crosshairH->end->setCoords(customPlot->xAxis->range().upper, y);
    crosshairV->start->setCoords(x, customPlot->yAxis->range().lower);
    crosshairV->end->setCoords(x, customPlot->yAxis->range().upper);

    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

bool PlotWidget::isCursorOverPlotArea(const QPoint &pos) const
{
    // check if the cursor is within the plotting area
    return customPlot->axisRect()->rect().contains(pos);
}

// Utility Methods
double PlotWidget::interpolateY(const QCPGraph* graph, double x)
{
    // find the closest data points for interpolation
    auto itLower = graph->data()->findBegin(x, false);
    if (itLower == graph->data()->constBegin() || itLower == graph->data()->constEnd()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto itPrev = itLower;
    --itPrev;

    double x1 = itPrev->key;
    double y1 = itPrev->value;
    double x2 = itLower->key;
    double y2 = itLower->value;

    if (x2 == x1) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

QPen PlotWidget::determineGraphPen(const GraphInfo &info, const QString &hoveredSessionId) const
{
    if (hoveredSessionId.isEmpty() || info.sessionId == hoveredSessionId) {
        // Highlight the graph
        return info.defaultPen;
    } else {
        // Dim the graph
        QPen pen = info.defaultPen;
        int h, s, l;
        pen.color().getHsl(&h, &s, &l);
        pen.setColor(QColor::fromHsl(h, s, (l + 255 * 15) / 16)); // Dimmed color
        return pen;
    }
}

QString PlotWidget::determineGraphLayer(const GraphInfo &info, const QString &hoveredSessionId) const
{
    return (hoveredSessionId.isEmpty() || info.sessionId == hoveredSessionId) ? "highlighted" : "main";
}

} // namespace FlySight
