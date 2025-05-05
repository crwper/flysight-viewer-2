#include "plotregistry.h"

using namespace FlySight;

PlotRegistry& PlotRegistry::instance() {
    static PlotRegistry R;
    return R;
}

void PlotRegistry::registerPlot(const PlotValue& pv) {
    m_plots.append(pv);
}

QVector<PlotValue> PlotRegistry::allPlots() const {
    return m_plots;
}
