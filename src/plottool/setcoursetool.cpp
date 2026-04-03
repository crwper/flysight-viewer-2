#include "setcoursetool.h"
#include "ui/docks/plot/PlotWidget.h"
#include "../crosshairmanager.h"
#include <QCustomPlot/qcustomplot.h>

#include <QMouseEvent>

namespace FlySight {

SetCourseTool::SetCourseTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_model(ctx.model)
{
}

bool SetCourseTool::mousePressEvent(QMouseEvent *event)
{
    // only left-click, and only if we have a plot + model
    if (!m_plot || !m_model || event->button() != Qt::LeftButton)
        return false;

    // 1) pixel -> axis coordinate
    double xCoord = m_plot->xAxis->pixelToCoord(event->pos().x());

    // 2) Get traced sessions from CrosshairManager via PlotWidget
    CrosshairManager* crosshairMgr = m_widget->crosshairManager();
    if (!crosshairMgr) return false;

    QSet<QString> tracedIds = crosshairMgr->getTracedSessionIds();

    for (const QString& sessionId : tracedIds) {
        double utcSec = m_widget->xCoordToUtcSeconds(xCoord, sessionId);
        m_model->updateAttribute(sessionId, SessionKeys::CourseRef, utcSec);
    }

    // 3) done — go back to primary tool
    m_widget->revertToPrimaryTool();
    return true;
}

bool SetCourseTool::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    return true;
}

void SetCourseTool::activateTool()
{
    PlotTool::activateTool();
}

void SetCourseTool::closeTool()
{
    PlotTool::closeTool();
}

} // namespace FlySight
