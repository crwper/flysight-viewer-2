// sessiondata.cpp

#include "sessiondata.h"
#include <stdexcept>
#include <QDataStream>

const QMap<QString, QString>& SessionData::getVars() const {
    return vars;
}

const QMap<QString, SessionData::SensorData>& SessionData::getSensors() const {
    return sensors;
}

const SessionData::SensorData& SessionData::operator[](const QString& sensorName) const {
    // Check sensors first
    auto it = sensors.find(sensorName);
    if (it != sensors.end()) {
        return it.value();
    } else {
        // If not found, throw an exception or handle it appropriately
        throw std::out_of_range("Sensor name not found");
    }
}

SessionData::SensorData& SessionData::operator[](const QString& sensorName) {
    return sensors[sensorName];
}
