#include "setexittool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"
#include <QMouseEvent>
#include <QDebug>
#include <QDateTime>

namespace FlySight {

SetExitTool::SetExitTool(const PlotWidget::PlotContext &ctx)
    : m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_model(ctx.model)
{
}

bool SetExitTool::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return false;

    double xFromExit = m_plot->xAxis->pixelToCoord(event->pos().x());
    QSet<QString> updatedSessionIds;

    // For each graph:
    for (int i = 0; i < m_plot->graphCount(); ++i) {
        QCPGraph *graph = m_plot->graph(i);
        if (!graph || !graph->visible())
            continue;

        // GraphInfo lookup:
        auto infoIt = m_graphMap->find(graph);
        if (infoIt == m_graphMap->end())
            continue;
        const PlotWidget::GraphInfo &info = infoIt.value();

        // Don’t re-update the same session:
        if (updatedSessionIds.contains(info.sessionId))
            continue;

        // Check if xFromExit is within domain
        if (graph->dataCount() < 2)
            continue;
        double minX = graph->data()->constBegin()->key;
        double maxX = (graph->data()->constEnd()-1)->key; // last element
        if (xFromExit < minX || xFromExit > maxX)
            continue;

        // Find the session row:
        int row = m_model->getSessionRow(info.sessionId);
        if (row < 0)
            continue;

        // Access the session *by reference* via a public method:
        SessionData &session = m_model->sessionRef(row);

        // Old exit time:
        QVariant oldExitVar = session.getAttribute(SessionKeys::ExitTime);
        if (!oldExitVar.canConvert<QDateTime>()) {
            qWarning() << "[SetExitTool] Invalid old ExitTime for session" << info.sessionId;
            continue;
        }
        double oldExitEpoch = oldExitVar.toDateTime().toSecsSinceEpoch();

        // The new exit is old exit + xFromExit
        double newExitEpoch = oldExitEpoch + xFromExit;
        QDateTime newExit = QDateTime::fromSecsSinceEpoch(qint64(newExitEpoch), QTimeZone::utc());

        session.setAttribute(SessionKeys::ExitTime, newExit);
        updatedSessionIds.insert(info.sessionId);

        qDebug() << "[SetExitTool] Session:" << info.sessionId
                 << " ExitTime ->" << newExit
                 << "(clicked xFromExit=" << xFromExit << ")";
    }

    if (!updatedSessionIds.isEmpty()) {
        emit m_model->modelChanged();
    }

    return true;
}

} // namespace FlySight
