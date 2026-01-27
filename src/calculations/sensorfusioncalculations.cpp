#include "sensorfusioncalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include "../imugnssekf.h"
#include <QVector>
#include <cmath>
#include <vector>

using namespace FlySight;

void Calculations::registerSensorfusionCalculations()
{
    // Define output mappings
    struct FusionOutputMapping {
        QString key;
        QVector<double> FusionOutput::*member;
    };

    static const std::vector<FusionOutputMapping> fusion_outputs = {
        {SessionKeys::Time, &FusionOutput::time},
        {"posN", &FusionOutput::posN},
        {"posE", &FusionOutput::posE},
        {"posD", &FusionOutput::posD},
        {"velN", &FusionOutput::velN},
        {"velE", &FusionOutput::velE},
        {"velD", &FusionOutput::velD},
        {"accN", &FusionOutput::accN},
        {"accE", &FusionOutput::accE},
        {"accD", &FusionOutput::accD},
        {"roll", &FusionOutput::roll},
        {"pitch", &FusionOutput::pitch},
        {"yaw", &FusionOutput::yaw}
    };

    auto compute_imu_gnss_ekf = [](SessionData &session, const QString &outputKey) -> std::optional<QVector<double>> {
        QVector<double> gnssTime = session.getMeasurement("GNSS", SessionKeys::Time);
        QVector<double> lat = session.getMeasurement("GNSS", "lat");
        QVector<double> lon = session.getMeasurement("GNSS", "lon");
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> hAcc = session.getMeasurement("GNSS", "hAcc");
        QVector<double> vAcc = session.getMeasurement("GNSS", "vAcc");
        QVector<double> sAcc = session.getMeasurement("GNSS", "sAcc");

        QVector<double> imuTime = session.getMeasurement("IMU", SessionKeys::Time);
        QVector<double> ax = session.getMeasurement("IMU", "ax");
        QVector<double> ay = session.getMeasurement("IMU", "ay");
        QVector<double> az = session.getMeasurement("IMU", "az");
        QVector<double> wx = session.getMeasurement("IMU", "wx");
        QVector<double> wy = session.getMeasurement("IMU", "wy");
        QVector<double> wz = session.getMeasurement("IMU", "wz");

        if (gnssTime.isEmpty() || lat.isEmpty() || lon.isEmpty() || hMSL.isEmpty() ||
            velN.isEmpty() || velE.isEmpty() || velD.isEmpty() || hAcc.isEmpty() ||
            vAcc.isEmpty() || sAcc.isEmpty() || imuTime.isEmpty() || ax.isEmpty() ||
            ay.isEmpty() || az.isEmpty() || wx.isEmpty() || wy.isEmpty() || wz.isEmpty()) {
            qWarning() << "Cannot calculate EKF due to missing data";
            return std::nullopt;
        }

        // Run the fusion
        FusionOutput out = runFusion(
            gnssTime, lat, lon, hMSL, velN, velE, velD, hAcc, vAcc, sAcc,
            imuTime, ax, ay, az, wx, wy, wz);

        std::optional<QVector<double>> result;

        // Iterate over all outputs and either store or return the requested one
        for (const auto &entry : fusion_outputs) {
            if (entry.key == outputKey) {
                result = out.*(entry.member);  // This is the requested key, return it
            } else {
                session.setCalculatedMeasurement(SessionKeys::ImuGnssEkf, entry.key, out.*(entry.member));
            }
        }

        return result;
    };

    // Register for all outputs dynamically using `fusion_outputs`
    for (const auto &entry : fusion_outputs) {
        SessionData::registerCalculatedMeasurement(
            SessionKeys::ImuGnssEkf, entry.key,
            {
                DependencyKey::measurement("GNSS", SessionKeys::Time),
                DependencyKey::measurement("GNSS", "lat"),
                DependencyKey::measurement("GNSS", "lon"),
                DependencyKey::measurement("GNSS", "hMSL"),
                DependencyKey::measurement("GNSS", "velN"),
                DependencyKey::measurement("GNSS", "velE"),
                DependencyKey::measurement("GNSS", "velD"),
                DependencyKey::measurement("GNSS", "hAcc"),
                DependencyKey::measurement("GNSS", "vAcc"),
                DependencyKey::measurement("GNSS", "sAcc"),
                DependencyKey::measurement("IMU", SessionKeys::Time),
                DependencyKey::measurement("IMU", "ax"),
                DependencyKey::measurement("IMU", "ay"),
                DependencyKey::measurement("IMU", "az"),
                DependencyKey::measurement("IMU", "wx"),
                DependencyKey::measurement("IMU", "wy"),
                DependencyKey::measurement("IMU", "wz")
            },
            [compute_imu_gnss_ekf, key = entry.key](SessionData &s) {
                return compute_imu_gnss_ekf(s, key);
            });
    }

    // EKF horizontal acceleration
    SessionData::registerCalculatedMeasurement(
        SessionKeys::ImuGnssEkf, "accH",
        {
            DependencyKey::measurement(SessionKeys::ImuGnssEkf, "accN"),
            DependencyKey::measurement(SessionKeys::ImuGnssEkf, "accE")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
            QVector<double> accN = session.getMeasurement(SessionKeys::ImuGnssEkf, "accN");
            QVector<double> accE = session.getMeasurement(SessionKeys::ImuGnssEkf, "accE");

            if (accN.isEmpty() || accE.isEmpty()) {
                qWarning() << "Cannot calculate accH due to missing accN or accE";
                return std::nullopt;
            }

            if (accN.size() != accE.size()) {
                qWarning() << "accN and accE size mismatch in session:" << session.getAttribute(SessionKeys::SessionId);
                return std::nullopt;
            }

            QVector<double> accH;
            accH.reserve(accN.size());
            for(int i = 0; i < accN.size(); ++i){
                accH.append(std::sqrt(accN[i]*accN[i] + accE[i]*accE[i]));
            }
            return accH;
        });
}
