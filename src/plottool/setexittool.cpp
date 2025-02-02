#include "setexittool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>
#include <limits>

namespace FlySight {

SetExitTool::SetExitTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_model(ctx.model)
{
    // No session hovered initially.
    m_hoveredSessionId.clear();
}

/*!
 * @brief getOrCreateTracer
 * Returns the tracer associated with the provided graph, creating one if needed.
 */
QCPItemTracer* SetExitTool::getOrCreateTracer(QCPGraph* graph)
{
    if (m_graphTracers.contains(graph))
        return m_graphTracers[graph];
    QCPItemTracer* tracer = new QCPItemTracer(m_plot);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(6);
    m_graphTracers.insert(graph, tracer);
    return tracer;
}

/*!
 * @brief clearTracers
 * Hides all tracer items so that tracers from a previously active tool do not remain visible.
 */
void SetExitTool::clearTracers()
{
    for (auto tracer : m_graphTracers)
        tracer->setVisible(false);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

/*!
 * @brief computeNewExit
 * Computes the new exit time for the given session using the provided xFromExit.
 */
QDateTime SetExitTool::computeNewExit(SessionData &session, double xFromExit) const
{
    QVariant oldExitVar = session.getAttribute(SessionKeys::ExitTime);
    if (!oldExitVar.canConvert<QDateTime>()) {
        qWarning() << "[SetExitTool] Invalid old ExitTime for session"
                   << session.getAttribute(SessionKeys::SessionId);
        return QDateTime(); // invalid QDateTime
    }
    double oldExitEpoch = oldExitVar.toDateTime().toMSecsSinceEpoch() / 1000.0;
    double newExitEpoch = oldExitEpoch + xFromExit;
    return QDateTime::fromMSecsSinceEpoch(qint64(newExitEpoch * 1000.0), QTimeZone::utc());
}

bool SetExitTool::mousePressEvent(QMouseEvent *event)
{
    if (!m_plot || !m_model) {
        qWarning() << "[SetExitTool] Not properly initialized!";
        return false;
    }
    if (event->button() != Qt::LeftButton)
        return false;

    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());
    QSet<QString> updatedSessionIds;

    // If a single (hovered) session exists, update only that session…
    if (!m_hoveredSessionId.isEmpty()) {
        int row = m_model->getSessionRow(m_hoveredSessionId);
        if (row >= 0) {
            SessionData &session = m_model->sessionRef(row);
            QDateTime newExit = computeNewExit(session, xFromExit);
            if (newExit.isValid()) {
                m_model->updateAttribute(m_hoveredSessionId, SessionKeys::ExitTime, newExit);
                updatedSessionIds.insert(m_hoveredSessionId);
            }
        }
    } else {
        // …otherwise, loop over all eligible graphs.
        QList<QPair<QString, QDateTime>> pendingUpdates;
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            QCPGraph *graph = m_plot->graph(i);
            if (!graph || !graph->visible())
                continue;

            auto it = m_graphMap->find(graph);
            if (it == m_graphMap->end())
                continue;
            const PlotWidget::GraphInfo &info = it.value();

            if (updatedSessionIds.contains(info.sessionId))
                continue;

            if (graph->dataCount() < 2)
                continue;
            double minX = graph->data()->constBegin()->key;
            double maxX = (graph->data()->constEnd() - 1)->key;
            if (xFromExit < minX || xFromExit > maxX)
                continue;

            int row = m_model->getSessionRow(info.sessionId);
            if (row < 0)
                continue;
            SessionData &session = m_model->sessionRef(row);
            QDateTime newExit = computeNewExit(session, xFromExit);
            if (!newExit.isValid()) {
                qWarning() << "[SetExitTool] Could not compute exit for session"
                           << info.sessionId << "at xFromExit=" << xFromExit;
                continue;
            }
            pendingUpdates.append(qMakePair(info.sessionId, newExit));
            updatedSessionIds.insert(info.sessionId);
        }
        for (const auto &update : pendingUpdates) {
            m_model->updateAttribute(update.first, SessionKeys::ExitTime, update.second);
        }
    }

    if (!updatedSessionIds.isEmpty())
        emit m_model->modelChanged();

    // After updating exit times, revert to the primary tool.
    m_widget->revertToPrimaryTool();
    return true;
}

bool SetExitTool::mouseMoveEvent(QMouseEvent *event)
{
    QPointF mousePos = event->position();
    double minDist = std::numeric_limits<double>::max();
    QCPGraph *closestGraph = nullptr;
    const PlotWidget::GraphInfo* closestInfo = nullptr;

    // Clear hovered session by default.
    m_hoveredSessionId.clear();

    // First pass: find the closest eligible graph.
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        auto it = m_graphMap->find(graph);
        if (it == m_graphMap->end())
            continue;
        const PlotWidget::GraphInfo &info = it.value();

        if (graph->dataCount() < 2)
            continue;
        double minX = graph->data()->constBegin()->key;
        double maxX = (graph->data()->constEnd() - 1)->key;
        // (Optional: you might check that the current x coordinate is within the graph’s domain.)
        double xFromExit = m_plot->xAxis->pixelToCoord(mousePos.x());
        if (xFromExit < minX || xFromExit > maxX)
            continue;

        // Use QCustomPlot’s selectTest to compute the distance.
        double distance = graph->selectTest(mousePos, false);
        if (distance >= 0 && distance < minDist) {
            minDist = distance;
            closestGraph = graph;
            closestInfo = &it.value();
        }
    }

    const double pixelThreshold = 10.0;
    double xPlot = m_plot->xAxis->pixelToCoord(mousePos.x());

    if (closestGraph && minDist < pixelThreshold) {
        // Single-tracer mode: only show the tracer for the closest graph.
        if (closestInfo)
            m_hoveredSessionId = closestInfo->sessionId;

        QCPItemTracer* tracer = getOrCreateTracer(closestGraph);
        double yPlot = PlotWidget::interpolateY(closestGraph, xPlot);
        tracer->position->setAxes(m_plot->xAxis, closestGraph->valueAxis());
        tracer->position->setCoords(xPlot, yPlot);
        tracer->setPen(closestInfo->defaultPen);
        tracer->setBrush(closestInfo->defaultPen.color());
        tracer->setVisible(true);

        // Hide tracers for all other graphs.
        for (auto it = m_graphTracers.begin(); it != m_graphTracers.end(); ++it) {
            if (it.key() != closestGraph)
                it.value()->setVisible(false);
        }
    } else {
        // Multi-tracer mode: show a tracer on every eligible graph.
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            QCPGraph *graph = m_plot->graph(i);
            auto it = m_graphMap->find(graph);
            if (it == m_graphMap->end())
                continue;
            const PlotWidget::GraphInfo &info = it.value();
            if (graph->dataCount() < 2)
                continue;
            double yPlot = PlotWidget::interpolateY(graph, xPlot);
            QCPItemTracer* tracer = getOrCreateTracer(graph);
            tracer->position->setAxes(m_plot->xAxis, graph->valueAxis());
            tracer->position->setCoords(xPlot, yPlot);
            tracer->setPen(info.defaultPen);
            tracer->setBrush(info.defaultPen.color());
            tracer->setVisible(true);
        }
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
    return true;
}

void SetExitTool::activateTool()
{
    QPointF localPos = m_plot->mapFromGlobal(QCursor::pos());
    QWidget *topLevel = m_plot->window();
    QPointF windowPos = topLevel->mapFromGlobal(QCursor::pos());
    QPointF screenPos = QCursor::pos();
    QMouseEvent syntheticEvent(QEvent::MouseMove,
                               localPos,
                               windowPos,
                               screenPos,
                               Qt::NoButton,
                               Qt::NoButton,
                               Qt::NoModifier);
    mouseMoveEvent(&syntheticEvent);
}

void SetExitTool::closeTool()
{
    // Clear tracers so that none remain when switching tools.
    clearTracers();
}

} // namespace FlySight
