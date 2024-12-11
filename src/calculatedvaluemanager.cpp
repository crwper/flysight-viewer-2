#include "calculatedvaluemanager.h"
#include <QDebug>

namespace FlySight {

void CalculatedValueManager::registerCalculatedValue(const QString &sensorID, const QString &measurementID, CalculationFunction func)
{
    m_calculations[sensorID][measurementID] = func;
}

QVector<double> CalculatedValueManager::getMeasurement(SessionData &session, const QString &sensorID, const QString &measurementID)
{
    // If the measurement is already available (raw or calculated), return it
    if (session.hasMeasurement(sensorID, measurementID)) {
        return session.getMeasurement(sensorID, measurementID);
    }

    // Check if a calculation is registered
    if (!m_calculations.contains(sensorID) || !m_calculations[sensorID].contains(measurementID)) {
        qWarning() << "Calculated value not registered for" << sensorID << "/" << measurementID;
        return QVector<double>();
    }

    CalculationKey key{sensorID, measurementID};

    // Check for circular dependency
    if (m_activeCalculations.contains(key)) {
        qWarning() << "Circular dependency detected while calculating" << sensorID << "/" << measurementID;
        return QVector<double>();
    }

    // Mark active
    m_activeCalculations.insert(key, true);

    // Perform the calculation
    CalculationFunction func = m_calculations[sensorID][measurementID];
    QVector<double> calculatedData = func(session, *this);

    // Unmark
    m_activeCalculations.remove(key);

    // Cache the result if not empty
    if (!calculatedData.isEmpty()) {
        session.setCalculatedValue(sensorID, measurementID, calculatedData);
        qDebug() << "Calculated and cached value for" << sensorID << "/" << measurementID;
    } else {
        qWarning() << "Calculated data is empty for" << sensorID << "/" << measurementID;
    }

    return calculatedData;
}

} // namespace FlySight
