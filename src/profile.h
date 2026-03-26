#ifndef PROFILE_H
#define PROFILE_H

#include <optional>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace FlySight {

// ============================================================================
// JSON Key Constants
// ============================================================================

namespace ProfileKeys {
    inline const QString Name             = QStringLiteral("name");
    inline const QString EnabledPlots     = QStringLiteral("enabledPlots");
    inline const QString EnabledMarkers   = QStringLiteral("enabledMarkers");
    inline const QString ReferenceMarker  = QStringLiteral("referenceMarker");
    inline const QString XAxisVariable    = QStringLiteral("xAxisVariable");
    inline const QString ZoomExtent       = QStringLiteral("zoomExtent");
    inline const QString LogbookColumns   = QStringLiteral("logbookColumns");
    inline const QString DockLayout       = QStringLiteral("dockLayout");
    inline const QString TreeExpansion    = QStringLiteral("treeExpansionState");
    inline const QString AltitudeMarkers  = QStringLiteral("altitudeMarkers");
    inline const QString AnalysisMethod   = QStringLiteral("analysisMethod");
}

// ============================================================================
// Profile Struct
// ============================================================================

struct Profile {
    // Identity
    QString id;            // Unique identifier (filename stem, assigned on first save)
    QString displayName;   // Human-readable name shown in menus

    // State fields — all optional; absence means "do not apply this setting"
    std::optional<QStringList> enabledPlots;            // list of "sensorID/measurementID"
    std::optional<QStringList> enabledMarkers;          // list of attributeKey strings
    std::optional<QString>     referenceMarker;         // attributeKey of reference marker
    std::optional<QString>     xAxisVariable;           // "sensorID/measurementID"
    std::optional<QJsonObject> zoomExtent;              // sub-object with mode, startMarker, endMarker, marginPct
    std::optional<QJsonArray>  logbookColumns;          // array of column config objects
    std::optional<QByteArray>  dockLayout;              // KDDockWidgets serialized layout (Base64-encoded in JSON)
    std::optional<QJsonObject> treeExpansionState;      // sub-object: category expansion state for plot/marker docks
    std::optional<QJsonObject> altitudeMarkers;         // sub-object: units and altitude values for altitude markers
    std::optional<QString>     analysisMethod;          // analysis dock method name (e.g. "Wingsuit Performance")

    // Round-trip preservation: keys present in JSON but not recognized by this version
    QJsonObject extraKeys;
};

// ============================================================================
// Serialization
// ============================================================================

QJsonObject profileToJson(const Profile &profile);
Profile profileFromJson(const QJsonObject &obj, const QString &id);

} // namespace FlySight

#endif // PROFILE_H
