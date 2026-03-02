#include "setsynctool.h"
#include "ui/docks/plot/PlotWidget.h"
#include "../crosshairmanager.h"
#include <QCustomPlot/qcustomplot.h>

#include <QMouseEvent>

namespace FlySight {

SetSyncTool::SetSyncTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_model(ctx.model)
{
}

bool SetSyncTool::mousePressEvent(QMouseEvent *event)
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
        QDateTime newSync = m_widget->xCoordToUtcDateTime(xCoord, sessionId);
        if (newSync.isValid()) {
            m_model->updateAttribute(sessionId, SessionKeys::SyncTime, newSync);
        }
    }

    // done — go back to your primary tool
    m_widget->revertToPrimaryTool();
    return true;
}

bool SetSyncTool::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    return true;
}

void SetSyncTool::activateTool()
{
    PlotTool::activateTool();
}

void SetSyncTool::closeTool()
{
    PlotTool::closeTool();
}

} // namespace FlySight
