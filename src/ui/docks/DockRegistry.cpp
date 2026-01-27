#include "DockRegistry.h"
#include "AppContext.h"
#include "DockFeature.h"

// Include concrete DockFeature headers here as they are created in Phase 3:
#include "logbook/LogbookDockFeature.h"
#include "legend/LegendDockFeature.h"
#include "plot/PlotDockFeature.h"
#include "plotselection/PlotSelectionDockFeature.h"
#include "markerselection/MarkerSelectionDockFeature.h"
#include "map/MapDockFeature.h"
#include "video/VideoDockFeature.h"

namespace FlySight {

QList<DockFeature*> DockRegistry::createAll(const AppContext& ctx, QObject* parent)
{
    QList<DockFeature*> features;

    // Create in display order (matches Window menu order)
    features.append(new LogbookDockFeature(ctx, parent));
    features.append(new PlotDockFeature(ctx, parent));
    features.append(new LegendDockFeature(ctx, parent));
    features.append(new MapDockFeature(ctx, parent));
    features.append(new PlotSelectionDockFeature(ctx, parent));
    features.append(new MarkerSelectionDockFeature(ctx, parent));
    features.append(new VideoDockFeature(ctx, parent));

    return features;
}

} // namespace FlySight
