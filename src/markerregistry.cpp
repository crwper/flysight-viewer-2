#include "markerregistry.h"
#include "momentmodel.h"

using namespace FlySight;

namespace {

/// Translate a MarkerDefinition into MomentTraits.
/// Trait mapping follows the overview spec tables:
///   - editable == true  -> Interaction::Drag
///   - editable == false -> Interaction::None
///   - All markers get DashedLineOnDrag, Bubble, SmallDot, Hidden legend
///   - PositionSource: Attribute with the marker's attributeKey
MomentTraits markerTraitsFromDef(const MarkerDefinition &def)
{
    MomentTraits traits;
    traits.positionSource = PositionSource::Attribute;
    traits.attributeKey = def.attributeKey;
    traits.interaction = def.editable ? Interaction::Drag : Interaction::None;
    traits.plotPresentation = PlotPresentation::DashedLineOnDrag;
    traits.plotBubble = PlotBubble::Bubble;
    traits.mapPresentation = MapPresentation::SmallDot;
    traits.legendVisibility = LegendVisibility::Hidden;
    traits.shortLabel = def.shortLabel;
    traits.color = def.color;
    return traits;
}

} // anonymous namespace

MarkerRegistry* MarkerRegistry::instance() {
    static MarkerRegistry* s_instance = new MarkerRegistry(nullptr);
    return s_instance;
}

MarkerRegistry::MarkerRegistry(QObject *parent)
    : QObject(parent)
{
}

void MarkerRegistry::setMomentModel(MomentModel *momentModel)
{
    m_momentModel = momentModel;
}

void MarkerRegistry::registerMarker(const MarkerDefinition& def) {
    m_markers.append(def);
    if (m_momentModel) {
        m_momentModel->registerMoment(def.attributeKey, def.displayName,
                                      markerTraitsFromDef(def), def.groupId);
    }
    emit markersChanged();
}

void MarkerRegistry::registerMarkers(const QVector<MarkerDefinition> &defs) {
    QVector<MomentRegistration> momentRegs;
    momentRegs.reserve(defs.size());

    for (const MarkerDefinition &def : defs) {
        m_markers.append(def);

        MomentRegistration reg;
        reg.id = def.attributeKey;
        reg.label = def.displayName;
        reg.traits = markerTraitsFromDef(def);
        reg.groupId = def.groupId;
        momentRegs.append(reg);
    }

    if (m_momentModel && !momentRegs.isEmpty()) {
        m_momentModel->registerMoments(momentRegs);
    }

    emit markersChanged();
}

void MarkerRegistry::clearMarkerGroup(const QString &groupId) {
    m_markers.removeIf([&groupId](const MarkerDefinition &def) {
        return def.groupId == groupId;
    });

    if (m_momentModel) {
        m_momentModel->unregisterMomentGroup(groupId);
    }

    emit markersChanged();
}

QVector<MarkerDefinition> MarkerRegistry::allMarkers() const {
    return m_markers;
}
