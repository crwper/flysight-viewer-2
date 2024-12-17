#include "sessiondata.h"
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>

namespace FlySight {

bool SessionData::isVisible() const {
    return m_vars.value(SessionKeys::Visible, "true") == "true";
}

void SessionData::setVisible(bool visible) {
    m_vars.insert(SessionKeys::Visible, visible ? "true" : "false");
}

QStringList SessionData::varKeys() const {
    return m_vars.keys();
}

bool SessionData::hasVar(const QString &key) const {
    return m_vars.contains(key);
}

QString SessionData::getVar(const QString &key) const {
    return m_vars.value(key, QString());
}

void SessionData::setVar(const QString &key, const QString &value) {
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
    if (hasMeasurement(sensorKey, measurementKey)) {
        return m_sensors.value(sensorKey).value(measurementKey);
    }

    // If not directly stored, try to compute it from a calculated value
    return computeMeasurement(sensorKey, measurementKey);
}

void SessionData::setMeasurement(const QString &sensorKey, const QString &measurementKey, const QVector<double> &data) {
    m_sensors[sensorKey].insert(measurementKey, data);
}

void SessionData::registerCalculatedValue(const QString &sensorID, const QString &measurementID, CalculationFunction func) {
    MeasurementKey key(sensorID, measurementID);
    CalculatedValue<MeasurementKey, QVector<double>>::registerCalculation(key, func);
}

QVector<double> SessionData::computeMeasurement(const QString &sensorID, const QString &measurementID) const {
    MeasurementKey key(sensorID, measurementID);

    if (!m_calculatedMeasurements.hasCalculation(key)) {
        qWarning() << "Calculated value not registered for" << sensorID << "/" << measurementID;
        return QVector<double>();
    }

    // Compute and return the calculated measurement
    return m_calculatedMeasurements.getValue(*const_cast<SessionData*>(this), key);
}

} // namespace FlySight
