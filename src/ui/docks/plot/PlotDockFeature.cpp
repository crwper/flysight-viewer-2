#include "PlotDockFeature.h"
#include "ui/docks/AppContext.h"
#include "PlotWidget.h"
#include "sessionmodel.h"
#include "plotmodel.h"
#include "markermodel.h"
#include "plotviewsettingsmodel.h"
#include "cursormodel.h"
#include "plotrangemodel.h"

namespace FlySight {

PlotDockFeature::PlotDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Plots"));

    // Create PlotWidget with all required models
    m_plotWidget = new PlotWidget(ctx.sessionModel,
                                  ctx.plotModel,
                                  ctx.markerModel,
                                  ctx.plotViewSettings,
                                  ctx.cursorModel,
                                  ctx.rangeModel,
                                  ctx.measureModel,
                                  m_dock);
    m_dock->setWidget(m_plotWidget);

    // Forward PlotWidget signals to feature signals
    connect(m_plotWidget, &PlotWidget::sessionsSelected,
            this, &PlotDockFeature::sessionsSelected);
    connect(m_plotWidget, &PlotWidget::toolChanged,
            this, &PlotDockFeature::toolChanged);

    // Connect MarkerModel changes to PlotWidget marker updates
    if (ctx.markerModel) {
        connect(ctx.markerModel, &QAbstractItemModel::modelReset,
                m_plotWidget, [this]() {
                    if (m_plotWidget)
                        m_plotWidget->updateMarkersOnly();
                });

        connect(ctx.markerModel, &QAbstractItemModel::dataChanged,
                m_plotWidget, [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                    if (m_plotWidget)
                        m_plotWidget->updateMarkersOnly();
                });
    }
}

QString PlotDockFeature::id() const
{
    return QStringLiteral("Plots");
}

QString PlotDockFeature::title() const
{
    return QStringLiteral("Plots");
}

KDDockWidgets::QtWidgets::DockWidget* PlotDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location PlotDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnLeft;
}

} // namespace FlySight
