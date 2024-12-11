#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QMap>
#include <QString>
#include <QVector>

namespace FlySight {

namespace SessionKeys {
constexpr char DeviceId[] = "DEVICE_ID";
constexpr char SessionId[] = "SESSION_ID";
constexpr char Visible[] = "_VISIBLE";
constexpr char Description[] = "_DESCRIPTION";
}

class SessionData {
public:
    // Types
    using SensorData = QMap<QString, QVector<double>>;

    // Constructors
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

    // Measurement getters/setters
    QStringList measurementKeys(const QString &sensorKey) const;
    bool hasMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getMeasurement(const QString& sensorKey, const QString& measurementKey) const;
    void setMeasurement(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

    // Calculated value getters/setters
    bool hasCalculatedValue(const QString& sensorKey, const QString& measurementKey) const;
    QVector<double> getCalculatedValue(const QString& sensorKey, const QString& measurementKey) const;
    void setCalculatedValue(const QString& sensorKey, const QString& measurementKey, const QVector<double>& data);

private:
    // Member variables
    QMap<QString, QString> m_vars;
    QMap<QString, SensorData> m_sensors;
    QMap<QString, SensorData> m_calculatedValues;

    // Friend class to allow FSDataImporter to access private members
    friend class DataImporter;
};

} // namespace FlySight

#endif // SESSIONDATA_H
