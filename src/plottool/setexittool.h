#ifndef SETEXITTOOL_H
#define SETEXITTOOL_H

#include "plottool.h"
#include "../plotwidget.h"

namespace FlySight {

class SetExitTool : public PlotTool {
public:
    SetExitTool(const PlotWidget::PlotContext &ctx);
    bool mousePressEvent(QMouseEvent *event) override;

    // This is a momentary tool
    bool isPrimary() override { return false; }

private:
    PlotWidget* m_widget;
    QCustomPlot* m_plot;
    QMap<QCPGraph*, PlotWidget::GraphInfo>* m_graphMap;
    SessionModel* m_model;
};

} // namespace FlySight

#endif // SETEXITTOOL_H
