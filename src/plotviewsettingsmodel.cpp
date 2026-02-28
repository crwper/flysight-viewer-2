#include "plotviewsettingsmodel.h"

#include "markerregistry.h"
#include "sessiondata.h"

namespace FlySight {

PlotViewSettingsModel::PlotViewSettingsModel(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    // Migration: if the new keys do not exist but the old key does, convert
    const bool hasNewKeys = m_settings &&
        (m_settings->contains(QStringLiteral("plot/xVariable")) ||
         m_settings->contains(QStringLiteral("plot/referenceMarkerKey")));

    if (!hasNewKeys && m_settings && m_settings->contains(QStringLiteral("plot/xAxisKey"))) {
        const QString oldKey = m_settings->value(QStringLiteral("plot/xAxisKey")).toString();

        if (oldKey == SessionKeys::Time) {
            // Absolute time mode: no reference marker
            m_xVariable = SessionKeys::Time;
            m_referenceMarkerKey = QString();
        } else {
            // Default / TimeFromExit: reference to exit
            m_xVariable = SessionKeys::Time;
            m_referenceMarkerKey = QStringLiteral("_EXIT_TIME");
        }

        // Persist the migrated values and remove the old key
        m_settings->setValue(QStringLiteral("plot/xVariable"), m_xVariable);
        m_settings->setValue(QStringLiteral("plot/referenceMarkerKey"), m_referenceMarkerKey);
        m_settings->remove(QStringLiteral("plot/xAxisKey"));
    } else {
        // Read from new keys (or use defaults)
        m_xVariable = m_settings
            ? m_settings->value(QStringLiteral("plot/xVariable"), SessionKeys::Time).toString()
            : SessionKeys::Time;

        m_referenceMarkerKey = m_settings
            ? m_settings->value(QStringLiteral("plot/referenceMarkerKey"),
                                QStringLiteral("_EXIT_TIME")).toString()
            : QStringLiteral("_EXIT_TIME");
    }
}

QString PlotViewSettingsModel::xVariable() const
{
    return m_xVariable;
}

QString PlotViewSettingsModel::referenceMarkerKey() const
{
    return m_referenceMarkerKey;
}

QString PlotViewSettingsModel::xAxisLabel() const
{
    if (m_referenceMarkerKey.isEmpty()) {
        return tr("Time (s)");
    }

    // Look up the marker's display name from MarkerRegistry
    const QVector<MarkerDefinition> markers = MarkerRegistry::instance().allMarkers();
    for (const MarkerDefinition &md : markers) {
        if (md.attributeKey == m_referenceMarkerKey) {
            return tr("Time from %1 (s)").arg(md.displayName.toLower());
        }
    }

    // Marker not found in registry: fall back to simple label
    return tr("Time (s)");
}

void PlotViewSettingsModel::setXVariable(const QString &xVariable)
{
    if (m_xVariable == xVariable)
        return;

    m_xVariable = xVariable;

    if (m_settings)
        m_settings->setValue(QStringLiteral("plot/xVariable"), m_xVariable);

    emit xVariableChanged(m_xVariable);
}

void PlotViewSettingsModel::setReferenceMarkerKey(const QString &key)
{
    if (m_referenceMarkerKey == key)
        return;

    const QString oldKey = m_referenceMarkerKey;
    m_referenceMarkerKey = key;

    if (m_settings)
        m_settings->setValue(QStringLiteral("plot/referenceMarkerKey"), m_referenceMarkerKey);

    emit referenceMarkerKeyChanged(oldKey, m_referenceMarkerKey);
}

} // namespace FlySight
