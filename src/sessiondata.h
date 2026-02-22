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
    constexpr char Visible[] = "_VISIBLE";
    constexpr char Description[] = "_DESCRIPTION";
    constexpr char Time[] = "_time";
    constexpr char TimeFromExit[] = "_time_from_exit";
    constexpr char TimeFitA[] = "_TIME_FIT_A";
    constexpr char TimeFitB[] = "_TIME_FIT_B";
    constexpr char ExitTime[] = "_EXIT_TIME";
    constexpr char StartTime[] = "_START_TIME";
    constexpr char Duration[] = "_DURATION";
    constexpr char GroundElev[] = "_GROUND_ELEV";
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
    void setAttribute(const QString &key, const QVariant &value);

    QStringList sensorKeys() const;
    bool hasSensor(const QString &key) const;
    QStringList measurementKeys(const QString &sensorKey) const;
    bool hasMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    void setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);
    void setCalculatedMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

    static void registerCalculatedAttribute(const QString &key,
                                            const QList<DependencyKey>& dependencies, AttributeFunction func);
    static void registerCalculatedMeasurement(const QString &sensorKey, const QString &measurementKey,
                                              const QList<DependencyKey>& dependencies, MeasurementFunction func);

    void addDependencies(const DependencyKey& thisKey,
                         const QList<DependencyKey>& deps) {
        m_dependencyManager.registerDependencies(thisKey, deps);
    }
private:
    QMap<QString, QVariant> m_attributes;
    QMap<QString, QMap<QString, QVector<double>>> m_sensors;

    CalculatedValue<QString, QVariant> m_calculatedAttributes;
    CalculatedValue<MeasurementKey, QVector<double>> m_calculatedMeasurements;

    DependencyManager m_dependencyManager;

    QVariant computeAttribute(const QString &key) const;
    QVector<double> computeMeasurement(const QString &sensorKey, const QString &measurementKey) const;

    friend class DataImporter;
};

} // namespace FlySight

#endif // SESSIONDATA_H
