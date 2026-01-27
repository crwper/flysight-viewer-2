#include "LegendDockFeature.h"
#include "ui/docks/AppContext.h"
#include "legendwidget.h"
#include "legendpresenter.h"
#include "sessionmodel.h"
#include "plotmodel.h"
#include "cursormodel.h"
#include "plotviewsettingsmodel.h"

namespace FlySight {

LegendDockFeature::LegendDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Legend"));

    // Create LegendWidget
    m_legendWidget = new LegendWidget(m_dock);
    m_dock->setWidget(m_legendWidget);

    // Create LegendPresenter to drive legend updates
    m_presenter = new LegendPresenter(ctx.sessionModel,
                                      ctx.plotModel,
                                      ctx.cursorModel,
                                      ctx.plotViewSettings,
                                      m_legendWidget,
                                      this);
}

QString LegendDockFeature::id() const
{
    return QStringLiteral("Legend");
}

QString LegendDockFeature::title() const
{
    return QStringLiteral("Legend");
}

KDDockWidgets::QtWidgets::DockWidget* LegendDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location LegendDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnRight;
}

} // namespace FlySight
