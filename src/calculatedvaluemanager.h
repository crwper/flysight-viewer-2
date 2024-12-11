#ifndef CALCULATEDVALUEMANAGER_H
#define CALCULATEDVALUEMANAGER_H

#include <QMap>
#include <QSet>
#include <QHash>
#include <QString>
#include <QVector>
#include <functional>
#include "sessiondata.h"

namespace FlySight {

struct CalculationKey {
    QString sensorID;
    QString measurementID;

    bool operator==(const CalculationKey &other) const {
        return sensorID == other.sensorID && measurementID == other.measurementID;
    }
};

// Define qHash for CalculationKey in global namespace
inline uint qHash(const CalculationKey &key, uint seed = 0) {
    // Combine the hashes of the sensorID and measurementID
    return ::qHash(key.sensorID, seed) ^ (::qHash(key.measurementID, seed) + 0x9e3779b9U);
}

class CalculatedValueManager
{
public:
    using CalculationFunction = std::function<QVector<double>(SessionData&, CalculatedValueManager&)>;

    // Registers a calculated value with sensorID, measurementID, and calculation function
    void registerCalculatedValue(const QString& sensorID, const QString& measurementID, CalculationFunction func);

    // Retrieves a calculated value for a given session, sensorID, and measurementID
    // Computes it on demand and caches the result in SessionData::calculatedValues
    QVector<double> getMeasurement(SessionData& session, const QString& sensorID, const QString& measurementID);

private:
    // Map of sensorID -> measurementID -> function
    QMap<QString, QMap<QString, CalculationFunction>> m_calculations;

    // Hash to track active calculations for cycle detection
    QHash<CalculationKey, bool> m_activeCalculations;
};

} // namespace FlySight

#endif // CALCULATEDVALUEMANAGER_H
