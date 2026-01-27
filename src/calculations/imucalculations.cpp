#include "imucalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <cmath>

using namespace FlySight;

void Calculations::registerImuCalculations()
{
    // IMU total acceleration (aTotal)
    SessionData::registerCalculatedMeasurement(
        "IMU", "aTotal",
        {
            DependencyKey::measurement("IMU", "ax"),
            DependencyKey::measurement("IMU", "ay"),
            DependencyKey::measurement("IMU", "az")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> ax = session.getMeasurement("IMU", "ax");
        QVector<double> ay = session.getMeasurement("IMU", "ay");
        QVector<double> az = session.getMeasurement("IMU", "az");

        if (ax.isEmpty() || ay.isEmpty() || az.isEmpty()) {
            qWarning() << "Cannot calculate aTotal due to missing ax, ay, or az";
            return std::nullopt;
        }

        if ((ax.size() != ay.size()) || (ax.size() != az.size())) {
            qWarning() << "az, ay, or az size mismatch in session:" << session.getAttribute("_SESSION_ID");
            return std::nullopt;
        }

        QVector<double> aTotal;
        aTotal.reserve(ax.size());
        for(int i = 0; i < ax.size(); ++i){
            aTotal.append(std::sqrt(ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]));
        }
        return aTotal;
    });

    // IMU total angular velocity (wTotal)
    SessionData::registerCalculatedMeasurement(
        "IMU", "wTotal",
        {
            DependencyKey::measurement("IMU", "wx"),
            DependencyKey::measurement("IMU", "wy"),
            DependencyKey::measurement("IMU", "wz")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> wx = session.getMeasurement("IMU", "wx");
        QVector<double> wy = session.getMeasurement("IMU", "wy");
        QVector<double> wz = session.getMeasurement("IMU", "wz");

        if (wx.isEmpty() || wy.isEmpty() || wz.isEmpty()) {
            qWarning() << "Cannot calculate wTotal due to missing wx, wy, or wz";
            return std::nullopt;
        }

        if ((wx.size() != wy.size()) || (wx.size() != wz.size())) {
            qWarning() << "wz, wy, or wz size mismatch in session:" << session.getAttribute("_SESSION_ID");
            return std::nullopt;
        }

        QVector<double> wTotal;
        wTotal.reserve(wx.size());
        for(int i = 0; i < wx.size(); ++i){
            wTotal.append(std::sqrt(wx[i]*wx[i] + wy[i]*wy[i] + wz[i]*wz[i]));
        }
        return wTotal;
    });
}
