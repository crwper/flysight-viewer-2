#include "timecalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <QDateTime>
#include <algorithm>

using namespace FlySight;

void Calculations::registerTimeCalculations()
{
    // Helper lambda to compute _time for non-GNSS sensors
    auto compute_time = [](SessionData &session, const QString &sensorKey) -> std::optional<QVector<double>> {
        // Check that sensor is allowed
        const QStringList allowedSensors = { "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT" };
        if (!allowedSensors.contains(sensorKey)) {
            return std::nullopt;
        }

        // We need TIME sensor data and a linear fit
        bool haveFit = session.hasAttribute(SessionKeys::TimeFitA) && session.hasAttribute(SessionKeys::TimeFitB);
        double a = 0.0, b = 0.0;
        if (!haveFit) {
            // Attempt to compute the fit
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

            a = (N * sumSU - sumS * sumU) / denom;
            b = (sumU - a * sumS) / N;

            // Store fit parameters
            session.setAttribute(SessionKeys::TimeFitA, QString::number(a, 'g', 17));
            session.setAttribute(SessionKeys::TimeFitB, QString::number(b, 'g', 17));
        } else {
            // Already computed fit
            a = session.getAttribute(SessionKeys::TimeFitA).toDouble();
            b = session.getAttribute(SessionKeys::TimeFitB).toDouble();
        }

        // Now convert the sensor's 'time' measurement using the linear fit
        if (!session.hasMeasurement(sensorKey, "time")) {
            return std::nullopt;
        }

        QVector<double> sensorSystemTime = session.getMeasurement(sensorKey, "time");
        QVector<double> result(sensorSystemTime.size());
        for (int i = 0; i < sensorSystemTime.size(); ++i) {
            result[i] = a * sensorSystemTime[i] + b;
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

    // Helper lambda to compute time from exit
    auto compute_time_from_exit = [](SessionData &session, const QString &sensorKey) -> std::optional<QVector<double>> {
        // Get raw time first to force calculation if needed
        QVector<double> rawTime = session.getMeasurement(sensorKey, SessionKeys::Time);

        // Then get exit time attribute
        QVariant var = session.getAttribute(SessionKeys::ExitTime);
        if (!var.canConvert<QDateTime>()) {
            return std::nullopt;
        }

        QDateTime dt = var.toDateTime();
        if (!dt.isValid()) {
            return std::nullopt;
        }

        // If you need the exit time as a double (seconds since epoch):
        double exitTime = dt.toMSecsSinceEpoch() / 1000.0;

        // Now calculate the difference
        QVector<double> result(rawTime.size());
        for (int i = 0; i < rawTime.size(); ++i) {
            result[i] = rawTime[i] - exitTime;
        }
        return result;
    };

    // Register for all sensors
    QStringList all_sensors = {"GNSS", "BARO", "HUM", "MAG", "IMU", "TIME", "VBAT", SessionKeys::ImuGnssEkf};
    for (const QString &sens : all_sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::TimeFromExit,
            {
                DependencyKey::measurement(sens, SessionKeys::Time),
                DependencyKey::attribute(SessionKeys::ExitTime)
            },
            [compute_time_from_exit, sens](SessionData &s) {
            return compute_time_from_exit(s, sens);
        });
    }
}
