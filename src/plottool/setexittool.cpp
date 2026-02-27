#include "setexittool.h"
#include "ui/docks/plot/PlotWidget.h"
#include "../crosshairmanager.h"
#include <QCustomPlot/qcustomplot.h>

#include <QMouseEvent>
#include <QDebug>

namespace FlySight {

SetExitTool::SetExitTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_model(ctx.model)
{
}

bool SetExitTool::mousePressEvent(QMouseEvent *event)
{
    // only left-click, and only if we have a plot + model
    if (!m_plot || !m_model || event->button() != Qt::LeftButton)
        return false;

    // 1) pixel → axis coordinate (could be seconds-from-exit or epoch secs)
    double xCoord = m_plot->xAxis->pixelToCoord(event->pos().x());

    // 2) Get traced sessions from CrosshairManager via PlotWidget
    CrosshairManager* crosshairMgr = m_widget->crosshairManager();
    if (!crosshairMgr) return false; // Safety check

    QSet<QString> tracedIds = crosshairMgr->getTracedSessionIds();

    for (const QString& sessionId : tracedIds) {
        QDateTime newExit = m_widget->xCoordToUtcDateTime(xCoord, sessionId);
        if (newExit.isValid()) {
            m_model->updateAttribute(sessionId, SessionKeys::ExitTime, newExit);
        }
    }

    // 4) done — go back to your primary tool
    m_widget->revertToPrimaryTool();
    return true;
}

bool SetExitTool::mouseMoveEvent(QMouseEvent *event)
{
    // We can do nothing here, because the CrosshairManager is already
    // showing single or multi tracers.
    // Or we can do extra stuff if you want.
    Q_UNUSED(event);
    return true;
}

void SetExitTool::activateTool()
{
    PlotTool::activateTool();
    // We might do a synthetic mouse move if desired,
    // or just do nothing
}

void SetExitTool::closeTool()
{
    // We do nothing, because the manager handles tracer removal
    PlotTool::closeTool();
}

} // namespace FlySight
