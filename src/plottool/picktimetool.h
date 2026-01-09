#ifndef PICKTIMETOOL_H
#define PICKTIMETOOL_H

#include "plottool.h"
#include "../plotwidget.h"

namespace FlySight {

/*!
 * \brief PickTimeTool: Momentary tool used to pick a UTC time from the plot.
 *
 * A pick can come from:
 * - Clicking a single-marker reference bubble (non-clustered), or
 * - Clicking in the plot area while exactly one session is traced by CrosshairManager.
 */
class PickTimeTool : public PlotTool
{
public:
    explicit PickTimeTool(const PlotWidget::PlotContext &ctx)
        : m_widget(ctx.widget)
        , m_plot(ctx.plot)
        , m_model(ctx.model)
    {
    }

    bool mousePressEvent(QMouseEvent *event) override
    {
        if (!m_plot || !m_widget || !event)
            return false;

        if (event->button() != Qt::LeftButton)
            return false;

        // 1) Attempt marker bubble hit-test (works even above the axis rect).
        constexpr double bubblePickThresholdPx = 5.0;

        QCPItemText *bestBubble = nullptr;
        double bestDist = bubblePickThresholdPx + 1.0;

        const auto &lanes = m_widget->markerItemsByLane();
        for (const auto &lane : lanes) {
            for (int i = 1; i < lane.size(); i += 2) {
                if (!lane[i])
                    continue;

                QCPItemText *bubble = qobject_cast<QCPItemText *>(lane[i].data());
                if (!bubble)
                    continue;

                const double dist = bubble->selectTest(event->pos(), /*onlySelectable=*/false);
                if (dist >= 0.0 && dist <= bubblePickThresholdPx && dist < bestDist) {
                    bestDist = dist;
                    bestBubble = bubble;
                }
            }
        }

        if (bestBubble) {
            PlotWidget::MarkerBubbleMeta meta;
            if (m_widget->markerBubbleMeta(bestBubble, &meta) && meta.count == 1) {
                emit m_widget->utcTimePicked(meta.utcSeconds);
                m_widget->revertToPrimaryTool();
            }
            // Clustered bubbles (×N) are intentionally non-pickable.
            return true;
        }

        // 2) Else attempt trace pick (only inside the plot axis rect).
        if (!m_plot->axisRect() || !m_plot->axisRect()->rect().contains(event->pos()))
            return false;

        CrosshairManager* crosshairMgr = m_widget->crosshairManager();
        if (!crosshairMgr)
            return true;

        QSet<QString> tracedIds = crosshairMgr->getTracedSessionIds();
        if (tracedIds.size() != 1)
            return true;

        const QString sessionId = *tracedIds.constBegin();
        const double xPlot = m_plot->xAxis->pixelToCoord(event->pos().x());

        double utcSeconds = xPlot;
        const QString axisKey = m_widget->getXAxisKey();

        if (axisKey == SessionKeys::Time) {
            // Absolute UTC time axis
            utcSeconds = xPlot;
        } else if (axisKey == SessionKeys::TimeFromExit) {
            // Relative time-from-exit axis (must use ExitTime from the traced session)
            if (!m_model)
                return true;

            const int row = m_model->getSessionRow(sessionId);
            if (row < 0)
                return true;

            SessionData &session = m_model->sessionRef(row);

            QVariant exitVar = session.getAttribute(SessionKeys::ExitTime);
            if (!exitVar.canConvert<QDateTime>())
                return true;

            QDateTime exitDt = exitVar.toDateTime();
            if (!exitDt.isValid())
                return true;

            const double exitUtc = exitDt.toMSecsSinceEpoch() / 1000.0;
            utcSeconds = exitUtc + xPlot;
        } else {
            // Fallback: treat xPlot as absolute UTC seconds
            utcSeconds = xPlot;
        }

        emit m_widget->utcTimePicked(utcSeconds);
        m_widget->revertToPrimaryTool();
        return true;
    }

    bool mouseMoveEvent(QMouseEvent *event) override
    {
        Q_UNUSED(event);
        // Consume mouse moves to prevent built-in interactions while picking time.
        return true;
    }

    bool mouseReleaseEvent(QMouseEvent *event) override
    {
        Q_UNUSED(event);
        return true;
    }

    void activateTool() override
    {
        PlotTool::activateTool();

        if (m_widget) {
            CrosshairManager* crosshairMgr = m_widget->crosshairManager();
            if (crosshairMgr)
                crosshairMgr->setMultiTraceEnabled(false);
        }
    }

    void closeTool() override
    {
        if (m_widget) {
            CrosshairManager* crosshairMgr = m_widget->crosshairManager();
            if (crosshairMgr)
                crosshairMgr->setMultiTraceEnabled(true);
        }

        PlotTool::closeTool();
    }

    bool isPrimary() override { return false; }

private:
    PlotWidget*   m_widget = nullptr;
    QCustomPlot*  m_plot   = nullptr;
    SessionModel* m_model  = nullptr;
};

} // namespace FlySight

#endif // PICKTIMETOOL_H
