#include "attributecalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include "../preferences/preferencesmanager.h"
#include <QDateTime>
#include <QTimeZone>
#include <QVector>
#include <algorithm>

using namespace FlySight;

void Calculations::registerAttributeCalculations()
{
    // Exit time calculation based on GNSS vertical speed threshold
    SessionData::registerCalculatedAttribute(
        SessionKeys::ExitTime,
        {
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "sAcc"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        // Find the first timestamp where vertical speed drops below a threshold
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> sAcc = session.getMeasurement("GNSS", "sAcc");
        QVector<double> accD = session.getMeasurement("GNSS", "accD");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size()) {
            qWarning() << "Insufficient data to calculate exit time.";
            return std::nullopt;
        }

        const double vThreshold = 10.0; // Vertical speed threshold in m/s
        const double maxAccuracy = 1.0; // Maximum speed acccuracy in m/s
        const double minAcceleration = 2.5; // Minimum vertical accleration in m/s^2

        for (int i = 1; i < velD.size(); ++i) {
            // Get interpolation coefficient
            const double a = (vThreshold - velD[i - 1]) / (velD[i] - velD[i - 1]);

            // Check vertical speed
            if (a < 0 || 1 < a) continue;

            // Check accuracy
            const double acc = sAcc[i - 1] + a * (sAcc[i] - sAcc[i - 1]);
            if (acc > maxAccuracy) continue;

            // Check acceleration
            const double az = accD[i - 1] + a * (accD[i] - accD[i - 1]);
            if (az < minAcceleration) continue;

            // Determine exit
            const double tExit = time[i - 1] + a * (time[i] - time[i - 1]) - vThreshold / az;
            return QDateTime::fromMSecsSinceEpoch((qint64)(tExit * 1000.0), QTimeZone::utc());
        }

        qWarning() << "Exit time could not be determined based on current data.";
        return QDateTime::fromMSecsSinceEpoch((qint64)(time.back() * 1000.0), QTimeZone::utc());
    });

    // Manoeuvre start time: last crossing where velD goes from < 10 to >= 10 m/s
    SessionData::registerCalculatedAttribute(
        SessionKeys::ManoeuvreStartTime,
        {
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size())
            return std::nullopt;

        const double vThreshold = 10.0; // m/s

        // Find the LAST upward crossing of the threshold
        double lastCrossingTime = -1.0;
        bool found = false;

        for (int i = 1; i < velD.size(); ++i) {
            if (velD[i - 1] < vThreshold && velD[i] >= vThreshold) {
                double a = (vThreshold - velD[i - 1]) / (velD[i] - velD[i - 1]);
                lastCrossingTime = time[i - 1] + a * (time[i] - time[i - 1]);
                found = true;
            }
        }

        if (!found)
            return std::nullopt;

        return QDateTime::fromMSecsSinceEpoch(
            qint64(lastCrossingTime * 1000.0), QTimeZone::utc());
    });

    // Landing time: last transition from "flying" to "walking"
    // "Walking" = vertical speed < 2*sAcc AND horizontal speed < 10 km/h
    SessionData::registerCalculatedAttribute(
        SessionKeys::LandingTime,
        {
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", "sAcc"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> sAcc = session.getMeasurement("GNSS", "sAcc");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        const int n = velD.size();
        if (n == 0 || velH.size() != n || sAcc.size() != n || time.size() != n)
            return std::nullopt;

        const double hSpeedThreshold = 10.0 / 3.6; // 10 km/h in m/s

        // "Walking" when both conditions are met:
        //   abs(velD) < 2*sAcc  AND  velH < 10 km/h
        // Find the LAST sample transition from not-walking to walking
        auto isWalking = [&](int i) {
            return std::abs(velD[i]) < 2.0 * sAcc[i]
                && velH[i] < hSpeedThreshold;
        };

        double lastTransitionTime = -1.0;
        bool found = false;

        for (int i = 1; i < n; ++i) {
            if (!isWalking(i - 1) && isWalking(i)) {
                lastTransitionTime = time[i];
                found = true;
            }
        }

        if (!found)
            return std::nullopt;

        return QDateTime::fromMSecsSinceEpoch(
            qint64(lastTransitionTime * 1000.0), QTimeZone::utc());
    });

    // Register start time and duration for all sensors
    QStringList all_sensors = {"GNSS", "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : all_sensors) {
        SessionData::registerCalculatedAttribute(
            SessionKeys::StartTime,
            {
                DependencyKey::measurement(sens, SessionKeys::Time)
            },
            [sens](SessionData &session) -> std::optional<QVariant> {
            // Retrieve sensor time measurement
            QVector<double> times = session.getMeasurement(sens, SessionKeys::Time);
            if (times.isEmpty()) {
                qWarning() << "No " << sens << "/time data available to calculate start time.";
                return std::nullopt;
            }

            double startTime = *std::min_element(times.begin(), times.end());
            return QDateTime::fromMSecsSinceEpoch((qint64)(startTime * 1000.0), QTimeZone::utc());
        });

        SessionData::registerCalculatedAttribute(
            SessionKeys::Duration,
            {
                DependencyKey::measurement(sens, SessionKeys::Time)
            },
            [sens](SessionData &session) -> std::optional<QVariant> {
            QVector<double> times = session.getMeasurement(sens, SessionKeys::Time);
            if (times.isEmpty()) {
                qWarning() << "No " << sens << "/time data available to calculate duration.";
                return std::nullopt;
            }

            double minTime = *std::min_element(times.begin(), times.end());
            double maxTime = *std::max_element(times.begin(), times.end());
            double durationSec = maxTime - minTime;
            if (durationSec < 0) {
                qWarning() << "Invalid " << sens << "/time data (max < min).";
                return std::nullopt;
            }

            return durationSec;
        });
    }

    // Maximum vertical speed time (time of peak velD after manoeuvre start)
    SessionData::registerCalculatedAttribute(
        SessionKeys::MaxVelDTime,
        {
            DependencyKey::attribute(SessionKeys::ManoeuvreStartTime),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        QVariant msVar = session.getAttribute(SessionKeys::ManoeuvreStartTime);
        if (!msVar.canConvert<QDateTime>())
            return std::nullopt;

        double msSec = msVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size())
            return std::nullopt;

        double maxVelD = -std::numeric_limits<double>::max();
        int maxIdx = -1;
        for (int i = 0; i < velD.size(); ++i) {
            if (time[i] >= msSec && velD[i] > maxVelD) {
                maxVelD = velD[i];
                maxIdx = i;
            }
        }

        if (maxIdx < 0)
            return std::nullopt;

        return QDateTime::fromMSecsSinceEpoch(
            qint64(time[maxIdx] * 1000.0), QTimeZone::utc());
    });

    // Maximum horizontal speed time (time of peak velH after manoeuvre start)
    SessionData::registerCalculatedAttribute(
        SessionKeys::MaxVelHTime,
        {
            DependencyKey::attribute(SessionKeys::ManoeuvreStartTime),
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        QVariant msVar = session.getAttribute(SessionKeys::ManoeuvreStartTime);
        if (!msVar.canConvert<QDateTime>())
            return std::nullopt;

        double msSec = msVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velH.isEmpty() || time.isEmpty() || velH.size() != time.size())
            return std::nullopt;

        double maxVelH = -std::numeric_limits<double>::max();
        int maxIdx = -1;
        for (int i = 0; i < velH.size(); ++i) {
            if (time[i] >= msSec && velH[i] > maxVelH) {
                maxVelH = velH[i];
                maxIdx = i;
            }
        }

        if (maxIdx < 0)
            return std::nullopt;

        return QDateTime::fromMSecsSinceEpoch(
            qint64(time[maxIdx] * 1000.0), QTimeZone::utc());
    });

    // Ground elevation calculation based on preferences and GNSS data
    SessionData::registerCalculatedAttribute(
        SessionKeys::GroundElev,
        {
            DependencyKey::measurement("GNSS", "hMSL")
        },
        [](SessionData &session) -> std::optional<QVariant> {
        PreferencesManager &prefs = PreferencesManager::instance();
        QString mode = prefs.getValue("import/groundReferenceMode").toString();
        double fixedElevation = prefs.getValue("import/fixedElevation").toDouble();

        if (mode == "Fixed") {
            // Always return the fixed elevation from preferences
            return fixedElevation;
        } else if (mode == "Automatic") {
            // Use some GNSS/hMSL measurement from session
            QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
            if (!hMSL.isEmpty()) {
                // e.g. use the last hMSL sample
                double groundElev = hMSL.last();
                return groundElev;
            } else {
                // Not enough data to compute
                // Return no value => the calculation fails for now
                return std::nullopt;
            }
        } else {
            // Possibly a fallback or "no ground reference" if mode is unknown
            return std::nullopt;
        }
    });
}
