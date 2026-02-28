#ifndef MARKERREGISTRY_H
#define MARKERREGISTRY_H

#include <QString>
#include <QColor>
#include <QPair>
#include <QVector>

namespace FlySight {

using MeasurementKey = QPair<QString, QString>;

// Marker definitions
struct MarkerDefinition {
    QString category;
    QString displayName;
    QString shortLabel;
    QColor color;
    QString attributeKey; // Unique, stable marker id (session attribute key)
    QVector<MeasurementKey> measurements;  // sensor measurements this marker relates to
    bool    editable = false;              // whether the user can reposition by dragging
};

class MarkerRegistry {
public:
    static MarkerRegistry& instance();

    /// register one marker (called by C++ or future PluginHost)
    void registerMarker(const MarkerDefinition& def);

    /// returns all markers (built-in + plugins). Called by MainWindow.
    QVector<MarkerDefinition> allMarkers() const;

private:
    QVector<MarkerDefinition> m_markers;
};

} // namespace FlySight

#endif // MARKERREGISTRY_H
