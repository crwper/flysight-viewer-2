#ifndef PICKTIMETOOL_H
#define PICKTIMETOOL_H

#include "plottool.h"
#include "../plotwidget.h"

namespace FlySight {

/*!
 * \brief PickTimeTool: Momentary tool used to pick a UTC time from the plot.
 *
 * Step 5 introduces this tool via the existing PlotTool system. The actual pick logic
 * is added in later steps; for now, this tool primarily disables built-in plot
 * interactions while active.
 */
class PickTimeTool : public PlotTool
{
public:
    explicit PickTimeTool(const PlotWidget::PlotContext &ctx)
        : m_widget(ctx.widget)
        , m_plot(ctx.plot)
    {
    }

    bool mousePressEvent(QMouseEvent *event) override
    {
        // Step 7 implements actual time picking. For now, consume clicks in the plot area
        // so that range drag/zoom doesn't interfere while in Pick-Time mode.
        if (!m_plot) return false;

        if (event->button() == Qt::LeftButton &&
            m_plot->axisRect() &&
            m_plot->axisRect()->rect().contains(event->pos()))
        {
            return true;
        }
        return false;
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
    PlotWidget*  m_widget = nullptr;
    QCustomPlot* m_plot   = nullptr;
};

} // namespace FlySight

#endif // PICKTIMETOOL_H
