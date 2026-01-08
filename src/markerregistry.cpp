#include "markerregistry.h"

using namespace FlySight;

MarkerRegistry& MarkerRegistry::instance() {
    static MarkerRegistry R;
    return R;
}

void MarkerRegistry::registerMarker(const MarkerDefinition& def) {
    m_markers.append(def);
}

QVector<MarkerDefinition> MarkerRegistry::allMarkers() const {
    return m_markers;
}
