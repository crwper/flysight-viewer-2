#include "calculatedvaluemanager.h"
#include <QDebug>

namespace FlySight {

void CalculatedValueManager::registerCalculatedValue(const QString &sensorID, const QString &measurementID, CalculationFunction func)
{
    m_calculations[sensorID][measurementID] = func;
}

QVector<double> CalculatedValueManager::getMeasurement(SessionData& session, const QString& sensorID, const QString& measurementID)
{
    // Check if the measurement exists in sensors
    if (session.hasMeasurement(sensorID, measurementID)) {
        return session.getMeasurement(sensorID, measurementID);
    }

    // Check if the calculated value is already present
    if (session.hasCalculatedValue(sensorID, measurementID)) {
        return session.getCalculatedValue(sensorID, measurementID);
    }

    // Check if a calculation is registered
    if (!m_calculations.contains(sensorID) ||
        !m_calculations[sensorID].contains(measurementID)) {
        qWarning() << "Calculated value not registered for" << sensorID << "/" << measurementID;
        return QVector<double>();
    }

    // Create a unique key for cycle detection
    QString key = sensorID + "/" + measurementID;

    // Check for circular dependency
    if (m_activeCalculations.contains(key)) {
        qWarning() << "Circular dependency detected while calculating" << key;
        return QVector<double>();
    }

    // Mark this calculation as active
    m_activeCalculations.insert(key);

    // Perform the calculation
    CalculationFunction func = m_calculations[sensorID][measurementID];
    QVector<double> calculatedData = func(session, *this);

    // Unmark this calculation
    m_activeCalculations.remove(key);

    // Check if calculation was successful
    if (!calculatedData.isEmpty()) {
        // Cache the result
        session.setCalculatedValue(sensorID, measurementID, calculatedData);
        qDebug() << "Calculated and cached value for" << sensorID << "/" << measurementID;
    } else {
        qWarning() << "Calculated data is empty for" << sensorID << "/" << measurementID;
    }

    return calculatedData;
}

} // namespace FlySight
