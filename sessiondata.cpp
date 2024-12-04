// sessiondata.cpp

#include "sessiondata.h"

QMap<QString, QString>& SessionData::getVars() {
    return vars;
}

const QMap<QString, QString>& SessionData::getVars() const {
    return vars;
}

QMap<QString, QMap<QString, QVector<double>>>& SessionData::getSensors() {
    return sensors;
}

const QMap<QString, QMap<QString, QVector<double>>>& SessionData::getSensors() const {
    return sensors;
}

QMap<QString, QVector<double>>& SessionData::operator[](const QString& sensorName) {
    return sensors[sensorName];
}

QMap<QString, QVector<double>> SessionData::operator[](const QString& sensorName) const {
    return sensors.value(sensorName);
}
