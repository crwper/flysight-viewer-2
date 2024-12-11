#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QMap>
#include <QString>
#include <QVector>

namespace FlySight {

class SessionData {
public:
    // Types
    using SensorData = QMap<QString, QVector<double>>;

    // Constructors
    SessionData() = default;

    // Accessors for visibility
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }

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

    // Static constant for default DEVICE_ID
    static const QString DEFAULT_DEVICE_ID;

private:
    // Member variables
    bool m_visible;
    QMap<QString, QString> m_vars;
    QMap<QString, SensorData> m_sensors;
    QMap<QString, SensorData> m_calculatedValues;

    // Friend class to allow FSDataImporter to access private members
    friend class DataImporter;
};

} // namespace FlySight

#endif // SESSIONDATA_H
