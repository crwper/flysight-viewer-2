#ifndef SETSYNCTOOL_H
#define SETSYNCTOOL_H

#include "plottool.h"
#include "ui/docks/plot/PlotWidget.h"

namespace FlySight {

/*!
 * \brief SetSyncTool: Updates the session's SyncTime when the user clicks.
 * Behaves identically to SetExitTool but targets SyncTime instead of ExitTime.
 */
class SetSyncTool : public PlotTool {
public:
    SetSyncTool(const PlotWidget::PlotContext &ctx);

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

#endif // SETSYNCTOOL_H
