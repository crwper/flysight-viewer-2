#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QMap>
#include <QString>
#include <QVector>
#include <QSet>
#include <functional>

namespace FlySight {

namespace SessionKeys {
constexpr char DeviceId[] = "DEVICE_ID";
constexpr char SessionId[] = "SESSION_ID";
constexpr char Visible[] = "_VISIBLE";
constexpr char Description[] = "_DESCRIPTION";
constexpr char Time[] = "_time";
constexpr char TimeFitA[] = "_TIME_FIT_A";
constexpr char TimeFitB[] = "_TIME_FIT_B";
}

class SessionData {
public:
    // Types
    using SensorData = QMap<QString, QVector<double>>;

    struct CalculationKey {
        QString sensorID;
        QString measurementID;
        bool operator==(const CalculationKey &other) const {
            return sensorID == other.sensorID && measurementID == other.measurementID;
        }
    };
    using CalculationFunction = std::function<QVector<double>(SessionData&)>;
    static void registerCalculatedValue(const QString& sensorID, const QString& measurementID, CalculationFunction func);

    SessionData() = default;

    // Accessors for visibility
    bool isVisible() const;
    void setVisible(bool visible);

    // Var getters/setters
    QStringList varKeys() const;
    bool hasVar(const QString &key) const;
    QString getVar(const QString &key) const;
    void setVar(const QString &key, const QString &value);

    // Sensor data getters/setters
    QStringList sensorKeys() const;
    bool hasSensor(const QString &key) const;
    QStringList measurementKeys(const QString &sensorKey) const;
    bool hasMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    void setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

private:
    QMap<QString, QString> m_vars;
    QMap<QString, SensorData> m_sensors;

    mutable QMap<QString, SensorData> m_calculatedValues;
    mutable QSet<CalculationKey> m_activeCalculations;

    // Calculated value getters/setters
    bool hasCalculatedValue(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getCalculatedValue(const QString& sensorKey, const QString& measurementKey) const;
    void setCalculatedValue(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data) const;

    QVector<double> computeMeasurement(const QString &sensorID, const QString &measurementID) const;
    static QMap<QString, QMap<QString, CalculationFunction>> s_calculations;

    friend class DataImporter; // Restored so that import.cpp can access private members as before
};

// Define qHash for CalculationKey
inline uint qHash(const SessionData::CalculationKey &key, uint seed = 0) {
    return ::qHash(key.sensorID, seed) ^ (::qHash(key.measurementID, seed) + 0x9e3779b9U);
}

} // namespace FlySight

#endif // SESSIONDATA_H
