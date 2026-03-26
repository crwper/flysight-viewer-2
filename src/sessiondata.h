#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QList>
#include <QMap>
#include <QString>
#include <QVector>
#include <QVariant>
#include <optional>
#include <functional>
#include "calculatedvalue.h"
#include "dependencykey.h"
#include "dependencymanager.h"

namespace FlySight {

namespace SessionKeys {
    constexpr char DeviceId[] = "DEVICE_ID";
    constexpr char SessionId[] = "SESSION_ID";
    constexpr char Description[] = "_DESCRIPTION";
    constexpr char ImportTime[] = "_IMPORT_TIME";
    constexpr char Time[] = "_time";
    constexpr char SystemTime[] = "_system_time";
    constexpr char TimeFitA[] = "_TIME_FIT_A";
    constexpr char TimeFitB[] = "_TIME_FIT_B";
    constexpr char ExitTime[] = "_EXIT_TIME";
    constexpr char SyncTime[] = "_SYNC_TIME";
    constexpr char StartTime[] = "_START_TIME";
    constexpr char Duration[] = "_DURATION";
    constexpr char GroundElev[] = "_GROUND_ELEV";
    constexpr char MaxVelDTime[] = "_MAX_VELD_TIME";
    constexpr char ManoeuvreStartTime[] = "_MANOEUVRE_START_TIME";
    constexpr char MaxVelHTime[] = "_MAX_VELH_TIME";
    constexpr char LandingTime[] = "_LANDING_TIME";

    // Flare detection keys
    constexpr char FlareStartTime[] = "_FLARE_START_TIME";
    constexpr char FlareEndTime[]   = "_FLARE_END_TIME";

    // Analysis range keys
    constexpr char AnalysisStartTime[] = "_ANALYSIS_START_TIME";
    constexpr char AnalysisEndTime[]   = "_ANALYSIS_END_TIME";

    // Wingsuit Performance (WS-P) parameter keys
    constexpr char WspVersion[]      = "_WSP_VERSION";
    constexpr char WspTopAlt[]       = "_WSP_TOP_ALT";
    constexpr char WspBottomAlt[]    = "_WSP_BOTTOM_ALT";
    constexpr char WspTask[]         = "_WSP_TASK";

    // Wingsuit Performance (WS-P) result keys
    constexpr char WspEntryTime[]    = "_WSP_ENTRY_TIME";
    constexpr char WspExitTime[]     = "_WSP_EXIT_TIME";
    constexpr char WspEntryLat[]     = "_WSP_ENTRY_LAT";
    constexpr char WspEntryLon[]     = "_WSP_ENTRY_LON";
    constexpr char WspExitLat[]      = "_WSP_EXIT_LAT";
    constexpr char WspExitLon[]      = "_WSP_EXIT_LON";
    constexpr char WspTimeResult[]   = "_WSP_TIME_RESULT";
    constexpr char WspDistResult[]   = "_WSP_DIST_RESULT";
    constexpr char WspSpeedResult[]  = "_WSP_SPEED_RESULT";
    constexpr char WspSepResult[]    = "_WSP_SEP_RESULT";

    // Wingsuit Performance (WS-P) lane reference keys
    constexpr char WspRef1Time[]     = "_WSP_REF1_TIME";
}

class SessionData {
public:
    using AttributeFunction = std::function<std::optional<QVariant>(SessionData&)>;
    using MeasurementFunction = std::function<std::optional<QVector<double>>(SessionData&)>;
    using MeasurementKey = QPair<QString, QString>;

    SessionData() = default;

    bool isVisible() const;
    void setVisible(bool visible);

    QStringList attributeKeys() const;
    bool hasAttribute(const QString &key) const;
    QVariant getAttribute(const QString &key) const;
    QSet<DependencyKey> setAttribute(const QString &key, const QVariant &value);
    QSet<DependencyKey> removeAttribute(const QString &key);

    /// Removes a dynamically-registered calculated attribute's global registration
    /// and flushes this session's cached value for that key.
    void unregisterCalculatedAttribute(const QString &key);

    QStringList sensorKeys() const;
    bool hasSensor(const QString &key) const;
    QStringList measurementKeys(const QString &sensorKey) const;
    bool hasMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    QSet<DependencyKey> setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

    void setUnit(const QString& sensorKey, const QString& measurementKey, const QString& unitString);
    QString getUnit(const QString& sensorKey, const QString& measurementKey) const;
    QMap<QString, QString> units(const QString& sensorKey) const;
    void setCalculatedAttribute(const QString &key, const QVariant &value);
    void setCalculatedMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

    static bool hasRegisteredCalculation(const QString &key);

    static void registerCalculatedAttribute(const QString &key,
                                            const QList<DependencyKey>& dependencies, AttributeFunction func);
    static void registerCalculatedMeasurement(const QString &sensorKey, const QString &measurementKey,
                                              const QList<DependencyKey>& dependencies, MeasurementFunction func);

    void addDependencies(const DependencyKey& thisKey,
                         const QList<DependencyKey>& deps) {
        m_dependencyManager.registerDependencies(thisKey, deps);
    }
private:
    bool m_visible = false;
    QMap<QString, QVariant> m_attributes;
    QMap<QString, QMap<QString, QVector<double>>> m_sensors;
    QMap<QString, QMap<QString, QString>> m_units;

    CalculatedValue<QString, QVariant> m_calculatedAttributes;
    CalculatedValue<MeasurementKey, QVector<double>> m_calculatedMeasurements;

    DependencyManager m_dependencyManager;

    QVariant computeAttribute(const QString &key) const;
    QVariant synthesizeInterpolation(const QString &key) const;

public:
    /// Builds an interpolation key: "{timeAttr}:{sensor}/{timeVector}/{dataVector}"
    static QString interpolationKey(const QString &timeAttr,
                                    const QString &sensor,
                                    const QString &timeVector,
                                    const QString &dataVector);
private:
    QVector<double> computeMeasurement(const QString &sensorKey, const QString &measurementKey) const;

    friend class DataImporter;
};

} // namespace FlySight

#endif // SESSIONDATA_H
