#ifndef SETGROUNDTOOL_H
#define SETGROUNDTOOL_H

#include "plottool.h"
#include "../plotwidget.h"

namespace FlySight {

class SetGroundTool : public PlotTool {
public:
    SetGroundTool(const PlotWidget::PlotContext &ctx);

    bool mousePressEvent(QMouseEvent *event) override;

private:
    QCustomPlot* m_plot;
    QMap<QCPGraph*, PlotWidget::GraphInfo>* m_graphMap;
    SessionModel* m_model;

    // Helper: compute ground elevation by interpolating (TimeFromExit vs hMSL)
    double computeGroundElevation(SessionData &session, double xFromExit) const;
};

} // namespace FlySight

#endif // SETGROUNDTOOL_H
