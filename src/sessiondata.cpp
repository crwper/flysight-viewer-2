#include "sessiondata.h"

#include <QDebug>
#include <QDateTime>
#include <QCryptographicHash>

QMap<QString, QMap<QString, FlySight::SessionData::CalculationFunction>> FlySight::SessionData::s_calculations;

namespace FlySight {

// Accessors for visibility
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
    if (hasMeasurement(sensorKey, measurementKey)) {
        return m_sensors.value(sensorKey).value(measurementKey);
    }

    if (hasCalculatedValue(sensorKey, measurementKey)) {
        return getCalculatedValue(sensorKey, measurementKey);
    }

    return computeMeasurement(sensorKey, measurementKey);
}

void SessionData::setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data) {
    m_sensors[sensorKey].insert(measurementKey, data);
}

bool SessionData::hasCalculatedValue(const QString &sensorKey, const QString &measurementKey) const {
    auto calculatedValueIt = m_calculatedValues.find(sensorKey);
    if (calculatedValueIt == m_calculatedValues.end()) return false;
    return calculatedValueIt.value().contains(measurementKey);
}

QVector<double> SessionData::getCalculatedValue(const QString &sensorKey, const QString &measurementKey) const {
    if (!hasCalculatedValue(sensorKey, measurementKey))
        return QVector<double>();
    return m_calculatedValues.value(sensorKey).value(measurementKey);
}

void SessionData::setCalculatedValue(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data) const {
    m_calculatedValues[sensorKey].insert(measurementKey, data);
}

QVector<double> SessionData::computeMeasurement(const QString &sensorID, const QString &measurementID) const {
    CalculationKey key{sensorID, measurementID};

    if (m_activeCalculations.contains(key)) {
        qWarning() << "Circular dependency detected while calculating" << sensorID << "/" << measurementID;
        return QVector<double>();
    }

    if (!s_calculations.contains(sensorID) || !s_calculations[sensorID].contains(measurementID)) {
        qWarning() << "Calculated value not registered for" << sensorID << "/" << measurementID;
        return QVector<double>();
    }

    m_activeCalculations.insert(key);

    CalculationFunction func = s_calculations[sensorID][measurementID];
    QVector<double> calculatedData = func(const_cast<SessionData&>(*this));

    m_activeCalculations.remove(key);

    if (!calculatedData.isEmpty()) {
        setCalculatedValue(sensorID, measurementID, calculatedData);
        qDebug() << "Calculated and cached value for" << sensorID << "/" << measurementID;
    } else {
        qWarning() << "Calculated data is empty for" << sensorID << "/" << measurementID;
    }

    return calculatedData;
}

void SessionData::registerCalculatedValue(const QString &sensorID, const QString &measurementID, CalculationFunction func) {
    s_calculations[sensorID][measurementID] = func;
}

} // namespace FlySight
