#include "setgroundtool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>
#include <limits>

// Anonymous namespace for helper functions local to this file
namespace {
    /**
     * @brief Checks whether the x-coordinate (x) falls within the domain of the graph.
     *
     * @param graph The QCPGraph to check.
     * @param x The x-coordinate (in plot units).
     * @return true if x is within the range of the graph's data; false otherwise.
     */
    bool isXWithinGraphDomain(QCPGraph* graph, double x) {
        if (graph->dataCount() < 2)
            return false;
        double minX = graph->data()->constBegin()->key;
        double maxX = (graph->data()->constEnd() - 1)->key;
        return (x >= minX && x <= maxX);
    }
}

namespace FlySight {

SetGroundTool::SetGroundTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_model(ctx.model)
{
    // Create a small circular tracer on the plot
    m_tracer = new QCPItemTracer(ctx.plot);
    m_tracer->setStyle(QCPItemTracer::tsCircle);
    m_tracer->setSize(6);
    m_tracer->setVisible(false);

    // No session hovered initially
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
    constexpr char measurementH[] = "hMSL";  // or "hMSL" if that’s your naming
    QString timeFromExitKey = SessionKeys::TimeFromExit; // "_time_from_exit"

    QVector<double> timeArr = session.getMeasurement(sensorGNSS, timeFromExitKey);
    QVector<double> hmslArr = session.getMeasurement(sensorGNSS, measurementH);

    if (timeArr.size() < 2 || timeArr.size() != hmslArr.size()) {
        qWarning() << "[SetGroundTool] In computeGroundElevation():"
                   << "mismatched or insufficient data in GNSS/timeFromExit and GNSS/hMSL";
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Quick bounds check:
    double minX = timeArr.first();
    double maxX = timeArr.last();
    if (xFromExit < minX || xFromExit > maxX) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Linear search to interpolate the elevation:
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

    // Fallback: return the last value (should not occur if bounds checked above)
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

    // Convert the mouse x-coordinate to plot coordinates ("time from exit")
    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());
    QSet<QString> updatedSessionIds;

    // If a session is hovered, update that session's ground elevation.
    if (!m_hoveredSessionId.isEmpty()) {
        int row = m_model->getSessionRow(m_hoveredSessionId);
        if (row >= 0) {
            SessionData &sd = m_model->sessionRef(row);
            double newElevation = computeGroundElevation(sd, xFromExit);
            if (!std::isnan(newElevation)) {
                m_model->updateAttribute(m_hoveredSessionId, SessionKeys::GroundElev, newElevation);
                updatedSessionIds.insert(m_hoveredSessionId);
            }
        }
    }
    else {
        // Loop over all visible graphs and update those sessions whose graphs contain xFromExit.
        QList<QPair<QString, double>> pendingUpdates;

        for (int i = 0; i < m_plot->graphCount(); ++i) {
            QCPGraph *graph = m_plot->graph(i);
            if (!graph || !graph->visible())
                continue;

            auto it = m_graphMap->find(graph);
            if (it == m_graphMap->end())
                continue;
            const PlotWidget::GraphInfo &info = it.value();

            // Skip if already updated
            if (updatedSessionIds.contains(info.sessionId))
                continue;

            // Only consider elevation plots
            if (info.sensorId != "GNSS" || info.measurementId != "z")
                continue;

            if (!isXWithinGraphDomain(graph, xFromExit))
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

        // Apply pending updates
        for (const auto &update : pendingUpdates) {
            m_model->updateAttribute(update.first, SessionKeys::GroundElev, update.second);
        }
    }

    // Emit modelChanged if any updates occurred.
    if (!updatedSessionIds.isEmpty()) {
        emit m_model->modelChanged();
    }

    // After updating the ground elevation, revert to the primary tool.
    m_widget->revertToPrimaryTool();

    return true;
}

bool SetGroundTool::mouseMoveEvent(QMouseEvent *event)
{
    // Convert the mouse position to plot coordinates.
    QPointF mousePos = event->position();
    double minDist = std::numeric_limits<double>::max();
    QCPGraph *closestGraph = nullptr;
    const PlotWidget::GraphInfo* closestInfo = nullptr;

    // Clear any previously hovered session.
    m_hoveredSessionId.clear();

    // Find the closest eligible graph.
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        auto it = m_graphMap->find(graph);
        if (it == m_graphMap->end())
            continue;

        const PlotWidget::GraphInfo &info = it.value();

        // Consider only elevation plots.
        if (info.sensorId != "GNSS" || info.measurementId != "z")
            continue;

        double distance = graph->selectTest(mousePos, false);
        if (distance >= 0 && distance < minDist) {
            minDist = distance;
            closestGraph = graph;
            closestInfo = &it.value();
        }
    }

    const double pixelThreshold = 10.0;  // Threshold in pixels to consider "close"
    if (closestGraph && minDist < pixelThreshold && closestInfo) {
        // Set the hovered session.
        m_hoveredSessionId = closestInfo->sessionId;

        // Update tracer appearance.
        QPen tracerPen = closestInfo->defaultPen;
        m_tracer->setPen(tracerPen);
        m_tracer->setBrush(tracerPen.color());

        // Determine the tracer's position based on the mouse x-coordinate.
        double xPlot = m_plot->xAxis->pixelToCoord(mousePos.x());
        double yPlot = PlotWidget::interpolateY(closestGraph, xPlot);

        // Configure the tracer's axes and position.
        m_tracer->position->setAxes(m_plot->xAxis, closestGraph->valueAxis());
        m_tracer->position->setCoords(xPlot, yPlot);
        m_tracer->setVisible(true);
    } else {
        m_tracer->setVisible(false);
    }

    // Queue a replot to update the display.
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    return true;
}

} // namespace FlySight
