#ifndef PLOTTOOL_H
#define PLOTTOOL_H

#include <QMouseEvent>

namespace FlySight {

class PlotTool
{
public:
    virtual ~PlotTool() = default;

    // Called when the user presses, moves, or releases the mouse
    // Return true if the tool consumed the event (so PlotWidget won't pass it on to QCustomPlot)
    virtual bool mousePressEvent(QMouseEvent *event)   { Q_UNUSED(event); return false; }
    virtual bool mouseMoveEvent(QMouseEvent *event)    { Q_UNUSED(event); return false; }
    virtual bool mouseReleaseEvent(QMouseEvent *event) { Q_UNUSED(event); return false; }

    // Called when the mouse leaves the widget area (optional)
    virtual bool leaveEvent(QEvent *event)             { Q_UNUSED(event); return false; }

    // Distinguish between primary tools and momentary tools
    virtual bool isPrimary() = 0;
};

} // namespace FlySight

#endif // PLOTTOOL_H
