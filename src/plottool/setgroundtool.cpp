#include "setgroundtool.h"
#include "mainwindow.h"               // for currentXAxisKey()
#include "../plotwidget.h"
#include "../crosshairmanager.h"
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

double SetGroundTool::computeGroundElevation(SessionData &session, double xCoord) const
{
    // 1) fetch the current time-measurement key from MainWindow
    auto *mw = qobject_cast<MainWindow*>(m_widget->parentWidget());
    const QString timeKey = mw
                                ? mw->currentXAxisKey()
                                : SessionKeys::TimeFromExit;  // safe fallback

    constexpr char sensor[] = "GNSS";
    constexpr char measH[]  = "hMSL";

    const auto times      = session.getMeasurement(sensor, timeKey);
    const auto elevations = session.getMeasurement(sensor, measH);

    const int n = times.size();
    if (n < 2 || n != elevations.size()) {
        qWarning() << "[SetGroundTool] Bad data sizes:"
                   << "times=" << n << "elevations=" << elevations.size();
        return std::numeric_limits<double>::quiet_NaN();
    }

    // 2) out-of-bounds?
    if (xCoord < times.first() || xCoord > times.last()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // 3) find insertion point
    auto it = std::lower_bound(times.constBegin(), times.constEnd(), xCoord);
    int idx = std::clamp<int>(int(it - times.constBegin()), 1, n - 1);

    // exact matches?
    if (qFuzzyCompare(xCoord, times[idx]))     return elevations[idx];
    if (qFuzzyCompare(xCoord, times[idx-1]))   return elevations[idx-1];

    // 4) linear interpolate
    double x1 = times[idx-1], x2 = times[idx];
    double y1 = elevations[idx-1], y2 = elevations[idx];
    double t  = (xCoord - x1) / (x2 - x1);

    return y1 + t * (y2 - y1);
}

bool SetGroundTool::mousePressEvent(QMouseEvent *event)
{
    // only left-button clicks, and only if plot+model exist
    if (!m_plot || !m_model || event->button() != Qt::LeftButton)
        return false;

    // 1) pixel → data coordinate
    double xCoord = m_plot->xAxis->pixelToCoord(event->pos().x());

    // 2) Get traced sessions from CrosshairManager via PlotWidget
    CrosshairManager* crosshairMgr = m_widget->crosshairManager();
    if (!crosshairMgr) return false; // Safety check

    QSet<QString> tracedIds = crosshairMgr->getTracedSessionIds();

    for (const QString& sessionId : tracedIds) {
        int row = m_model->getSessionRow(sessionId);
        if (row >= 0) {
            SessionData &session = m_model->sessionRef(row);
            double newElev = computeGroundElevation(session, xCoord);
            m_model->updateAttribute(sessionId, SessionKeys::GroundElev, newElev);
        }
    }

    // 3) clean up
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
