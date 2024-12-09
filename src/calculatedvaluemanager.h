#ifndef CALCULATEDVALUEMANAGER_H
#define CALCULATEDVALUEMANAGER_H

#include <QMap>
#include <QSet>
#include <QString>
#include <functional>
#include "sessiondata.h"

namespace FlySight {

class CalculatedValueManager
{
public:
    using CalculationFunction = std::function<QVector<double>(SessionData&, CalculatedValueManager&)>;

    // Registers a calculated value with sensorID, measurementID, and calculation function
    void registerCalculatedValue(const QString& sensorID, const QString& measurementID, CalculationFunction func);

    // Retrieves a calculated value for a given session, sensorID, and measurementID
    // Computes it on demand and caches the result in SessionData::calculatedValues
    QVector<double> getMeasurement(SessionData& session, const QString& sensorID, const QString& measurementID);

    // Clears the cache within SessionData (optional)
    void clearCache(SessionData& session);

private:
    // Map of sensorID -> measurementID -> function
    QMap<QString, QMap<QString, CalculationFunction>> m_calculations;

    // Set to track active calculations for cycle detection
    QSet<QString> m_activeCalculations;
};

} // namespace FlySight

#endif // CALCULATEDVALUEMANAGER_H
