#include "MapDockFeature.h"
#include "ui/docks/AppContext.h"
#include "MapWidget.h"
#include "sessionmodel.h"
#include "cursormodel.h"
#include "plotrangemodel.h"

namespace FlySight {

MapDockFeature::MapDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Map"));

    // Create MapWidget
    m_mapWidget = new MapWidget(ctx.sessionModel, ctx.cursorModel, ctx.rangeModel, m_dock);
    m_dock->setWidget(m_mapWidget);
}

QString MapDockFeature::id() const
{
    return QStringLiteral("Map");
}

QString MapDockFeature::title() const
{
    return QStringLiteral("Map");
}

KDDockWidgets::QtWidgets::DockWidget* MapDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location MapDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnBottom;
}

} // namespace FlySight
