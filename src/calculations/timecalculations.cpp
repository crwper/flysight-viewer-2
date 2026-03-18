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
    // Shared lambda to compute the system-time-to-UTC linear fit coefficients.
    // Both TimeFitA and TimeFitB are computed together and cached as session
    // attributes.  Follows the computeAnalysisRange pattern in
    // attributecalculations.cpp.
    auto computeTimeFit = [](SessionData &session, const QString &outputKey) -> std::optional<QVariant> {
        // Short-circuit: if both attributes already exist, return the requested one
        if (session.hasAttribute(SessionKeys::TimeFitA) &&
            session.hasAttribute(SessionKeys::TimeFitB)) {
            return session.getAttribute(outputKey);
        }

        // Need TIME sensor data to compute the fit
        if (!session.hasMeasurement("TIME", "time") ||
            !session.hasMeasurement("TIME", "tow") ||
            !session.hasMeasurement("TIME", "week")) {
            return std::nullopt;
        }

        QVector<double> systemTime = session.getMeasurement("TIME", "time");
        QVector<double> tow = session.getMeasurement("TIME", "tow");
        QVector<double> week = session.getMeasurement("TIME", "week");

        int N = std::min({systemTime.size(), tow.size(), week.size()});
        if (N < 2) {
            return std::nullopt;
        }

        // Compute UTC time from GPS week + time-of-week
        QVector<double> utcTime(N);
        for (int i = 0; i < N; ++i) {
            utcTime[i] = week[i] * 604800 + tow[i] + 315964800;
        }

        // Least-squares linear fit: utcTime = a * systemTime + b
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
            return std::nullopt;
        }

        double a = (N * sumSU - sumS * sumU) / denom;
        double b = (sumU - a * sumS) / N;

        session.setCalculatedAttribute(SessionKeys::TimeFitA, QString::number(a, 'g', 17));
        session.setCalculatedAttribute(SessionKeys::TimeFitB, QString::number(b, 'g', 17));

        return session.getAttribute(outputKey);
    };

    // Register TimeFitA and TimeFitB as calculated attributes so the dependency
    // system can trigger their computation on demand — regardless of whether
    // _time or _system_time is the active x-variable.
    const QList<DependencyKey> fitDeps = {
        DependencyKey::measurement("TIME", "time"),
        DependencyKey::measurement("TIME", "tow"),
        DependencyKey::measurement("TIME", "week")
    };

    SessionData::registerCalculatedAttribute(
        SessionKeys::TimeFitA, fitDeps,
        [computeTimeFit](SessionData &s) -> std::optional<QVariant> {
            return computeTimeFit(s, SessionKeys::TimeFitA);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::TimeFitB, fitDeps,
        [computeTimeFit](SessionData &s) -> std::optional<QVariant> {
            return computeTimeFit(s, SessionKeys::TimeFitB);
        });

    // Helper lambda to compute _time for non-GNSS sensors.
    // The fit coefficients are now computed on demand via the calculated
    // attribute system; systemTimeToUtc triggers this through getAttribute.
    auto compute_time = [](SessionData &session, const QString &sensorKey) -> std::optional<QVector<double>> {
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

    // Register for other sensors (need time conversion via fit coefficients)
    const QStringList sensors = {"BARO", "HUM", "MAG", "IMU", "TIME", "VBAT"};
    for (const QString &sens : sensors) {
        SessionData::registerCalculatedMeasurement(
            sens, SessionKeys::Time,
            {
                DependencyKey::measurement(sens, "time"),
                DependencyKey::attribute(SessionKeys::TimeFitA),
                DependencyKey::attribute(SessionKeys::TimeFitB)
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
