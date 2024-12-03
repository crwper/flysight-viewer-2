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
    const QMap<QString, QString>& getVars() const;
    const QMap<QString, SensorData>& getSensors() const;

    // Operator[] to access sensor data
    const SensorData& operator[](const QString& sensorName) const;
    SensorData& operator[](const QString& sensorName);

private:
    // Member variables
    QMap<QString, QString> vars;
    QMap<QString, SensorData> sensors;

    // Friend class to allow FSDataImporter to access private members
    friend class FSImport::FSDataImporter;
};

#endif // SESSIONDATA_H
