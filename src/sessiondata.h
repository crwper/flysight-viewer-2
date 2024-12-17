#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QSet>
#include <functional>
#include "calculatedvalue.h"

namespace FlySight {

namespace SessionKeys {
constexpr char DeviceId[] = "DEVICE_ID";
constexpr char SessionId[] = "SESSION_ID";
constexpr char Visible[] = "_VISIBLE";
constexpr char Description[] = "_DESCRIPTION";
constexpr char Time[] = "_time";
constexpr char TimeFitA[] = "_TIME_FIT_A";
constexpr char TimeFitB[] = "_TIME_FIT_B";
}

class SessionData {
public:
    using CalculationFunction = std::function<QVector<double>(SessionData&)>;
    using MeasurementKey = QPair<QString, QString>;

    SessionData() = default;

    bool isVisible() const;
    void setVisible(bool visible);

    QStringList varKeys() const;
    bool hasVar(const QString &key) const;
    QString getVar(const QString &key) const;
    void setVar(const QString &key, const QString &value);

    QStringList sensorKeys() const;
    bool hasSensor(const QString &key) const;
    QStringList measurementKeys(const QString &sensorKey) const;
    bool hasMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    void setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

    static void registerCalculatedValue(const QString &sensorID, const QString &measurementID, CalculationFunction func);

private:
    QMap<QString, QString> m_vars;
    QMap<QString, QMap<QString, QVector<double>>> m_sensors;

    mutable CalculatedValue<MeasurementKey, QVector<double>> m_calculatedMeasurements;

    QVector<double> computeMeasurement(const QString &sensorID, const QString &measurementID) const;

    friend class DataImporter;
};

} // namespace FlySight

#endif // SESSIONDATA_H
