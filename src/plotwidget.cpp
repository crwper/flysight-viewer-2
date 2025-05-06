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
{
    // set up the layout with the custom plot
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // initialize the plot and crosshairs
    setupPlot();

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

    // create crosshair manager
    m_crosshairManager = std::make_unique<CrosshairManager>(
        customPlot,
        model,
        &m_graphInfoMap, // existing QMap<QCPGraph*, GraphInfo>
        this
        );

    // For example, installing event filter:
    customPlot->installEventFilter(this);


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
    // Close the old tool
    if (m_currentTool)
        m_currentTool->closeTool();

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

    // Activate the new tool
    if (m_currentTool)
        m_currentTool->activateTool();

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

CrosshairManager* PlotWidget::crosshairManager() const
{
    return m_crosshairManager.get();
}

// Slots
void PlotWidget::updatePlot()
{
    // Clear existing graphs and axes
    customPlot->clearPlottables();
    m_graphInfoMap.clear();

    const QList<QCPAxis *> axesToRemove = m_plotValueAxes.values();
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

                    QVector<double> xData = const_cast<SessionData &>(session).getMeasurement(sensorID, m_xAxisKey);
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

    customPlot->replot();

    if (m_xAxisKey == SessionKeys::Time)
        updateXAxisTicker();      // update format when span changes

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
    if (obj == customPlot && m_currentTool) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto me = static_cast<QMouseEvent*>(event);
            // forward to crosshairManager if desired:
            if (m_crosshairManager) {
                m_crosshairManager->handleMouseMove(me->pos());
            }
            // also forward to the current tool
            return m_currentTool->mouseMoveEvent(me);
        }
        case QEvent::MouseButtonPress: {
            auto me = static_cast<QMouseEvent*>(event);
            return m_currentTool->mousePressEvent(me);
        }
        case QEvent::MouseButtonRelease: {
            auto me = static_cast<QMouseEvent*>(event);
            return m_currentTool->mouseReleaseEvent(me);
        }
        case QEvent::Leave:
            if (m_crosshairManager) {
                m_crosshairManager->handleMouseLeave();
            }
            m_currentTool->leaveEvent(event);
            return false;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
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

void PlotWidget::updateXAxisTicker()
{
    if (m_xAxisKey == SessionKeys::Time) {
        // Absolute UTC time → human‑readable
        auto dtTicker = QSharedPointer<QCPAxisTickerDateTime>::create();
        dtTicker->setDateTimeSpec(Qt::UTC);

        // Choose format based on current visible span (seconds)
        double span = customPlot->xAxis->range().size();
        if (span < 30)                   // < 30 s
            dtTicker->setDateTimeFormat("HH:mm:ss.z");
        else if (span < 3600)            // < 1 h
            dtTicker->setDateTimeFormat("HH:mm:ss");
        else if (span < 86400)           // < 1 day
            dtTicker->setDateTimeFormat("HH:mm");
        else                             // ≥ 1 day
            dtTicker->setDateTimeFormat("yyyy‑MM‑dd\nHH:mm");

        customPlot->xAxis->setTicker(dtTicker);
    } else {
        // Relative seconds → default numeric ticker
        customPlot->xAxis->setTicker(QSharedPointer<QCPAxisTicker>::create());
    }
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
    // Always use the default pen
    return info.defaultPen;
}

QString PlotWidget::determineGraphLayer(const GraphInfo &info, const QString &hoveredSessionId) const
{
    // Always use the default layer
    return "highlighted";
}

// View management
const SessionData* PlotWidget::referenceSession() const
{
    // 1. hovered?
    QString hovered = model->hoveredSessionId();
    if (!hovered.isEmpty()) {
        for (const auto& s : model->getAllSessions())
            if (s.getAttribute(SessionKeys::SessionId).toString() == hovered)
                return &s;
    }
    // 2. first visible
    for (const auto& s : model->getAllSessions())
        if (s.isVisible()) return &s;

    return nullptr;           // should not happen
}

double PlotWidget::exitTimeSeconds(const SessionData& s)
{
    QVariant v = s.getAttribute(SessionKeys::ExitTime);
    if (!v.canConvert<QDateTime>()) return 0.0;
    return v.toDateTime().toMSecsSinceEpoch() / 1000.0;
}

QCPRange PlotWidget::keyRangeOf(const SessionData& s,
                                const QString& sensor,
                                const QString& meas) const
{
    QVector<double> v =
        const_cast<SessionData&>(s).getMeasurement(sensor, meas);
    if (v.isEmpty()) return QCPRange(0,0);

    auto [minIt,maxIt] = std::minmax_element(v.begin(), v.end());
    return QCPRange(*minIt, *maxIt);
}

void PlotWidget::setXAxisKey(const QString& key, const QString& label)
{
    if (key == m_xAxisKey)           // already on this scale
        return;

    /* 1. keep a copy of the current window (old scale) */
    QCPRange oldRange = customPlot->xAxis->range();

    /* 2. locate a reference session */
    const SessionData* ref = referenceSession();
    double exitT = ref ? exitTimeSeconds(*ref) : 0.0;

    /* 3. translate the window */
    QCPRange newRange;
    if (key == SessionKeys::Time) {                  // going relative → absolute
        newRange.lower = oldRange.lower + exitT;
        newRange.upper = oldRange.upper + exitT;
    } else {                                         // absolute → relative
        newRange.lower = oldRange.lower - exitT;
        newRange.upper = oldRange.upper - exitT;
    }

    /* 4. commit the switch */
    m_xAxisKey   = key;
    m_xAxisLabel = label;
    customPlot->xAxis->setLabel(label);
    customPlot->xAxis->setRange(newRange);

    updatePlot();                 // rebuild graphs with new x‑values
    customPlot->replot();

    updateXAxisTicker();
}

} // namespace FlySight
