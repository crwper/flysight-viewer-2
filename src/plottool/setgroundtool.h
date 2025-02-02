#ifndef SETGROUNDTOOL_H
#define SETGROUNDTOOL_H

#include "plottool.h"
#include "../plotwidget.h"

namespace FlySight {

class SetGroundTool : public PlotTool {
public:
    SetGroundTool(const PlotWidget::PlotContext &ctx);

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;

    void activateTool() override;
    void closeTool() override;

    // This is a momentary tool
    bool isPrimary() override { return false; }

private:
    PlotWidget* m_widget;
    QCustomPlot* m_plot;
    QMap<QCPGraph*, PlotWidget::GraphInfo>* m_graphMap;
    SessionModel* m_model;

    QMap<QCPGraph*, QCPItemTracer*> m_graphTracers;
    QString m_hoveredSessionId;

    QCPItemTracer* getOrCreateTracer(QCPGraph* graph);
    void clearTracers();
    double computeGroundElevation(SessionData &session, double xFromExit) const;
};

} // namespace FlySight

#endif // SETGROUNDTOOL_H
