#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QMap>
#include <QString>
#include <QVector>

namespace FlySight {

class FSDataImporter;

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
    QMap<QString, SensorData>& getSensors();
    const QMap<QString, SensorData>& getSensors() const;

    // Accessors for calculated values
    QMap<QString, SensorData>& getCalculatedValues();
    const QMap<QString, SensorData>& getCalculatedValues() const;

    // Operator[] to access sensor data
    SensorData& operator[](const QString& sensorName);
    SensorData operator[](const QString& sensorName) const;

    // Setter methods for encapsulation
    void setSensorMeasurement(const QString& sensorName, const QString& measurementKey, const QVector<double>& data);
    void setCalculatedValue(const QString& sensorName, const QString& measurementKey, const QVector<double>& data);

    // Static constant for default DEVICE_ID
    static const QString DEFAULT_DEVICE_ID;

private:
    // Member variables
    bool m_visible;
    QMap<QString, QString> m_vars;
    QMap<QString, SensorData> sensors;
    QMap<QString, SensorData> calculatedValues;

    // Friend class to allow FSDataImporter to access private members
    friend class DataImporter;
};

} // namespace FlySight

#endif // SESSIONDATA_H
