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

QVariant SessionData::getAttribute(const QString &key) const {
    if (hasAttribute(key)) {
        return m_attributes.value(key);
    }

    // If not directly stored, try to compute it from a calculated attribute
    return computeAttribute(key);
}

void SessionData::setAttribute(const QString &key, const QVariant &value) {
    m_attributes.insert(key, value);
}

QStringList SessionData::sensorKeys() const {
    const QString sessionId = m_attributes.value(SessionKeys::SessionId).toString();
    return SensorDataStore::instance().sensorKeys(sessionId);
}

bool SessionData::hasSensor(const QString &key) const {
    const QString sessionId = m_attributes.value(SessionKeys::SessionId).toString();
    return SensorDataStore::instance().hasSensor(sessionId, key);
}

QStringList SessionData::measurementKeys(const QString &sensorKey) const {
    const QString sessionId = m_attributes.value(SessionKeys::SessionId).toString();
    return SensorDataStore::instance().measurementKeys(sessionId, sensorKey);
}

bool SessionData::hasMeasurement(const QString &sensorKey, const QString &measurementKey) const {
    const QString sessionId = m_attributes.value(SessionKeys::SessionId).toString();
    return SensorDataStore::instance().hasMeasurement(sessionId, sensorKey, measurementKey);
}

QVector<double> SessionData::getMeasurement(const QString &sensorKey, const QString &measurementKey) const {
    if (hasMeasurement(sensorKey, measurementKey)) {
        const QString sessionId = m_attributes.value(SessionKeys::SessionId).toString();
        return SensorDataStore::instance().getMeasurement(sessionId, sensorKey, measurementKey);
    }

    // If not directly stored, try to compute it from a calculated measurement
    return computeMeasurement(sensorKey, measurementKey);
}

void SessionData::setMeasurement(const QString &sensorKey, const QString &measurementKey, const QVector<double> &data) {
    const QString sessionId = m_attributes.value(SessionKeys::SessionId).toString();
    SensorDataStore::instance().setMeasurement(sessionId, sensorKey, measurementKey, data);
}

void SessionData::registerCalculatedAttribute(const QString &key, AttributeFunction func) {
    CalculatedValue<QString, QVariant>::registerCalculation(key, func);
}

void SessionData::registerCalculatedMeasurement(const QString &sensorKey, const QString &measurementKey, MeasurementFunction func) {
    MeasurementKey key(sensorKey, measurementKey);
    CalculatedValue<MeasurementKey, QVector<double>>::registerCalculation(key, func);
}

QVariant SessionData::computeAttribute(const QString &key) const {
    auto result = m_calculatedAttributes.getValue(*const_cast<SessionData*>(this), key);
    if (!result.has_value()) {
        // handle failure
        return QVariant();
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
