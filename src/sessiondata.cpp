#include "sessiondata.h"
#include "dependencykey.h"
#include "dependencymanager.h"
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
    // Store the attribute
    m_attributes.insert(key, value);

    // Invalidate dependencies
    DependencyManager::invalidateKeyAndDependents(
        DependencyKey::attribute(key),
        m_calculatedAttributes,
        m_calculatedMeasurements);
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
    // Store the measurement
    m_sensors[sensorKey].insert(measurementKey, data);

    // Invalidate dependencies
    DependencyManager::invalidateKeyAndDependents(
        DependencyKey::measurement(sensorKey, measurementKey),
        m_calculatedAttributes,
        m_calculatedMeasurements);
}

void SessionData::registerCalculatedAttribute(
    const QString& key,
    const QList<DependencyKey>& dependencies,
    AttributeFunction func)
{
    // Register the calculation function
    CalculatedValue<QString, QVariant>::registerCalculation(key, func);

    // Register the reverse dependencies
    DependencyManager::registerDependencies(DependencyKey::attribute(key), dependencies);
}

void SessionData::registerCalculatedMeasurement(
    const QString &sensorKey,
    const QString &measurementKey,
    const QList<DependencyKey>& dependencies,
    MeasurementFunction func)
{
    // Register the calculation function
    MeasurementKey key(sensorKey, measurementKey);
    CalculatedValue<MeasurementKey, QVector<double>>::registerCalculation(key, func);

    // Register the reverse dependencies
    DependencyManager::registerDependencies(
        DependencyKey::measurement(sensorKey, measurementKey),
        dependencies);
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
