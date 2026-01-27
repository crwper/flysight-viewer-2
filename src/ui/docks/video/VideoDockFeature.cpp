#include "VideoDockFeature.h"
#include "ui/docks/AppContext.h"
#include "VideoWidget.h"
#include "sessionmodel.h"
#include "cursormodel.h"

namespace FlySight {

VideoDockFeature::VideoDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Video"));

    // Create VideoWidget
    m_videoWidget = new VideoWidget(ctx.sessionModel, ctx.cursorModel, m_dock);
    m_dock->setWidget(m_videoWidget);
}

QString VideoDockFeature::id() const
{
    return QStringLiteral("Video");
}

QString VideoDockFeature::title() const
{
    return QStringLiteral("Video");
}

KDDockWidgets::QtWidgets::DockWidget* VideoDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location VideoDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnBottom;
}

} // namespace FlySight
