#ifndef SETEXITTOOL_H
#define SETEXITTOOL_H

#include "plottool.h"
#include "../plotwidget.h"
#include "../graphinfo.h"

namespace FlySight {

/*!
 * \brief SetExitTool: Now it only updates the session’s ExitTime
 * when the user clicks, and doesn't manage tracers at all.
 */
class SetExitTool : public PlotTool {
public:
    SetExitTool(const PlotWidget::PlotContext &ctx);

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    void activateTool() override;
    void closeTool() override;

    bool isPrimary() override { return false; }

private:
    PlotWidget* m_widget;
    QCustomPlot* m_plot;
    QMap<QCPGraph*, GraphInfo>* m_graphMap;
    SessionModel* m_model;
};

} // namespace FlySight

#endif // SETEXITTOOL_H
