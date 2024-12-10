// sessiondata.cpp

#include "sessiondata.h"

namespace FlySight {

// Initialize the static constants
const QString SessionData::DEFAULT_DEVICE_ID = "_";

QStringList SessionData::varKeys() const {
    return m_vars.keys();
}

bool SessionData::hasVar(const QString &key) const {
    return m_vars.contains(key);
}

QString SessionData::getVar(const QString &key) const {
    return m_vars.value(key, QString());
}

void SessionData::setVar(const QString& key, const QString& value) {
    m_vars.insert(key, value);
}

QStringList SessionData::sensorKeys() const {
    return m_sensors.keys();
}

bool SessionData::hasSensor(const QString &key) const {
    return m_sensors.contains(key);
}

QStringList SessionData::measurementKeys(const QString &sensorKey) const {
    if (!m_sensors.contains(sensorKey)) return QStringList();
    return m_sensors.value(sensorKey).keys();
}

bool SessionData::hasMeasurement(const QString &sensorKey, const QString &measurementKey) const {
    auto sensorIt = m_sensors.find(sensorKey);
    if (sensorIt == m_sensors.end()) return false;
    return sensorIt.value().contains(measurementKey);
}

QVector<double> SessionData::getMeasurement(const QString &sensorKey, const QString &measurementKey) const {
    if (!hasMeasurement(sensorKey, measurementKey))
        return QVector<double>();
    return m_sensors.value(sensorKey).value(measurementKey);
}

void SessionData::setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data) {
    m_sensors[sensorKey].insert(measurementKey, data);
}

QMap<QString, QMap<QString, QVector<double>>>& SessionData::getCalculatedValues() {
    return calculatedValues;
}

const QMap<QString, QMap<QString, QVector<double>>>& SessionData::getCalculatedValues() const {
    return calculatedValues;
}

void SessionData::setCalculatedValue(const QString& sensorID, const QString& measurementID, const QVector<double>& data) {
    calculatedValues[sensorID][measurementID] = data;
}

} // namespace FlySight
