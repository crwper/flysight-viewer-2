#ifndef SESSIONDATA_H
#define SESSIONDATA_H

#include <QMap>
#include <QString>
#include <QVector>

namespace FSImport {
    class FSDataImporter;
}

class SessionData {
public:
    // Types
    using SensorData = QMap<QString, QVector<double>>;

    // Constructors
    SessionData() = default;

    // Accessors for vars and sensors
    QMap<QString, QString>& getVars();
    const QMap<QString, QString>& getVars() const;

    QMap<QString, QMap<QString, QVector<double>>>& getSensors();
    const QMap<QString, QMap<QString, QVector<double>>>& getSensors() const;

    // Operator[] to access sensor data
    QMap<QString, QVector<double>>& operator[](const QString& sensorName);
    QMap<QString, QVector<double>> operator[](const QString& sensorName) const;

    // Setter methods for encapsulation
    void setVar(const QString& key, const QString& value);
    void setSensorMeasurement(const QString& sensorName, const QString& measurementKey, const QVector<double>& data);

private:
    // Member variables
    QMap<QString, QString> vars;
    QMap<QString, SensorData> sensors;

    // Friend class to allow FSDataImporter to access private members
    friend class FSImport::FSDataImporter;
};

#endif // SESSIONDATA_H
