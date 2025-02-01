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
}

/*!
 * \brief computeGroundElevation
 * Looks up the session’s “GNSS/hMSL” array and the matching “GNSS/TimeFromExit” array
 * to interpolate a ground elevation at the given xFromExit.
 */
double SetGroundTool::computeGroundElevation(SessionData &session, double xFromExit) const
{
    // We'll retrieve the two vectors:
    //   timeArr  = GNSS / _time_from_exit
    //   hmslArr  = GNSS / hMSL
    //
    // Then do a straightforward linear search to find xFromExit within timeArr,
    // and interpolate hMSL accordingly.

    constexpr char sensorGNSS[]  = "GNSS";
    constexpr char measurementH[] = "hMSL";  // or "hMSL" if that’s your naming
    QString timeFromExitKey = SessionKeys::TimeFromExit; // "_time_from_exit"

    QVector<double> timeArr  = session.getMeasurement(sensorGNSS, timeFromExitKey);
    QVector<double> hmslArr  = session.getMeasurement(sensorGNSS, measurementH);

    if (timeArr.size() < 2 || timeArr.size() != hmslArr.size()) {
        qWarning() << "[SetGroundTool] In computeGroundElevation():"
                   << "mismatched or insufficient data in GNSS/timeFromExit and GNSS/hMSL";
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Quick check of bounds:
    double minX = timeArr.first();
    double maxX = timeArr.last();
    if (xFromExit < minX || xFromExit > maxX) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Linear search for xFromExit in [timeArr[i], timeArr[i+1]]:
    for (int i = 0; i < timeArr.size() - 1; ++i) {
        double x1 = timeArr[i];
        double x2 = timeArr[i+1];
        double y1 = hmslArr[i];
        double y2 = hmslArr[i+1];

        // if xFromExit exactly equals x1 or x2, easy answer:
        if (xFromExit == x1) return y1;
        if (xFromExit == x2) return y2;

        // check if x1 <= x < x2
        if (x1 <= xFromExit && xFromExit < x2) {
            double t = (xFromExit - x1)/(x2 - x1);
            return y1 + t*(y2 - y1);
        }
    }

    // If we exit the loop, it means xFromExit == timeArr.last() or fell out of range
    // But we already checked the last element above, so:
    return hmslArr.last();  // or NaN, depending on your preference
}

bool SetGroundTool::mousePressEvent(QMouseEvent *event)
{
    if (!m_plot || !m_model) {
        qWarning() << "[SetGroundTool] Not properly initialized!";
        return false;
    }

    if (event->button() != Qt::LeftButton)
        return false;

    // xFromExit is the "time from exit" coordinate at the crosshair
    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());
    QSet<QString> updatedSessionIds;

    // We loop over all *visible* graphs and check if xFromExit is in their domain.
    // If so, we do the ground elevation interpolation for that session
    // (unless we already updated it).

    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        if (!graph || !graph->visible())
            continue;

        // Look up the session ID from the graph
        auto it = m_graphMap->find(graph);
        if (it == m_graphMap->end())
            continue;
        const PlotWidget::GraphInfo &info = it.value();

        // Already updated ground for this session?
        if (updatedSessionIds.contains(info.sessionId))
            continue;

        // Check domain
        if (graph->dataCount() < 2)
            continue;
        double minX = graph->data()->constBegin()->key;
        double maxX = (graph->data()->constEnd()-1)->key;
        if (xFromExit < minX || xFromExit > maxX)
            continue;

        // So we have at least one plotted measurement that intersects with xFromExit
        int row = m_model->getSessionRow(info.sessionId);
        if (row < 0)
            continue;

        // Interpolate ground from the "GNSS/hMSL" vs "GNSS/timeFromExit":
        SessionData &session = m_model->sessionRef(row);
        double newGround = computeGroundElevation(session, xFromExit);

        if (std::isnan(newGround)) {
            qWarning() << "[SetGroundTool] Could not compute ground for session"
                       << info.sessionId << "at xFromExit=" << xFromExit;
            continue;
        }

        // Update the session's ground elevation
        session.setAttribute(SessionKeys::GroundElev, newGround);
        updatedSessionIds.insert(info.sessionId);

        qDebug() << "[SetGroundTool]" << info.sessionId
                 << "GroundElev ->" << newGround
                 << "(clicked xFromExit=" << xFromExit << ")";
    }

    if (!updatedSessionIds.isEmpty()) {
        emit m_model->modelChanged();
    }

    // After setting ground elevation, revert to the primary tool
    m_widget->revertToPrimaryTool();

    return true;
}

} // namespace FlySight
