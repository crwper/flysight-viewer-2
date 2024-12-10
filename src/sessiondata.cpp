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

QMap<QString, QMap<QString, QVector<double>>>& SessionData::getSensors() {
    return sensors;
}

const QMap<QString, QMap<QString, QVector<double>>>& SessionData::getSensors() const {
    return sensors;
}

QMap<QString, QMap<QString, QVector<double>>>& SessionData::getCalculatedValues() {
    return calculatedValues;
}

const QMap<QString, QMap<QString, QVector<double>>>& SessionData::getCalculatedValues() const {
    return calculatedValues;
}

QMap<QString, QVector<double>>& SessionData::operator[](const QString& sensorName) {
    return sensors[sensorName];
}

QMap<QString, QVector<double>> SessionData::operator[](const QString& sensorName) const {
    return sensors.value(sensorName);
}

void SessionData::setSensorMeasurement(const QString& sensorName, const QString& measurementKey, const QVector<double>& data) {
    sensors[sensorName].insert(measurementKey, data);
}

void SessionData::setCalculatedValue(const QString& sensorID, const QString& measurementID, const QVector<double>& data) {
    calculatedValues[sensorID][measurementID] = data;
}

} // namespace FlySight
