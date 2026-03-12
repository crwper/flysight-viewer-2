#include "logbookcolumn.h"

#include <QSettings>

#include "attributeregistry.h"
#include "markerregistry.h"
#include "plotregistry.h"
#include "sessiondata.h"
#include "preferences/preferencekeys.h"
#include "preferences/preferencesmanager.h"

using namespace FlySight;

// ============================================================================
// Display name helpers
// ============================================================================

QString FlySight::logbookColumnDisplayName(const LogbookColumn &col)
{
    switch (col.type) {
    case ColumnType::SessionAttribute: {
        // Look up the display name from AttributeRegistry
        const auto attrs = AttributeRegistry::instance().allAttributes();
        for (const auto &def : attrs) {
            if (def.attributeKey == col.attributeKey)
                return def.displayName;
        }
        // Fallback: return the raw key
        return col.attributeKey;
    }
    case ColumnType::MeasurementAtMarker: {
        // Build "{plotName} @ {markerDisplayName}"
        QString plotName;
        const auto plots = PlotRegistry::instance().allPlots();
        for (const auto &pv : plots) {
            if (pv.sensorID == col.sensorID && pv.measurementID == col.measurementID) {
                plotName = pv.plotName;
                break;
            }
        }
        if (plotName.isEmpty())
            plotName = col.sensorID + QStringLiteral("/") + col.measurementID;

        QString markerName;
        const auto markers = MarkerRegistry::instance()->allMarkers();
        for (const auto &md : markers) {
            if (md.attributeKey == col.markerAttributeKey) {
                markerName = md.displayName;
                break;
            }
        }
        if (markerName.isEmpty())
            markerName = col.markerAttributeKey;

        return plotName + QStringLiteral(" @ ") + markerName;
    }
    case ColumnType::Delta: {
        // Build "{plotName} ({marker1DisplayName} -> {marker2DisplayName})"
        QString plotName;
        const auto plots = PlotRegistry::instance().allPlots();
        for (const auto &pv : plots) {
            if (pv.sensorID == col.sensorID && pv.measurementID == col.measurementID) {
                plotName = pv.plotName;
                break;
            }
        }
        if (plotName.isEmpty())
            plotName = col.sensorID + QStringLiteral("/") + col.measurementID;

        QString marker1Name, marker2Name;
        const auto markers = MarkerRegistry::instance()->allMarkers();
        for (const auto &md : markers) {
            if (md.attributeKey == col.markerAttributeKey)
                marker1Name = md.displayName;
            if (md.attributeKey == col.marker2AttributeKey)
                marker2Name = md.displayName;
        }
        if (marker1Name.isEmpty())
            marker1Name = col.markerAttributeKey;
        if (marker2Name.isEmpty())
            marker2Name = col.marker2AttributeKey;

        return plotName + QStringLiteral(" (") + marker1Name
               + QStringLiteral(" -> ") + marker2Name + QStringLiteral(")");
    }
    }

    return QString();
}

QString FlySight::logbookColumnLabel(const LogbookColumn &col)
{
    if (!col.customLabel.isEmpty())
        return col.customLabel;
    return logbookColumnDisplayName(col);
}

// ============================================================================
// LogbookColumnStore
// ============================================================================

static const QString kSettingsArrayKey = QStringLiteral("logbook/columns");

LogbookColumnStore& LogbookColumnStore::instance()
{
    static LogbookColumnStore store;
    return store;
}

LogbookColumnStore::LogbookColumnStore()
{
    // Register the version preference so PreferencesManager can track it
    PreferencesManager::instance().registerPreference(
        PreferenceKeys::LogbookColumnsVersion, 0);
}

QVector<LogbookColumn> LogbookColumnStore::columns() const
{
    return m_columns;
}

QVector<LogbookColumn> LogbookColumnStore::enabledColumns() const
{
    QVector<LogbookColumn> result;
    for (const auto &col : m_columns) {
        if (col.enabled)
            result.append(col);
    }
    return result;
}

void LogbookColumnStore::setColumns(const QVector<LogbookColumn> &columns)
{
    if (columns == m_columns)
        return;

    m_columns = columns;
    save();
    emit columnsChanged();
}

void LogbookColumnStore::load()
{
    QSettings settings;
    int count = settings.beginReadArray(kSettingsArrayKey);

    m_columns.clear();
    m_columns.reserve(count);

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);

        LogbookColumn col;
        col.type = static_cast<ColumnType>(
            settings.value(QStringLiteral("type"), 0).toInt());
        col.attributeKey = settings.value(QStringLiteral("attributeKey")).toString();
        col.sensorID = settings.value(QStringLiteral("sensorID")).toString();
        col.measurementID = settings.value(QStringLiteral("measurementID")).toString();
        col.measurementType = settings.value(QStringLiteral("measurementType")).toString();
        col.markerAttributeKey = settings.value(QStringLiteral("markerAttributeKey")).toString();
        col.marker2AttributeKey = settings.value(QStringLiteral("marker2AttributeKey")).toString();
        col.enabled = settings.value(QStringLiteral("enabled"), true).toBool();
        col.customLabel = settings.value(QStringLiteral("customLabel")).toString();

        m_columns.append(col);
    }
    settings.endArray();

    if (m_columns.isEmpty())
        loadDefaults();

    emit columnsChanged();
}

void LogbookColumnStore::save()
{
    // Write the QSettings array FIRST (before any scalar preference updates)
    QSettings settings;
    settings.beginWriteArray(kSettingsArrayKey, m_columns.size());
    for (int i = 0; i < m_columns.size(); ++i) {
        settings.setArrayIndex(i);
        const auto &col = m_columns[i];

        settings.setValue(QStringLiteral("type"), static_cast<int>(col.type));
        settings.setValue(QStringLiteral("attributeKey"), col.attributeKey);
        settings.setValue(QStringLiteral("sensorID"), col.sensorID);
        settings.setValue(QStringLiteral("measurementID"), col.measurementID);
        settings.setValue(QStringLiteral("measurementType"), col.measurementType);
        settings.setValue(QStringLiteral("markerAttributeKey"), col.markerAttributeKey);
        settings.setValue(QStringLiteral("marker2AttributeKey"), col.marker2AttributeKey);
        settings.setValue(QStringLiteral("enabled"), col.enabled);
        settings.setValue(QStringLiteral("customLabel"), col.customLabel);
    }
    settings.endArray();

    // Increment version to notify listeners
    auto &prefs = PreferencesManager::instance();
    int version = prefs.getValue(PreferenceKeys::LogbookColumnsVersion).toInt();
    prefs.setValue(PreferenceKeys::LogbookColumnsVersion, version + 1);
}

void LogbookColumnStore::loadDefaults()
{
    m_columns.clear();

    // Six SessionAttribute columns matching the current hard-coded logbook layout
    auto makeCol = [](const char *key) {
        LogbookColumn col;
        col.type = ColumnType::SessionAttribute;
        col.attributeKey = QString::fromLatin1(key);
        col.enabled = true;
        return col;
    };

    m_columns.append(makeCol(SessionKeys::Description));
    m_columns.append(makeCol(SessionKeys::DeviceId));
    m_columns.append(makeCol(SessionKeys::StartTime));
    m_columns.append(makeCol(SessionKeys::Duration));
    m_columns.append(makeCol(SessionKeys::ExitTime));
    m_columns.append(makeCol(SessionKeys::GroundElev));

    save();
}
