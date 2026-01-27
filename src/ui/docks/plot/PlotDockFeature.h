#ifndef PLOTDOCKFEATURE_H
#define PLOTDOCKFEATURE_H

#include "ui/docks/DockFeature.h"
#include "plotwidget.h"

namespace FlySight {

struct AppContext;

/**
 * DockFeature for the main plot widget.
 * Displays QCustomPlot graphs with plot tools (pan, zoom, select).
 */
class PlotDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit PlotDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

    PlotWidget* plotWidget() const { return m_plotWidget; }

signals:
    // Re-expose PlotWidget signals for MainWindow to connect
    void sessionsSelected(const QList<QString> &sessionIds);
    void toolChanged(PlotWidget::Tool newTool);

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    PlotWidget* m_plotWidget = nullptr;
};

} // namespace FlySight

#endif // PLOTDOCKFEATURE_H
