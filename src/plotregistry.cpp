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

QVector<PlotValue> PlotRegistry::dependentPlots() const {
    QVector<PlotValue> out;
    for (const auto &pv : m_plots) {
        if (pv.role == PlotRole::Dependent)
            out.append(pv);
    }
    return out;
}

QVector<PlotValue> PlotRegistry::independentPlots() const {
    QVector<PlotValue> out;
    for (const auto &pv : m_plots) {
        if (pv.role == PlotRole::Independent)
            out.append(pv);
    }
    return out;
}
