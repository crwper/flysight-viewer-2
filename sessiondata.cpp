// sessiondata.cpp

#include "sessiondata.h"

// Initialize the static constants
const QString SessionData::DEFAULT_DEVICE_ID = "_";

QMap<QString, QString>& SessionData::getVars() {
    return vars;
}

const QMap<QString, QString>& SessionData::getVars() const {
    return vars;
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

void SessionData::setVar(const QString& key, const QString& value) {
    vars.insert(key, value);
}

void SessionData::setSensorMeasurement(const QString& sensorName, const QString& measurementKey, const QVector<double>& data) {
    sensors[sensorName].insert(measurementKey, data);
}

void SessionData::setCalculatedValue(const QString& sensorID, const QString& measurementID, const QVector<double>& data) {
    calculatedValues[sensorID][measurementID] = data;
}
