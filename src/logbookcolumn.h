#ifndef LOGBOOKCOLUMN_H
#define LOGBOOKCOLUMN_H

#include <QObject>
#include <QString>
#include <QVector>

namespace FlySight {

// ============================================================================
// Column Type Enum
// ============================================================================

enum class ColumnType {
    SessionAttribute,      // References one AttributeRegistry entry
    MeasurementAtMarker,   // References one PlotRegistry entry + one MarkerRegistry entry
    Delta                  // References one PlotRegistry entry + two MarkerRegistry entries
};

// ============================================================================
// LogbookColumn Struct
// ============================================================================

struct LogbookColumn {
    ColumnType type;

    // --- Session Attribute fields ---
    QString attributeKey;

    // --- Measurement fields ---
    QString sensorID;
    QString measurementID;
    QString measurementType;    // For UnitConverter

    // --- Marker fields ---
    QString markerAttributeKey;

    // --- Second marker (Delta only) ---
    QString marker2AttributeKey;

    // --- Common fields ---
    bool enabled = true;
    QString customLabel;

    bool operator==(const LogbookColumn &other) const {
        return type == other.type
            && attributeKey == other.attributeKey
            && sensorID == other.sensorID
            && measurementID == other.measurementID
            && measurementType == other.measurementType
            && markerAttributeKey == other.markerAttributeKey
            && marker2AttributeKey == other.marker2AttributeKey
            && enabled == other.enabled
            && customLabel == other.customLabel;
    }
};

/// Auto-generated display name based on column type and registry lookups
QString logbookColumnDisplayName(const LogbookColumn &col);

/// Returns customLabel if non-empty, otherwise logbookColumnDisplayName(col)
QString logbookColumnLabel(const LogbookColumn &col);

// ============================================================================
// LogbookColumnStore — QObject singleton for persistence
// ============================================================================

class LogbookColumnStore : public QObject {
    Q_OBJECT
public:
    static LogbookColumnStore& instance();

    QVector<LogbookColumn> columns() const;
    QVector<LogbookColumn> enabledColumns() const;
    void setColumns(const QVector<LogbookColumn> &columns);
    void load();

signals:
    void columnsChanged();

private:
    LogbookColumnStore();
    void save();
    void loadDefaults();

    QVector<LogbookColumn> m_columns;

    Q_DISABLE_COPY(LogbookColumnStore)
};

} // namespace FlySight

#endif // LOGBOOKCOLUMN_H
