#include "timecalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <QDateTime>
#include <algorithm>

using namespace FlySight;

std::optional<double> Calculations::systemTimeToUtc(const SessionData &session, double systemTime)
{
    if (!session.hasAttribute(SessionKeys::TimeFitA) ||
        !session.hasAttribute(SessionKeys::TimeFitB)) {
        return std::nullopt;
    }
    double a = session.getAttribute(SessionKeys::TimeFitA).toDouble();
    double b = session.getAttribute(SessionKeys::TimeFitB).toDouble();
    return a * systemTime + b;
}

std::optional<double> Calculations::utcToSystemTime(const SessionData &session, double utc)
{
    if (!session.hasAttribute(SessionKeys::TimeFitA) ||
        !session.hasAttribute(SessionKeys::TimeFitB)) {
        return std::nullopt;
    }
    double a = session.getAttribute(SessionKeys::TimeFitA).toDouble();
    double b = session.getAttribute(SessionKeys::TimeFitB).toDouble();
    if (a == 0.0) {
        return std::nullopt;  // Cannot invert: degenerate fit
    }
    return (utc - b) / a;
}

void Calculations::registerTimeCalculations()
{
    // Helper lambda to compute _time for non-GNSS sensors
    auto compute_time = [](SessionData &session, const QString &sensorKey) -> std::optional<QVector<double>> {
        // Check that sensor is allowed
        const QStringList allowedSensors = { "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT" };
        if (!allowedSensors.contains(sensorKey)) {
            return std::nullopt;
        }

        // Ensure fit coefficients are cached in session attributes
        bool haveFit = session.hasAttribute(SessionKeys::TimeFitA) && session.hasAttribute(SessionKeys::TimeFitB);
        if (!haveFit) {
            // Attempt to compute the fit from TIME sensor data
            if (!session.hasMeasurement("TIME", "time") ||
                !session.hasMeasurement("TIME", "tow") ||
                !session.hasMeasurement("TIME", "week")) {
                // TIME sensor not available or incomplete data
                return std::nullopt;
            }

            QVector<double> systemTime = session.getMeasurement("TIME", "time");
            QVector<double> tow = session.getMeasurement("TIME", "tow");
            QVector<double> week = session.getMeasurement("TIME", "week");

            int N = std::min({systemTime.size(), tow.size(), week.size()});
            if (N < 2) {
                // Not enough points for a linear fit
                return std::nullopt;
            }

            // Compute UTC time
            QVector<double> utcTime(N);
            for (int i = 0; i < N; ++i) {
                utcTime[i] = week[i] * 604800 + tow[i] + 315964800;
            }

            // Perform a linear fit: utcTime = a*systemTime + b
            double sumS = 0.0, sumU = 0.0, sumSS = 0.0, sumSU = 0.0;
            for (int i = 0; i < N; ++i) {
                double S = systemTime[i];
                double U = utcTime[i];
                sumS += S;
                sumU += U;
                sumSS += S * S;
                sumSU += S * U;
            }

            double denom = (N * sumSS - sumS * sumS);
            if (denom == 0.0) {
                // Degenerate fit
                return std::nullopt;
            }

            double a = (N * sumSU - sumS * sumU) / denom;
            double b = (sumU - a * sumS) / N;

            // Store fit parameters
            session.setAttribute(SessionKeys::TimeFitA, QString::number(a, 'g', 17));
            session.setAttribute(SessionKeys::TimeFitB, QString::number(b, 'g', 17));
        }

        // Now convert the sensor's 'time' measurement using the linear fit
        if (!session.hasMeasurement(sensorKey, "time")) {
            return std::nullopt;
        }

        QVector<double> sensorSystemTime = session.getMeasurement(sensorKey, "time");
        QVector<double> result(sensorSystemTime.size());
        for (int i = 0; i < sensorSystemTime.size(); ++i) {
            auto utc = systemTimeToUtc(session, sensorSystemTime[i]);
            if (!utc.has_value()) {
                return std::nullopt;
            }
            result[i] = utc.value();
        }
        return result;
    };

    // Register for GNSS (time is already in UTC)
    const QStringList gnss_sensors = {"GNSS"};
    for (const QString &sens : gnss_sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::Time,
            {
                DependencyKey::measurement(sens, "time")
            },
            [sens](SessionData& session) -> std::optional<QVector<double>> {
                QVector<double> sensTime = session.getMeasurement(sens, "time");

                if (sensTime.isEmpty()) {
                    qWarning() << "Cannot calculate time from epoch";
                    return std::nullopt;
                }

                return sensTime;
            });
    }

    // Register for other sensors (need time conversion)
    const QStringList sensors = {"BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::Time,
            {
                DependencyKey::measurement(sens, "time")
            },
            [compute_time, sens](SessionData &s) -> std::optional<QVector<double>> {
            return compute_time(s, sens);
        });
    }

    // Register _system_time for GNSS (inverse fit: UTC -> system time)
    SessionData::registerCalculatedMeasurement(
        "GNSS", SessionKeys::SystemTime,
        {
            DependencyKey::measurement("GNSS", "time"),
            DependencyKey::attribute(SessionKeys::TimeFitA),
            DependencyKey::attribute(SessionKeys::TimeFitB)
        },
        [](SessionData &session) -> std::optional<QVector<double>> {
            QVector<double> utcTime = session.getMeasurement("GNSS", "time");
            if (utcTime.isEmpty()) {
                return std::nullopt;
            }

            QVector<double> result(utcTime.size());
            for (int i = 0; i < utcTime.size(); ++i) {
                auto st = utcToSystemTime(session, utcTime[i]);
                if (!st.has_value()) {
                    return std::nullopt;
                }
                result[i] = st.value();
            }
            return result;
        });

    // Register _system_time for non-GNSS sensors (passthrough of raw 'time')
    const QStringList nonGnssSensors = {"BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : nonGnssSensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::SystemTime,
            {
                DependencyKey::measurement(sens, "time")
            },
            [sens](SessionData &session) -> std::optional<QVector<double>> {
                QVector<double> sensTime = session.getMeasurement(sens, "time");
                if (sensTime.isEmpty()) {
                    return std::nullopt;
                }
                return sensTime;
            });
    }

}
