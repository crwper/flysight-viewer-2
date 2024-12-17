#include "sessiondata.h"
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>

namespace FlySight {

bool SessionData::isVisible() const {
    return m_attributes.value(SessionKeys::Visible, "true") == "true";
}

void SessionData::setVisible(bool visible) {
    m_attributes.insert(SessionKeys::Visible, visible ? "true" : "false");
}

QStringList SessionData::attributeKeys() const {
    return m_attributes.keys();
}

bool SessionData::hasAttribute(const QString &key) const {
    return m_attributes.contains(key);
}

QString SessionData::getAttribute(const QString &key) const {
    if (hasAttribute(key)) {
        return m_attributes.value(key);
    }

    // If not directly stored, try to compute it from a calculated attribute
    return computeAttribute(key);
}

void SessionData::setAttribute(const QString &key, const QString &value) {
    m_attributes.insert(key, value);
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

    // If not directly stored, try to compute it from a calculated measurement
    return computeMeasurement(sensorKey, measurementKey);
}

void SessionData::setMeasurement(const QString &sensorKey, const QString &measurementKey, const QVector<double> &data) {
    m_sensors[sensorKey].insert(measurementKey, data);
}

void SessionData::registerCalculatedAttribute(const QString &key, AttributeFunction func) {
    CalculatedValue<QString, QString>::registerCalculation(key, func);
}

void SessionData::registerCalculatedMeasurement(const QString &sensorKey, const QString &measurementKey, MeasurementFunction func) {
    MeasurementKey key(sensorKey, measurementKey);
    CalculatedValue<MeasurementKey, QVector<double>>::registerCalculation(key, func);
}

QString SessionData::computeAttribute(const QString &key) const {
    auto result = m_calculatedAttributes.getValue(*const_cast<SessionData*>(this), key);
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
