#include "PlotSelectionDockFeature.h"
#include "ui/docks/AppContext.h"
#include "plotmodel.h"
#include <QAbstractItemView>

namespace FlySight {

PlotSelectionDockFeature::PlotSelectionDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Plot Selection"));

    // Create tree view
    m_treeView = new QTreeView(m_dock);
    m_dock->setWidget(m_treeView);

    // Configure tree view
    m_treeView->setModel(ctx.plotModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

QString PlotSelectionDockFeature::id() const
{
    return QStringLiteral("Plot Selection");
}

QString PlotSelectionDockFeature::title() const
{
    return QStringLiteral("Plot Selection");
}

KDDockWidgets::QtWidgets::DockWidget* PlotSelectionDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location PlotSelectionDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnLeft;
}

} // namespace FlySight
