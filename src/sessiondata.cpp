#include "sessiondata.h"
#include "dependencykey.h"
#include "dependencymanager.h"
#include <algorithm>
#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>

namespace FlySight {

bool SessionData::isVisible() const {
    return m_visible;
}

void SessionData::setVisible(bool visible) {
    m_visible = visible;
}

QStringList SessionData::attributeKeys() const {
    return m_attributes.keys();
}

bool SessionData::hasAttribute(const QString &key) const {
    return m_attributes.contains(key);
}

QVariant SessionData::getAttribute(const QString &key) const {
    if (m_attributes.contains(key)) {
        return m_attributes.value(key);
    }

    // If not directly stored, try to compute it from a calculated attribute
    return computeAttribute(key);
}

QSet<DependencyKey> SessionData::setAttribute(const QString &key, const QVariant &value) {
    // Store the attribute
    m_attributes.insert(key, value);

    // Invalidate dependencies and return the set of all visited keys
    return m_dependencyManager.invalidateKeyAndDependents(
        DependencyKey::attribute(key),
        m_calculatedAttributes,
        m_calculatedMeasurements);
}

QSet<DependencyKey> SessionData::removeAttribute(const QString &key) {
    // Remove the stored attribute (no-op if key is absent)
    m_attributes.remove(key);

    // Invalidate dependencies so downstream caches recompute from the calculated value
    return m_dependencyManager.invalidateKeyAndDependents(
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

QSet<DependencyKey> SessionData::setMeasurement(const QString &sensorKey, const QString &measurementKey, const QVector<double> &data) {
    // Store the measurement
    m_sensors[sensorKey].insert(measurementKey, data);

    // Invalidate dependencies and return the set of all visited keys
    return m_dependencyManager.invalidateKeyAndDependents(
        DependencyKey::measurement(sensorKey, measurementKey),
        m_calculatedAttributes,
        m_calculatedMeasurements);
}

void SessionData::setCalculatedAttribute(const QString &key, const QVariant &value)
{
    m_calculatedAttributes.setValue(key, value);
}

void SessionData::setCalculatedMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data)
{
    MeasurementKey k(sensorKey, measurementKey);
    m_calculatedMeasurements.setValue(k, data);
}

bool SessionData::hasRegisteredCalculation(const QString &key)
{
    return CalculatedValue<QString, QVariant>::hasRegisteredCalculation(key);
}

void SessionData::unregisterCalculatedAttribute(const QString &key)
{
    // Remove from global calculation registry (affects all sessions)
    CalculatedValue<QString, QVariant>::unregisterCalculation(key);
    // Cascade invalidation through the dependency graph
    m_dependencyManager.invalidateKeyAndDependents(
        DependencyKey::attribute(key),
        m_calculatedAttributes,
        m_calculatedMeasurements);
}

void SessionData::registerCalculatedAttribute(
    const QString& key,
    const QList<DependencyKey>& dependencies,
    AttributeFunction func)
{
    // Register the calculation function
    CalculatedValue<QString, QVariant>::
        registerCalculation(key, dependencies, func);
}

void SessionData::registerCalculatedMeasurement(
    const QString &sensorKey,
    const QString &measurementKey,
    const QList<DependencyKey>& dependencies,
    MeasurementFunction func)
{
    // Register the calculation function
    MeasurementKey key(sensorKey, measurementKey);
    CalculatedValue<MeasurementKey, QVector<double>>::
        registerCalculation(key, dependencies, func);
}

QVariant SessionData::computeAttribute(const QString &key) const {
    auto result = m_calculatedAttributes.getValue(*const_cast<SessionData*>(this), key);
    if (result.has_value()) {
        return result.value();
    }
    return synthesizeInterpolation(key);
}

QVariant SessionData::synthesizeInterpolation(const QString &key) const {
    // 1. Parse the key: {timeAttr}:{sensor}/{timeVector}/{dataVector}
    const int colonPos = key.indexOf(':');
    if (colonPos < 0)
        return QVariant();

    const QString timeAttrKey = key.left(colonPos);
    const QStringList parts = key.mid(colonPos + 1).split('/');
    if (parts.size() != 3)
        return QVariant();

    const QString &sensor     = parts[0];
    const QString &timeVector = parts[1];
    const QString &dataVector = parts[2];

    // 2. Resolve the time attribute
    const QVariant timeVar = getAttribute(timeAttrKey);
    if (!timeVar.canConvert<double>())
        return QVariant();
    const double markerTime = timeVar.toDouble();

    // 3. Read the measurement vectors
    const QVector<double> timeVec = getMeasurement(sensor, timeVector);
    if (timeVec.isEmpty())
        return QVariant();

    const QVector<double> dataVec = getMeasurement(sensor, dataVector);
    if (dataVec.isEmpty() || dataVec.size() != timeVec.size())
        return QVariant();

    // 4. Binary search and linear interpolation
    //    lower_bound returns cbegin() when markerTime <= first element, and
    //    cend() when markerTime > last element. Both cases mean the query
    //    falls outside the interpolatable range (we need two bracketing
    //    points). This matches interpolateAtX() in plotutils.cpp.
    auto it = std::lower_bound(timeVec.cbegin(), timeVec.cend(), markerTime);
    if (it == timeVec.cbegin() || it == timeVec.cend())
        return QVariant();

    const int idx = static_cast<int>(std::distance(timeVec.cbegin(), it));
    const double t1 = timeVec[idx - 1], v1 = dataVec[idx - 1];
    const double t2 = timeVec[idx],     v2 = dataVec[idx];

    if (t2 == t1)
        return QVariant();

    const double result = v1 + (v2 - v1) * (markerTime - t1) / (t2 - t1);

    // 5. Cache the result
    const_cast<CalculatedValue<QString, QVariant>&>(m_calculatedAttributes).setValue(key, QVariant(result));

    // 6. Register dependency edges for DAG invalidation
    DependencyKey thisKey = DependencyKey::attribute(key);
    QList<DependencyKey> deps = {
        DependencyKey::attribute(timeAttrKey),
        DependencyKey::measurement(sensor, timeVector),
        DependencyKey::measurement(sensor, dataVector)
    };
    const_cast<SessionData*>(this)->addDependencies(thisKey, deps);

    // 7. Return the interpolated value
    return QVariant(result);
}

QString SessionData::interpolationKey(const QString &timeAttr,
                                      const QString &sensor,
                                      const QString &timeVector,
                                      const QString &dataVector)
{
    return timeAttr
        + QStringLiteral(":")
        + sensor
        + QStringLiteral("/")
        + timeVector
        + QStringLiteral("/")
        + dataVector;
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
