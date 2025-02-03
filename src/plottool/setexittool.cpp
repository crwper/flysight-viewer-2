#include "setexittool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

#include <QMouseEvent>
#include <QDebug>

namespace FlySight {

SetExitTool::SetExitTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_model(ctx.model)
{
}

bool SetExitTool::mousePressEvent(QMouseEvent *event)
{
    if (!m_plot || !m_model) {
        return false;
    }
    if (event->button() != Qt::LeftButton) {
        return false;
    }

    // Figure out the xFromExit from the mouse click
    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());

    // We'll do the same logic as before:
    // If the user wants to set exit time for whichever session is hovered,
    // we can check model->hoveredSessionId() now:
    QString hoveredId = m_model->hoveredSessionId();
    if (!hoveredId.isEmpty()) {
        // update that one session
        int row = m_model->getSessionRow(hoveredId);
        if (row >= 0) {
            SessionData &session = m_model->sessionRef(row);
            // do your old "computeNewExit" logic
            QVariant oldExitVar = session.getAttribute(SessionKeys::ExitTime);
            if (oldExitVar.canConvert<QDateTime>()) {
                QDateTime oldExit = oldExitVar.toDateTime();
                double oldEpoch = oldExit.toMSecsSinceEpoch() / 1000.0;
                double newEpoch = oldEpoch + xFromExit;
                QDateTime newExit = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(newEpoch * 1000.0), QTimeZone::utc());
                m_model->updateAttribute(hoveredId, SessionKeys::ExitTime, newExit);
            }
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
