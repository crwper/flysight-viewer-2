#ifndef PLOTSELECTIONDOCKFEATURE_H
#define PLOTSELECTIONDOCKFEATURE_H

#include "ui/docks/DockFeature.h"
#include <QTreeView>

namespace FlySight {

struct AppContext;

/**
 * DockFeature for the plot selection tree view.
 * Displays available plot categories and individual plots.
 */
class PlotSelectionDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit PlotSelectionDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    QTreeView* m_treeView = nullptr;
};

} // namespace FlySight

#endif // PLOTSELECTIONDOCKFEATURE_H
