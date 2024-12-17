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
    if (hasVar(key)) {
        return m_vars.value(key);
    }

    // If not directly stored, try to compute it from a calculated var
    return computeVar(key);
}

void SessionData::setVar(const QString &key, const QString &value) {
    m_vars.insert(key, value);
}

QStringList SessionData::sensorKeys() const {
    return m_sensors.keys();
}

bool SessionData::hasSensor(const QString &sensorKey) const {
    return m_sensors.contains(sensorKey);
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

    // If not directly stored, try to compute it from a calculated measurement
    return computeMeasurement(sensorKey, measurementKey);
}

void SessionData::setMeasurement(const QString &sensorKey, const QString &measurementKey, const QVector<double> &data) {
    m_sensors[sensorKey].insert(measurementKey, data);
}

void SessionData::registerCalculatedVar(const QString &key, VarFunction func) {
    CalculatedValue<QString, QString>::registerCalculation(key, func);
}

void SessionData::registerCalculatedMeasurement(const QString &sensorKey, const QString &measurementKey, MeasurementFunction func) {
    MeasurementKey key(sensorKey, measurementKey);
    CalculatedValue<MeasurementKey, QVector<double>>::registerCalculation(key, func);
}

QString SessionData::computeVar(const QString &key) const {
    auto result = m_calculatedVars.getValue(*const_cast<SessionData*>(this), key);
    if (!result.has_value()) {
        // handle failure
        return QString();
    }
    return result.value();
}

QVector<double> SessionData::computeMeasurement(const QString &sensorKey, const QString &measurementKey) const {
    MeasurementKey k(sensorKey, measurementKey);
    auto result = m_calculatedMeasurements.getValue(*const_cast<SessionData*>(this), k);
    if (!result.has_value()) {
        // handle failure
        return QVector<double>();
    }
    return result.value();
}

} // namespace FlySight
