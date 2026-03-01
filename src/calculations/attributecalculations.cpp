#include "attributecalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include "../preferences/preferencesmanager.h"
#include "../preferences/preferencekeys.h"
#include <QDateTime>
#include <QTimeZone>
#include <QVector>
#include <algorithm>

using namespace FlySight;

void Calculations::registerAttributeCalculations()
{
    // Analysis range: find the largest monotonic descent in elevation.
    // Both attributes are computed by the same shared function (like the
    // simplified-track pattern in simplificationcalculations.cpp).
    auto computeAnalysisRange = [](SessionData& session, const QString& outputKey) -> std::optional<QVariant> {
        // If the other attribute is already stored, the algorithm has already run
        QString otherKey = (outputKey == SessionKeys::AnalysisStartTime)
            ? SessionKeys::AnalysisEndTime
            : SessionKeys::AnalysisStartTime;
        if (session.hasAttribute(otherKey) && session.hasAttribute(outputKey))
            return session.getAttribute(outputKey);

        // Read the pause timeout from preferences at compute time
        PreferencesManager &prefs = PreferencesManager::instance();
        double timeout = prefs.getValue(PreferenceKeys::ImportDescentPauseSeconds).toDouble();

        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (hMSL.isEmpty() || time.isEmpty() || hMSL.size() != time.size())
            return std::nullopt;

        const int n = hMSL.size();

        // Initialize current-descent tracking from the first sample
        double currentHigh = hMSL[0];
        int currentHighIdx = 0;
        double currentLow = hMSL[0];
        int currentLowIdx = 0;

        // Best descent found so far
        double bestDrop = 0.0;
        int bestHighIdx = -1;
        int bestLowIdx = -1;

        for (int i = 1; i < n; ++i) {
            if (hMSL[i] > currentHigh) {
                // New high found: reset descent tracking
                currentHigh = hMSL[i];
                currentHighIdx = i;
                currentLow = hMSL[i];
                currentLowIdx = i;
            } else if (hMSL[i] <= currentLow) {
                // New low found (equal or below)
                currentLow = hMSL[i];
                currentLowIdx = i;
            } else if (time[i] - time[currentLowIdx] > timeout) {
                // Descent has ended due to pause timeout
                double drop = currentHigh - currentLow;
                if (drop > bestDrop) {
                    bestDrop = drop;
                    bestHighIdx = currentHighIdx;
                    bestLowIdx = currentLowIdx;
                }
                // Reset all current-state variables to this sample
                currentHigh = hMSL[i];
                currentHighIdx = i;
                currentLow = hMSL[i];
                currentLowIdx = i;
            }
        }

        // Finalize: compare the last descent against bestDrop
        double drop = currentHigh - currentLow;
        if (drop > bestDrop) {
            bestDrop = drop;
            bestHighIdx = currentHighIdx;
            bestLowIdx = currentLowIdx;
        }

        if (bestDrop > 0 && bestHighIdx >= 0) {
            // Add a grace period (equal to the pause timeout) on each side
            double startSec = std::max(time[bestHighIdx] - timeout, time[0]);
            double endSec = std::min(time[bestLowIdx] + timeout, time[n - 1]);

            QDateTime analysisStartTime = QDateTime::fromMSecsSinceEpoch(
                qint64(startSec * 1000.0), QTimeZone::utc());
            QDateTime analysisEndTime = QDateTime::fromMSecsSinceEpoch(
                qint64(endSec * 1000.0), QTimeZone::utc());

            // Store both results
            session.setAttribute(SessionKeys::AnalysisStartTime, analysisStartTime);
            session.setAttribute(SessionKeys::AnalysisEndTime, analysisEndTime);

            return session.getAttribute(outputKey);
        }

        return std::nullopt;
    };

    // Register both analysis range attributes with the same dependencies and shared function
    SessionData::registerCalculatedAttribute(
        SessionKeys::AnalysisStartTime,
        {
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [computeAnalysisRange](SessionData& session) -> std::optional<QVariant> {
        return computeAnalysisRange(session, SessionKeys::AnalysisStartTime);
    });

    SessionData::registerCalculatedAttribute(
        SessionKeys::AnalysisEndTime,
        {
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [computeAnalysisRange](SessionData& session) -> std::optional<QVariant> {
        return computeAnalysisRange(session, SessionKeys::AnalysisEndTime);
    });

    // Exit time calculation based on GNSS vertical speed threshold
    SessionData::registerCalculatedAttribute(
        SessionKeys::ExitTime,
        {
            DependencyKey::attribute(SessionKeys::AnalysisStartTime),
            DependencyKey::attribute(SessionKeys::AnalysisEndTime),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "sAcc"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        // Retrieve analysis window to constrain the search
        QVariant asVar = session.getAttribute(SessionKeys::AnalysisStartTime);
        if (!asVar.canConvert<QDateTime>())
            return std::nullopt;
        double analysisStartSec = asVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVariant aeVar = session.getAttribute(SessionKeys::AnalysisEndTime);
        if (!aeVar.canConvert<QDateTime>())
            return std::nullopt;
        double analysisEndSec = aeVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

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
            if (time[i] < analysisStartSec) continue;
            if (time[i] > analysisEndSec) break;

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
        return std::nullopt;
    });

    // Manoeuvre start time: last crossing where velD goes from < 10 to >= 10 m/s
    // between exit time and landing time
    SessionData::registerCalculatedAttribute(
        SessionKeys::ManoeuvreStartTime,
        {
            DependencyKey::attribute(SessionKeys::ExitTime),
            DependencyKey::attribute(SessionKeys::LandingTime),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        // Retrieve exit time and landing time to bound the search
        QVariant exitVar = session.getAttribute(SessionKeys::ExitTime);
        if (!exitVar.canConvert<QDateTime>())
            return std::nullopt;
        double exitSec = exitVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVariant landVar = session.getAttribute(SessionKeys::LandingTime);
        if (!landVar.canConvert<QDateTime>())
            return std::nullopt;
        double landingSec = landVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size())
            return std::nullopt;

        const double vThreshold = 10.0; // m/s

        // Find the LAST upward crossing of the threshold between exit and landing
        double lastCrossingTime = -1.0;
        bool found = false;

        for (int i = 1; i < velD.size(); ++i) {
            if (time[i] < exitSec) continue;
            if (time[i] > landingSec) break;
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

    // Landing time: first flying-to-walking transition within the analysis window
    // "Walking" = vertical speed < 2*sAcc AND horizontal speed < 10 km/h
    //             AND elevation within 10 m of ground
    SessionData::registerCalculatedAttribute(
        SessionKeys::LandingTime,
        {
            DependencyKey::attribute(SessionKeys::AnalysisStartTime),
            DependencyKey::attribute(SessionKeys::AnalysisEndTime),
            DependencyKey::attribute(SessionKeys::GroundElev),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", "sAcc"),
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        // Retrieve analysis window to constrain the search
        QVariant asVar = session.getAttribute(SessionKeys::AnalysisStartTime);
        if (!asVar.canConvert<QDateTime>())
            return std::nullopt;
        double analysisStartSec = asVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVariant aeVar = session.getAttribute(SessionKeys::AnalysisEndTime);
        if (!aeVar.canConvert<QDateTime>())
            return std::nullopt;
        double analysisEndSec = aeVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVariant geVar = session.getAttribute(SessionKeys::GroundElev);
        if (!geVar.canConvert<double>())
            return std::nullopt;
        double groundElev = geVar.toDouble();

        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> sAcc = session.getMeasurement("GNSS", "sAcc");
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        const int n = velD.size();
        if (n == 0 || velH.size() != n || sAcc.size() != n
            || hMSL.size() != n || time.size() != n)
            return std::nullopt;

        const double hSpeedThreshold = 10.0 / 3.6; // 10 km/h in m/s
        const double elevThreshold = 10.0; // metres above ground

        // "Walking" when all conditions are met:
        //   abs(velD) < 2*sAcc  AND  velH < 10 km/h  AND  within 10 m of ground
        auto isWalking = [&](int i) {
            return std::abs(velD[i]) < 2.0 * sAcc[i]
                && velH[i] < hSpeedThreshold
                && (hMSL[i] - groundElev) < elevThreshold;
        };

        // Find the FIRST flying-to-walking transition within the analysis window
        for (int i = 1; i < n; ++i) {
            if (time[i] < analysisStartSec) continue;
            if (time[i] > analysisEndSec) break;
            if (!isWalking(i - 1) && isWalking(i)) {
                return QDateTime::fromMSecsSinceEpoch(
                    qint64(time[i] * 1000.0), QTimeZone::utc());
            }
        }

        return std::nullopt;
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

    // Maximum vertical speed time (time of peak velD between manoeuvre start and landing)
    SessionData::registerCalculatedAttribute(
        SessionKeys::MaxVelDTime,
        {
            DependencyKey::attribute(SessionKeys::ManoeuvreStartTime),
            DependencyKey::attribute(SessionKeys::LandingTime),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        QVariant msVar = session.getAttribute(SessionKeys::ManoeuvreStartTime);
        if (!msVar.canConvert<QDateTime>())
            return std::nullopt;
        double msSec = msVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVariant landVar = session.getAttribute(SessionKeys::LandingTime);
        if (!landVar.canConvert<QDateTime>())
            return std::nullopt;
        double landingSec = landVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size())
            return std::nullopt;

        double maxVelD = -std::numeric_limits<double>::max();
        int maxIdx = -1;
        for (int i = 0; i < velD.size(); ++i) {
            if (time[i] < msSec) continue;
            if (time[i] > landingSec) break;
            if (velD[i] > maxVelD) {
                maxVelD = velD[i];
                maxIdx = i;
            }
        }

        if (maxIdx < 0)
            return std::nullopt;

        return QDateTime::fromMSecsSinceEpoch(
            qint64(time[maxIdx] * 1000.0), QTimeZone::utc());
    });

    // Maximum horizontal speed time (time of peak velH between manoeuvre start and landing)
    SessionData::registerCalculatedAttribute(
        SessionKeys::MaxVelHTime,
        {
            DependencyKey::attribute(SessionKeys::ManoeuvreStartTime),
            DependencyKey::attribute(SessionKeys::LandingTime),
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
        QVariant msVar = session.getAttribute(SessionKeys::ManoeuvreStartTime);
        if (!msVar.canConvert<QDateTime>())
            return std::nullopt;
        double msSec = msVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVariant landVar = session.getAttribute(SessionKeys::LandingTime);
        if (!landVar.canConvert<QDateTime>())
            return std::nullopt;
        double landingSec = landVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velH.isEmpty() || time.isEmpty() || velH.size() != time.size())
            return std::nullopt;

        double maxVelH = -std::numeric_limits<double>::max();
        int maxIdx = -1;
        for (int i = 0; i < velH.size(); ++i) {
            if (time[i] < msSec) continue;
            if (time[i] > landingSec) break;
            if (velH[i] > maxVelH) {
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
            DependencyKey::attribute(SessionKeys::AnalysisEndTime),
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData &session) -> std::optional<QVariant> {
        PreferencesManager &prefs = PreferencesManager::instance();
        QString mode = prefs.getValue(PreferenceKeys::ImportGroundReferenceMode).toString();
        double fixedElevation = prefs.getValue(PreferenceKeys::ImportFixedElevation).toDouble();

        if (mode == "Fixed") {
            // Always return the fixed elevation from preferences
            return fixedElevation;
        } else if (mode == "Automatic") {
            // Interpolate hMSL at the analysis end time
            QVariant aeVar = session.getAttribute(SessionKeys::AnalysisEndTime);
            if (!aeVar.canConvert<QDateTime>())
                return std::nullopt;
            double analysisEndSec = aeVar.toDateTime().toUTC().toMSecsSinceEpoch() / 1000.0;

            QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");
            QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

            if (hMSL.isEmpty() || time.isEmpty() || hMSL.size() != time.size())
                return std::nullopt;

            // Find the bracketing interval and interpolate
            int n = time.size();
            if (analysisEndSec <= time[0])
                return hMSL[0];
            if (analysisEndSec >= time[n - 1])
                return hMSL[n - 1];

            for (int i = 1; i < n; ++i) {
                if (time[i] >= analysisEndSec) {
                    double t0 = time[i - 1];
                    double t1 = time[i];
                    double a = (analysisEndSec - t0) / (t1 - t0);
                    double groundElev = hMSL[i - 1] + a * (hMSL[i] - hMSL[i - 1]);
                    return groundElev;
                }
            }
            return std::nullopt;
        } else {
            // Possibly a fallback or "no ground reference" if mode is unknown
            return std::nullopt;
        }
    });
}
