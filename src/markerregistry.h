#ifndef MARKERREGISTRY_H
#define MARKERREGISTRY_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QVector>

namespace FlySight {

class MomentModel;

struct MarkerMeasurement {
    QString sensor;
    QString timeVector;
    QString dataVector;
};

// Marker definitions
struct MarkerDefinition {
    QString category;
    QString displayName;
    QString shortLabel;
    QColor color;
    QString attributeKey; // Unique, stable marker id (session attribute key)
    QVector<MarkerMeasurement> measurements;  // sensor measurements this marker relates to
    bool    editable = false;              // whether the user can reposition by dragging
    QString groupId;                       // empty = statically registered; non-empty = managed group
    bool    defaultEnabled = false;        // initial enabled state when first seen
};

class MarkerRegistry : public QObject {
    Q_OBJECT

public:
    static MarkerRegistry* instance();

    /// Set the MomentModel so that marker registrations are mirrored as moments.
    /// Must be called before registerMarker / registerMarkers.
    void setMomentModel(MomentModel *momentModel);

    /// register one marker (called by C++ or future PluginHost)
    void registerMarker(const MarkerDefinition& def);

    /// register a batch of markers, emitting markersChanged() once after all are added
    void registerMarkers(const QVector<MarkerDefinition> &defs);

    /// returns all markers (built-in + plugins). Called by MainWindow.
    QVector<MarkerDefinition> allMarkers() const;

    /// removes all markers belonging to the given group
    void clearMarkerGroup(const QString &groupId);

    /// atomically replace all markers in a group (one markersChanged signal)
    void replaceMarkerGroup(const QString &groupId, const QVector<MarkerDefinition> &defs);

signals:
    void markersChanged();

private:
    explicit MarkerRegistry(QObject *parent = nullptr);

    QVector<MarkerDefinition> m_markers;
    MomentModel *m_momentModel = nullptr;

    Q_DISABLE_COPY(MarkerRegistry)
};

} // namespace FlySight

#endif // MARKERREGISTRY_H
