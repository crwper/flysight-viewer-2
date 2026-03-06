#include "altitudemarkerfeature.h"
#include "calculatedvalue.h"
#include "sessionmodel.h"
#include "sessiondata.h"
#include "dependencykey.h"
#include "markerregistry.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
#include <QColor>
#include <algorithm>

using namespace FlySight;

AltitudeMarkerManager::AltitudeMarkerManager(SessionModel *sessionModel, QObject *parent)
    : QObject(parent)
    , m_sessionModel(sessionModel)
{
    // Register altitude-marker preferences with their defaults
    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.registerPreference(PreferenceKeys::AltitudeMarkersUnits, QStringLiteral("Imperial"));
    prefs.registerPreference(PreferenceKeys::AltitudeMarkersColor, QColor(0x87, 0xCE, 0xEB));
    prefs.registerPreference(PreferenceKeys::AltitudeMarkersSize, 3);
    prefs.registerPreference(PreferenceKeys::AltitudeMarkersVersion, 0);
    prefs.registerPreference(PreferenceKeys::altitudeMarkerValueKey(1), 300);
    prefs.registerPreference(PreferenceKeys::altitudeMarkerValueKey(2), 600);
    prefs.registerPreference(PreferenceKeys::altitudeMarkerValueKey(3), 900);

    // Trigger refresh() whenever any altitude-marker preference changes
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, [this](const QString &key, const QVariant &) {
        if (key.startsWith(QStringLiteral("altitudeMarkers/"))) {
            refresh();
        }
    });
}

void AltitudeMarkerManager::registerAll()
{
    // Step 1: Read current preferences
    PreferencesManager &prefs = PreferencesManager::instance();
    QString units    = prefs.getValue(PreferenceKeys::AltitudeMarkersUnits).toString();
    QColor  color    = prefs.getValue(PreferenceKeys::AltitudeMarkersColor).value<QColor>();
    bool    isImperial = (units == QStringLiteral("Imperial"));

    // Step 2: Read altitude array using QSettings directly
    QSettings settings;
    int count = settings.beginReadArray(QStringLiteral("altitudeMarkers"));
    QList<int> altitudes;
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        altitudes.append(settings.value(QStringLiteral("value")).toInt());
    }
    settings.endArray();

    // Fall back to defaults if no array entries found
    if (altitudes.isEmpty()) {
        altitudes = {300, 600, 900};
    }

    // Sort ascending so markers appear in order in the dock
    std::sort(altitudes.begin(), altitudes.end());

    // Step 3: Build unit labels
    QString unitSuffix = isImperial ? QStringLiteral("FT") : QStringLiteral("M");
    QString unitLabel  = isImperial ? QStringLiteral("ft") : QStringLiteral("m");

    // Step 4 & 5: For each altitude, register a calculated attribute and build a MarkerDefinition
    QVector<MarkerDefinition> defs;
    for (int value : altitudes) {
        QString attributeKey = QStringLiteral("_ALTITUDE_%1_%2").arg(value).arg(unitSuffix);
        QString displayName  = QStringLiteral("%1 %2 AGL").arg(value).arg(unitLabel);
        QString shortLabel   = QStringLiteral("%1%2").arg(value).arg(unitLabel);

        // Convert threshold to SI metres at registration time (captured by lambda)
        double thresholdMetres = isImperial ? value * 0.3048 : static_cast<double>(value);

        // Register the calculated attribute lambda
        SessionData::registerCalculatedAttribute(
            attributeKey,
            {
                DependencyKey::attribute(SessionKeys::AnalysisStartTime),
                DependencyKey::attribute(SessionKeys::AnalysisEndTime),
                DependencyKey::measurement("GNSS", "z"),
                DependencyKey::measurement("GNSS", SessionKeys::Time)
            },
            [thresholdMetres](SessionData &session) -> std::optional<QVariant> {
                // Retrieve analysis window bounds
                QVariant asVar = session.getAttribute(SessionKeys::AnalysisStartTime);
                if (!asVar.canConvert<double>())
                    return std::nullopt;
                double analysisStartSec = asVar.toDouble();

                QVariant aeVar = session.getAttribute(SessionKeys::AnalysisEndTime);
                if (!aeVar.canConvert<double>())
                    return std::nullopt;
                double analysisEndSec = aeVar.toDouble();

                // Retrieve GNSS altitude AGL and time vectors
                QVector<double> z    = session.getMeasurement("GNSS", "z");
                QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

                if (z.isEmpty() || time.isEmpty() || z.size() != time.size())
                    return std::nullopt;

                // Find the last downward crossing of thresholdMetres within the analysis window
                double lastCrossingTime = -1.0;
                bool   foundCrossing    = false;

                for (int i = 1; i < z.size(); ++i) {
                    if (time[i] < analysisStartSec) continue;
                    if (time[i - 1] > analysisEndSec) break;

                    // Downward crossing: z[i-1] >= threshold AND z[i] < threshold
                    if (z[i - 1] >= thresholdMetres && z[i] < thresholdMetres) {
                        // Linear interpolation to find precise crossing time
                        double t_cross = time[i - 1]
                            + (thresholdMetres - z[i - 1]) / (z[i] - z[i - 1])
                            * (time[i] - time[i - 1]);
                        lastCrossingTime = t_cross;
                        foundCrossing    = true;
                    }
                }

                if (!foundCrossing)
                    return std::nullopt;

                return QVariant(lastCrossingTime);
            });

        // Build the MarkerDefinition
        MarkerDefinition def;
        def.category     = QStringLiteral("Altitude");
        def.displayName  = displayName;
        def.shortLabel   = shortLabel;
        def.color        = color;
        def.attributeKey = attributeKey;
        def.measurements = {};
        def.editable       = false;
        def.groupId        = QStringLiteral("altitude");
        def.defaultEnabled = true;
        defs.append(def);

        // Step 6: Write the shared marker colour so plot rendering finds it via the
        // standard per-marker key lookup (goes through PreferencesManager like all
        // other marker colour writes).
        PreferencesManager::instance().setValue(
            PreferenceKeys::markerColorKey(attributeKey),
            color);

        m_registeredKeys.append(attributeKey);
    }

    // Atomically replace the altitude group so markersChanged() fires only once
    MarkerRegistry::instance()->replaceMarkerGroup(QStringLiteral("altitude"), defs);
}

void AltitudeMarkerManager::refresh()
{
    QSet<QString> oldKeys(m_registeredKeys.begin(), m_registeredKeys.end());

    // Unregister calculated attribute recipes (global, not per-session)
    for (const QString &key : std::as_const(m_registeredKeys)) {
        CalculatedValue<QString, QVariant>::unregisterCalculation(key);
    }
    m_registeredKeys.clear();

    // Re-register with current preferences (uses atomic replaceMarkerGroup)
    registerAll();

    // Clear saved enabled state for removed markers so that re-adding
    // them later falls through to defaultEnabled = true.
    QSet<QString> newKeys(m_registeredKeys.begin(), m_registeredKeys.end());
    QSettings settings;
    for (const QString &key : oldKeys) {
        if (!newKeys.contains(key)) {
            settings.remove(QStringLiteral("state/markers/") + key);
        }
    }
}
