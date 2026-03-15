#include "profile.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

using namespace FlySight;

// ============================================================================
// Serialization
// ============================================================================

QJsonObject FlySight::profileToJson(const Profile &profile)
{
    // Start with extra keys so that known keys overwrite any collisions
    QJsonObject obj = profile.extraKeys;

    // Display name (always written)
    obj[ProfileKeys::Name] = profile.displayName;

    // Optional state fields — only written when present
    if (profile.enabledPlots.has_value()) {
        QJsonArray arr;
        for (const QString &s : *profile.enabledPlots)
            arr.append(s);
        obj[ProfileKeys::EnabledPlots] = arr;
    }

    if (profile.enabledMarkers.has_value()) {
        QJsonArray arr;
        for (const QString &s : *profile.enabledMarkers)
            arr.append(s);
        obj[ProfileKeys::EnabledMarkers] = arr;
    }

    if (profile.referenceMarker.has_value())
        obj[ProfileKeys::ReferenceMarker] = *profile.referenceMarker;

    if (profile.xAxisVariable.has_value())
        obj[ProfileKeys::XAxisVariable] = *profile.xAxisVariable;

    if (profile.zoomExtent.has_value())
        obj[ProfileKeys::ZoomExtent] = *profile.zoomExtent;

    if (profile.logbookColumns.has_value())
        obj[ProfileKeys::LogbookColumns] = *profile.logbookColumns;

    if (profile.dockLayout.has_value())
        obj[ProfileKeys::DockLayout] = QString::fromLatin1(profile.dockLayout->toBase64());

    if (profile.treeExpansionState.has_value())
        obj[ProfileKeys::TreeExpansion] = *profile.treeExpansionState;

    if (profile.altitudeMarkers.has_value())
        obj[ProfileKeys::AltitudeMarkers] = *profile.altitudeMarkers;

    return obj;
}

Profile FlySight::profileFromJson(const QJsonObject &obj, const QString &id)
{
    Profile profile;
    profile.id = id;

    // Known keys — collected so we can separate extra keys
    static const QSet<QString> knownKeys = {
        ProfileKeys::Name,
        ProfileKeys::EnabledPlots,
        ProfileKeys::EnabledMarkers,
        ProfileKeys::ReferenceMarker,
        ProfileKeys::XAxisVariable,
        ProfileKeys::ZoomExtent,
        ProfileKeys::LogbookColumns,
        ProfileKeys::DockLayout,
        ProfileKeys::TreeExpansion,
        ProfileKeys::AltitudeMarkers
    };

    // Display name
    if (obj.contains(ProfileKeys::Name))
        profile.displayName = obj[ProfileKeys::Name].toString();

    // Enabled plots
    if (obj.contains(ProfileKeys::EnabledPlots)) {
        QStringList list;
        const QJsonArray arr = obj[ProfileKeys::EnabledPlots].toArray();
        for (const QJsonValue &v : arr)
            list.append(v.toString());
        profile.enabledPlots = list;
    }

    // Enabled markers
    if (obj.contains(ProfileKeys::EnabledMarkers)) {
        QStringList list;
        const QJsonArray arr = obj[ProfileKeys::EnabledMarkers].toArray();
        for (const QJsonValue &v : arr)
            list.append(v.toString());
        profile.enabledMarkers = list;
    }

    // Reference marker
    if (obj.contains(ProfileKeys::ReferenceMarker))
        profile.referenceMarker = obj[ProfileKeys::ReferenceMarker].toString();

    // X-axis variable
    if (obj.contains(ProfileKeys::XAxisVariable))
        profile.xAxisVariable = obj[ProfileKeys::XAxisVariable].toString();

    // Zoom extent
    if (obj.contains(ProfileKeys::ZoomExtent))
        profile.zoomExtent = obj[ProfileKeys::ZoomExtent].toObject();

    // Logbook columns
    if (obj.contains(ProfileKeys::LogbookColumns))
        profile.logbookColumns = obj[ProfileKeys::LogbookColumns].toArray();

    // Dock layout (Base64-encoded string -> QByteArray)
    if (obj.contains(ProfileKeys::DockLayout)) {
        const QString encoded = obj[ProfileKeys::DockLayout].toString();
        profile.dockLayout = QByteArray::fromBase64(encoded.toLatin1());
    }

    // Tree expansion state
    if (obj.contains(ProfileKeys::TreeExpansion))
        profile.treeExpansionState = obj[ProfileKeys::TreeExpansion].toObject();

    // Altitude markers
    if (obj.contains(ProfileKeys::AltitudeMarkers))
        profile.altitudeMarkers = obj[ProfileKeys::AltitudeMarkers].toObject();

    // Collect unknown keys for round-trip preservation
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!knownKeys.contains(it.key()))
            profile.extraKeys[it.key()] = it.value();
    }

    return profile;
}
