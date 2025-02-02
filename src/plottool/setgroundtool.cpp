#include "setgroundtool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>
#include <limits>

namespace FlySight {

SetGroundTool::SetGroundTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_model(ctx.model)
{
    // All tracer functionality will now use m_graphTracers.
    // No session hovered initially.
    m_hoveredSessionId.clear();
}

/*!
 * \brief computeGroundElevation
 * Looks up the session’s “GNSS/hMSL” array and the matching “GNSS/TimeFromExit” array
 * to interpolate a ground elevation at the given xFromExit.
 */
double SetGroundTool::computeGroundElevation(SessionData &session, double xFromExit) const
{
    constexpr char sensorGNSS[]  = "GNSS";
    constexpr char measurementH[] = "hMSL";
    QString timeFromExitKey = SessionKeys::TimeFromExit;

    QVector<double> timeArr = session.getMeasurement(sensorGNSS, timeFromExitKey);
    QVector<double> hmslArr = session.getMeasurement(sensorGNSS, measurementH);

    if (timeArr.size() < 2 || timeArr.size() != hmslArr.size()) {
        qWarning() << "[SetGroundTool] In computeGroundElevation():"
                   << "mismatched or insufficient data in GNSS/timeFromExit and GNSS/hMSL";
        return std::numeric_limits<double>::quiet_NaN();
    }

    double minX = timeArr.first();
    double maxX = timeArr.last();
    if (xFromExit < minX || xFromExit > maxX)
        return std::numeric_limits<double>::quiet_NaN();

    for (int i = 0; i < timeArr.size() - 1; ++i) {
        double x1 = timeArr[i];
        double x2 = timeArr[i+1];
        double y1 = hmslArr[i];
        double y2 = hmslArr[i+1];

        if (xFromExit == x1)
            return y1;
        if (xFromExit == x2)
            return y2;

        if (x1 <= xFromExit && xFromExit < x2) {
            double t = (xFromExit - x1) / (x2 - x1);
            return y1 + t * (y2 - y1);
        }
    }

    return hmslArr.last();
}

bool SetGroundTool::mousePressEvent(QMouseEvent *event)
{
    if (!m_plot || !m_model) {
        qWarning() << "[SetGroundTool] Not properly initialized!";
        return false;
    }

    if (event->button() != Qt::LeftButton)
        return false;

    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());
    QSet<QString> updatedSessionIds;

    if (!m_hoveredSessionId.isEmpty()) {
        int row = m_model->getSessionRow(m_hoveredSessionId);
        if (row >= 0) {
            SessionData &sd = m_model->sessionRef(row);
            double newElevation = computeGroundElevation(sd, xFromExit);
            if (!std::isnan(newElevation)) {
                m_model->updateAttribute(m_hoveredSessionId,
                                         SessionKeys::GroundElev,
                                         newElevation);
                updatedSessionIds.insert(m_hoveredSessionId);
            }
        }
    } else {
        QList<QPair<QString, double>> pendingUpdates;
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

            if (info.sensorId != "GNSS" || info.measurementId != "z")
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
            double newGround = computeGroundElevation(session, xFromExit);
            if (std::isnan(newGround)) {
                qWarning() << "[SetGroundTool] Could not compute ground for session"
                           << info.sessionId << "at xFromExit=" << xFromExit;
                continue;
            }

            pendingUpdates.append(qMakePair(info.sessionId, newGround));
            updatedSessionIds.insert(info.sessionId);
        }

        for (const auto &update : pendingUpdates) {
            m_model->updateAttribute(update.first, SessionKeys::GroundElev, update.second);
        }
    }

    if (!updatedSessionIds.isEmpty())
        emit m_model->modelChanged();

    m_widget->revertToPrimaryTool();
    return true;
}

bool SetGroundTool::mouseMoveEvent(QMouseEvent *event)
{
    QPointF mousePos = event->position();
    double minDist = std::numeric_limits<double>::max();
    QCPGraph *closestGraph = nullptr;
    const PlotWidget::GraphInfo* closestInfo = nullptr;

    // Clear hovered session by default.
    m_hoveredSessionId.clear();

    // First pass: find the closest eligible elevation plot.
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        auto it = m_graphMap->find(graph);
        if (it == m_graphMap->end())
            continue;

        const PlotWidget::GraphInfo &info = it.value();
        if (info.sensorId != "GNSS" || info.measurementId != "z")
            continue;

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
        // Single-tracer mode: only show tracer for the closest graph.
        if (closestInfo)
            m_hoveredSessionId = closestInfo->sessionId;

        // Update tracer for the closest graph.
        QCPItemTracer* tracer = nullptr;
        if (m_graphTracers.contains(closestGraph))
            tracer = m_graphTracers[closestGraph];
        else {
            tracer = new QCPItemTracer(m_plot);
            tracer->setStyle(QCPItemTracer::tsCircle);
            tracer->setSize(6);
            m_graphTracers.insert(closestGraph, tracer);
        }
        double yPlot = PlotWidget::interpolateY(closestGraph, xPlot);
        tracer->position->setAxes(m_plot->xAxis, closestGraph->valueAxis());
        tracer->position->setCoords(xPlot, yPlot);
        tracer->setPen(closestInfo->defaultPen);
        tracer->setBrush(closestInfo->defaultPen.color());
        tracer->setVisible(true);

        // Hide any other tracers.
        for (auto it = m_graphTracers.begin(); it != m_graphTracers.end(); ++it) {
            if (it.key() != closestGraph)
                it.value()->setVisible(false);
        }
    } else {
        // Multi-tracer mode: show a tracer on every elevation plot.
        for (int i = 0; i < m_plot->graphCount(); ++i) {
            QCPGraph *graph = m_plot->graph(i);
            auto it = m_graphMap->find(graph);
            if (it == m_graphMap->end())
                continue;
            const PlotWidget::GraphInfo &info = it.value();
            if (info.sensorId != "GNSS" || info.measurementId != "z")
                continue;

            double yPlot = PlotWidget::interpolateY(graph, xPlot);
            QCPItemTracer* tracer = nullptr;
            if (m_graphTracers.contains(graph))
                tracer = m_graphTracers[graph];
            else {
                tracer = new QCPItemTracer(m_plot);
                tracer->setStyle(QCPItemTracer::tsCircle);
                tracer->setSize(6);
                m_graphTracers.insert(graph, tracer);
            }
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

} // namespace FlySight
