#ifndef SETCOURSETOOL_H
#define SETCOURSETOOL_H

#include "plottool.h"
#include "ui/docks/plot/PlotWidget.h"

namespace FlySight {

/*!
 * \brief SetCourseTool: Updates the session's CourseRef time
 * when the user clicks, mirroring SetExitTool.
 */
class SetCourseTool : public PlotTool {
public:
    SetCourseTool(const PlotWidget::PlotContext &ctx);

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    void activateTool() override;
    void closeTool() override;

    bool isPrimary() override { return false; }

private:
    PlotWidget* m_widget;
    QCustomPlot* m_plot;
    SessionModel* m_model;
};

} // namespace FlySight

#endif // SETCOURSETOOL_H
