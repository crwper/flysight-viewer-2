#ifndef MARKERSELECTIONDOCKFEATURE_H
#define MARKERSELECTIONDOCKFEATURE_H

#include "ui/docks/DockFeature.h"
#include <QTreeView>

namespace FlySight {

struct AppContext;

/**
 * DockFeature for the marker selection tree view.
 * Displays available marker categories and individual markers.
 */
class MarkerSelectionDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit MarkerSelectionDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    QTreeView* m_treeView = nullptr;
};

} // namespace FlySight

#endif // MARKERSELECTIONDOCKFEATURE_H
