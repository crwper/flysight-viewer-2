#include "gnsscalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <cmath>

using namespace FlySight;

void Calculations::registerGnssCalculations()
{
    // GNSS altitude above ground (z)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "z",
        {
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::attribute(SessionKeys::GroundElev)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");

        bool ok;
        double groundElev = session.getAttribute(SessionKeys::GroundElev).toDouble(&ok);
        if (!ok) {
            qWarning() << "Cannot calculate z due to missing groundElev";
            return std::nullopt;
        }

        if (hMSL.isEmpty()) {
            qWarning() << "Cannot calculate z due to missing hMSL";
            return std::nullopt;
        }

        QVector<double> z;
        z.reserve(hMSL.size());
        for(int i = 0; i < hMSL.size(); ++i){
            z.append(hMSL[i] - groundElev);
        }
        return z;
    });

    // GNSS horizontal velocity (velH)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "velH",
        {
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");

        if (velN.isEmpty() || velE.isEmpty()) {
            qWarning() << "Cannot calculate velH due to missing velN or velE";
            return std::nullopt;
        }

        if (velN.size() != velE.size()) {
            qWarning() << "velN and velE size mismatch in session:" << session.getAttribute("_SESSION_ID");
            return std::nullopt;
        }

        QVector<double> velH;
        velH.reserve(velN.size());
        for(int i = 0; i < velN.size(); ++i){
            velH.append(std::sqrt(velN[i]*velN[i] + velE[i]*velE[i]));
        }
        return velH;
    });

    // GNSS total velocity (vel)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "vel",
        {
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", "velD")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (velH.isEmpty() || velD.isEmpty()) {
            qWarning() << "Cannot calculate vel due to missing velH or velD";
            return std::nullopt;
        }

        if (velH.size() != velD.size()) {
            qWarning() << "velH and velD size mismatch in session:" << session.getAttribute("_SESSION_ID");
            return std::nullopt;
        }

        QVector<double> vel;
        vel.reserve(velH.size());
        for(int i = 0; i < velH.size(); ++i){
            vel.append(std::sqrt(velH[i]*velH[i] + velD[i]*velD[i]));
        }
        return vel;
    });

    // GNSS vertical acceleration (accD)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accD",
        {
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (velD.isEmpty()) {
            qWarning() << "Cannot calculate accD due to missing velD";
            return std::nullopt;
        }

        if (time.size() != velD.size()) {
            qWarning() << "Cannot calculate accD because time and velD size mismatch.";
            return std::nullopt;
        }

        // If there's fewer than two samples, we cannot compute acceleration.
        if (velD.size() < 2) {
            qWarning() << "Not enough data points to calculate accD.";
            return std::nullopt;
        }

        QVector<double> accD;
        accD.reserve(velD.size());

        // For the first sample (i = 0), use forward difference:
        // a[0] = (velD[1] - velD[0]) / (time[1] - time[0])
        {
            double dt = time[1] - time[0];
            if (dt == 0.0) {
                qWarning() << "Zero time difference encountered between indices 0 and 1.";
                return std::nullopt;
            }
            double a = (velD[1] - velD[0]) / dt;
            accD.append(a);
        }

        // For the interior points (1 <= i <= velD.size()-2), use centered difference:
        // a[i] = (velD[i+1] - velD[i-1]) / (time[i+1] - time[i-1])
        for (int i = 1; i < velD.size() - 1; ++i) {
            double dt = time[i+1] - time[i-1];
            if (dt == 0.0) {
                qWarning() << "Zero time difference encountered for indices" << i-1 << "and" << i+1;
                return std::nullopt;
            }
            double a = (velD[i+1] - velD[i-1]) / dt;
            accD.append(a);
        }

        // For the last sample (i = velD.size()-1), use backward difference:
        // a[last] = (velD[last] - velD[last-1]) / (time[last] - time[last-1])
        {
            int last = velD.size() - 1;
            double dt = time[last] - time[last-1];
            if (dt == 0.0) {
                qWarning() << "Zero time difference encountered at the end indices:" << last-1 << "and" << last;
                return std::nullopt;
            }
            double a = (velD[last] - velD[last-1]) / dt;
            accD.append(a);
        }

        return accD;
    });
}
