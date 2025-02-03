#include "setgroundtool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

#include <QMouseEvent>
#include <QDebug>

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
    if (!m_plot || !m_model)
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());
    QString hoveredId = m_model->hoveredSessionId();

    if (!hoveredId.isEmpty()) {
        // compute ground for that session
        int row = m_model->getSessionRow(hoveredId);
        if (row >= 0) {
            SessionData &sd = m_model->sessionRef(row);
            double newElev = computeGroundElevation(sd, xFromExit);
            m_model->updateAttribute(hoveredId, SessionKeys::GroundElev, newElev);
        }
    } else {
        // fallback: if no single hovered session,
        // do your "multi-graph" approach if you still want that
        // ...
    }

    // revert to primary tool
    m_widget->revertToPrimaryTool();
    return true;
}

bool SetGroundTool::mouseMoveEvent(QMouseEvent *event)
{
    // We can do nothing here, because the CrosshairManager is already
    // showing single or multi tracers.
    // Or we can do extra stuff if you want.
    Q_UNUSED(event);
    return true;
}

void SetGroundTool::activateTool()
{
    PlotTool::activateTool();
    // We might do a synthetic mouse move if desired,
    // or just do nothing
}

void SetGroundTool::closeTool()
{
    // We do nothing, because the manager handles tracer removal
    PlotTool::closeTool();
}

} // namespace FlySight
