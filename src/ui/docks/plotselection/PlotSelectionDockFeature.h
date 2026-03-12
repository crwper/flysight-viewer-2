#ifndef PLOTSELECTIONDOCKFEATURE_H
#define PLOTSELECTIONDOCKFEATURE_H

#include "ui/docks/DockFeature.h"
#include <QSet>
#include <QTreeView>

class QAbstractItemModel;
class QSettings;

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
    void saveExpansionState();
    void restoreExpansionState();
    void onExpanded(const QModelIndex &index);
    void onCollapsed(const QModelIndex &index);

    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    QTreeView* m_treeView = nullptr;
    QAbstractItemModel* m_plotModel = nullptr;
    QSettings* m_settings = nullptr;
    QSet<QString> m_expandedCategories;
};

} // namespace FlySight

#endif // PLOTSELECTIONDOCKFEATURE_H
