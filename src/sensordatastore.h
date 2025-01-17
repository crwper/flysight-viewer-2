#ifndef SENSORDATASTORE_H
#define SENSORDATASTORE_H

#include <QMap>
#include <QString>
#include <QVector>

namespace FlySight {

class SensorDataStore
{
public:
    // Simple singleton-style access
    static SensorDataStore& instance()
    {
        static SensorDataStore s_instance;
        return s_instance;
    }

    // Return all sensor keys for a given session
    QStringList sensorKeys(const QString &sessionId) const
    {
        QStringList result;
        auto sessionIt = m_data.find(sessionId);
        if (sessionIt == m_data.end())
            return result;
        // sessionIt->keys() returns the list of sensor keys
        result = sessionIt->keys();
        return result;
    }

    // Check if a given sensor exists for this session
    bool hasSensor(const QString &sessionId, const QString &sensorKey) const
    {
        auto sessionIt = m_data.find(sessionId);
        if (sessionIt == m_data.end())
            return false;
        return sessionIt->contains(sensorKey);
    }

    // Return all measurement keys (columns) for a given sensor
    QStringList measurementKeys(const QString &sessionId, const QString &sensorKey) const
    {
        QStringList result;
        auto sessionIt = m_data.find(sessionId);
        if (sessionIt == m_data.end())
            return result;

        auto sensorIt = sessionIt->find(sensorKey);
        if (sensorIt == sessionIt->end())
            return result;

        result = sensorIt->keys(); // All measurement keys
        return result;
    }

    // Check if a given measurement exists for this session
    bool hasMeasurement(const QString &sessionId,
                        const QString &sensorKey,
                        const QString &measurementKey) const
    {
        auto sessionIt = m_data.find(sessionId);
        if (sessionIt == m_data.end())
            return false;
        auto sensorIt = sessionIt->find(sensorKey);
        if (sensorIt == sessionIt->end())
            return false;
        return sensorIt->contains(measurementKey);
    }

    // Retrieve data for a given session/sensor/measurement
    QVector<double> getMeasurement(const QString &sessionId,
                                   const QString &sensorKey,
                                   const QString &measurementKey) const
    {
        auto sessionIt = m_data.find(sessionId);
        if (sessionIt == m_data.end())
            return {};

        auto sensorIt = sessionIt->find(sensorKey);
        if (sensorIt == sessionIt->end())
            return {};

        auto measIt = sensorIt->find(measurementKey);
        if (measIt == sensorIt->end())
            return {};

        return measIt.value();
    }

    // Store data for a given session/sensor/measurement
    void setMeasurement(const QString &sessionId,
                        const QString &sensorKey,
                        const QString &measurementKey,
                        const QVector<double> &data)
    {
        m_data[sessionId][sensorKey][measurementKey] = data;
    }

private:
    // Private constructor => forces `instance()` usage
    SensorDataStore() = default;

    // sessionId -> sensorKey -> measurementKey -> data
    QMap<QString, QMap<QString, QMap<QString, QVector<double>>>> m_data;
};

} // namespace FlySight

#endif // SENSORDATASTORE_H
