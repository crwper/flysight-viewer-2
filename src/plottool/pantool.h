#ifndef PANTOOL_H
#define PANTOOL_H

#include "plottool.h"
#include "../plotwidget.h"

class QCustomPlot;

namespace FlySight {

class PanTool : public PlotTool
{
public:
    explicit PanTool(const PlotWidget::PlotContext &ctx)
        : m_plot(ctx.plot)
    {
        // Make sure QCP::iRangeDrag is enabled on plot if you want built-in panning
        // or handle it manually yourself.
    }

    bool mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
        {
            // We can "not consume" so QCustomPlot does its normal drag
            return false;
        }
        return false;
    }

    // Typically, we let QCustomPlot do the built-in dragging,
    // so we might not need to override mouseMoveEvent or mouseReleaseEvent.

    // This is a primary tool
    bool isPrimary() override { return true; }

private:
    QCustomPlot* m_plot;
};

} // namespace FlySight

#endif // PANTOOL_H
