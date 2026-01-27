#include "MarkerSelectionDockFeature.h"
#include "ui/docks/AppContext.h"
#include "markermodel.h"
#include <QAbstractItemView>

namespace FlySight {

MarkerSelectionDockFeature::MarkerSelectionDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Marker Selection"));

    // Create tree view
    m_treeView = new QTreeView(m_dock);
    m_dock->setWidget(m_treeView);

    // Configure tree view
    m_treeView->setModel(ctx.markerModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

QString MarkerSelectionDockFeature::id() const
{
    return QStringLiteral("Marker Selection");
}

QString MarkerSelectionDockFeature::title() const
{
    return QStringLiteral("Marker Selection");
}

KDDockWidgets::QtWidgets::DockWidget* MarkerSelectionDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location MarkerSelectionDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnLeft;
}

} // namespace FlySight
