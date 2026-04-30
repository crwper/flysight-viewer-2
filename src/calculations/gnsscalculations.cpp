#include "gnsscalculations.h"
#include "derivativehelper.h"
#include "isadensity.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <cmath>
#include <limits>

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

        return Calculations::computeDerivative(velD, time);
    });

    // GNSS northward acceleration (accN)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accN",
        {
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (velN.isEmpty()) {
            return std::nullopt;
        }

        return Calculations::computeDerivative(velN, time);
    });

    // GNSS eastward acceleration (accE)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accE",
        {
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (velE.isEmpty()) {
            return std::nullopt;
        }

        return Calculations::computeDerivative(velE, time);
    });

    // GNSS wind-corrected total speed (wcVel)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "wcVel",
        {
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::attribute(SessionKeys::WindN),
            DependencyKey::attribute(SessionKeys::WindE)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (velN.isEmpty() || velE.isEmpty() || velD.isEmpty()) {
            return std::nullopt;
        }
        if (velN.size() != velE.size() || velN.size() != velD.size()) {
            return std::nullopt;
        }

        bool ok;
        double windN = session.getAttribute(SessionKeys::WindN).toDouble(&ok);
        if (!ok) windN = 0.0;
        double windE = session.getAttribute(SessionKeys::WindE).toDouble(&ok);
        if (!ok) windE = 0.0;

        QVector<double> result;
        result.reserve(velN.size());
        for (int i = 0; i < velN.size(); ++i) {
            double wcN = velN[i] - windN;
            double wcE = velE[i] - windE;
            double wcD = velD[i];
            result.append(std::sqrt(wcN * wcN + wcE * wcE + wcD * wcD));
        }
        return result;
    });

    // GNSS course (unwrapped heading minus reference)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "course",
        {
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", SessionKeys::Time),
            DependencyKey::attribute(SessionKeys::CourseRef)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (velN.isEmpty() || velE.isEmpty() || time.isEmpty()) {
            return std::nullopt;
        }
        if (velN.size() != velE.size() || velN.size() != time.size()) {
            return std::nullopt;
        }

        // Compute raw headings in degrees
        QVector<double> rawDeg;
        rawDeg.reserve(velN.size());
        for (int i = 0; i < velN.size(); ++i) {
            rawDeg.append(std::atan2(velE[i], velN[i]) * 180.0 / M_PI);
        }

        // Unwrap phase
        QVector<double> course;
        course.reserve(rawDeg.size());
        course.append(rawDeg[0]);
        for (int i = 1; i < rawDeg.size(); ++i) {
            double delta = rawDeg[i] - rawDeg[i - 1];
            if (delta > 180.0) delta -= 360.0;
            if (delta < -180.0) delta += 360.0;
            course.append(course[i - 1] + delta);
        }

        // Determine reference angle from CourseRef time
        double courseRef = 0.0;
        bool ok;
        double refTime = session.getAttribute(SessionKeys::CourseRef).toDouble(&ok);
        if (ok && refTime >= time.first() && refTime <= time.last()) {
            auto it = std::lower_bound(time.constBegin(), time.constEnd(), refTime);
            int idx = std::clamp<int>(int(it - time.constBegin()), 1, time.size() - 1);
            if (qFuzzyCompare(refTime, time[idx])) {
                courseRef = course[idx];
            } else if (qFuzzyCompare(refTime, time[idx - 1])) {
                courseRef = course[idx - 1];
            } else {
                double t = (refTime - time[idx - 1]) / (time[idx] - time[idx - 1]);
                courseRef = course[idx - 1] + t * (course[idx] - course[idx - 1]);
            }
        }

        // Subtract course reference
        for (int i = 0; i < course.size(); ++i) {
            course[i] -= courseRef;
        }

        return course;
    });

    // GNSS course rate (rate of change of course)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "courseRate",
        {
            DependencyKey::measurement("GNSS", "course"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> course = session.getMeasurement("GNSS", "course");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (course.isEmpty()) {
            return std::nullopt;
        }

        return Calculations::computeDerivative(course, time);
    });

    // GNSS glide ratio
    SessionData::registerCalculatedMeasurement(
        "GNSS", "glideRatio",
        {
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", "velD")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (velH.isEmpty() || velD.isEmpty()) {
            return std::nullopt;
        }
        if (velH.size() != velD.size()) {
            return std::nullopt;
        }

        QVector<double> result;
        result.reserve(velH.size());
        for (int i = 0; i < velH.size(); ++i) {
            if (std::abs(velD[i]) < 1e-6) {
                result.append(std::numeric_limits<double>::quiet_NaN());
            } else {
                result.append(velH[i] / velD[i]);
            }
        }
        return result;
    });

    // GNSS dive angle
    SessionData::registerCalculatedMeasurement(
        "GNSS", "diveAngle",
        {
            DependencyKey::measurement("GNSS", "velH"),
            DependencyKey::measurement("GNSS", "velD")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velH = session.getMeasurement("GNSS", "velH");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (velH.isEmpty() || velD.isEmpty()) {
            return std::nullopt;
        }
        if (velH.size() != velD.size()) {
            return std::nullopt;
        }

        QVector<double> result;
        result.reserve(velH.size());
        for (int i = 0; i < velH.size(); ++i) {
            result.append(std::atan2(velD[i], velH[i]) * 180.0 / M_PI);
        }
        return result;
    });

    // GNSS dive angle rate (rate of change of dive angle)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "diveAngleRate",
        {
            DependencyKey::measurement("GNSS", "diveAngle"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> diveAngle = session.getMeasurement("GNSS", "diveAngle");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (diveAngle.isEmpty()) {
            return std::nullopt;
        }

        return Calculations::computeDerivative(diveAngle, time);
    });

    // GNSS horizontal acceleration (accH)
    // Magnitude of the horizontal component of the total acceleration vector,
    // so that sqrt(accH^2 + accD^2) equals the total acceleration magnitude.
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accH",
        {
            DependencyKey::measurement("GNSS", "accN"),
            DependencyKey::measurement("GNSS", "accE")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> accN = session.getMeasurement("GNSS", "accN");
        QVector<double> accE = session.getMeasurement("GNSS", "accE");

        if (accN.isEmpty() || accE.isEmpty()) {
            return std::nullopt;
        }
        if (accN.size() != accE.size()) {
            return std::nullopt;
        }

        QVector<double> accH;
        accH.reserve(accN.size());
        for (int i = 0; i < accN.size(); ++i) {
            accH.append(std::sqrt(accN[i] * accN[i] + accE[i] * accE[i]));
        }
        return accH;
    });

    // GNSS wind-corrected horizontal speed (wcVelH)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "wcVelH",
        {
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::attribute(SessionKeys::WindN),
            DependencyKey::attribute(SessionKeys::WindE)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");

        if (velN.isEmpty() || velE.isEmpty()) {
            return std::nullopt;
        }
        if (velN.size() != velE.size()) {
            return std::nullopt;
        }

        bool ok;
        double windN = session.getAttribute(SessionKeys::WindN).toDouble(&ok);
        if (!ok) windN = 0.0;
        double windE = session.getAttribute(SessionKeys::WindE).toDouble(&ok);
        if (!ok) windE = 0.0;

        QVector<double> result;
        result.reserve(velN.size());
        for (int i = 0; i < velN.size(); ++i) {
            double wcN = velN[i] - windN;
            double wcE = velE[i] - windE;
            result.append(std::sqrt(wcN * wcN + wcE * wcE));
        }
        return result;
    });

    // GNSS along-track acceleration (accAlongTrack)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accAlongTrack",
        {
            DependencyKey::measurement("GNSS", "accN"),
            DependencyKey::measurement("GNSS", "accE"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::attribute(SessionKeys::WindN),
            DependencyKey::attribute(SessionKeys::WindE)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> accN = session.getMeasurement("GNSS", "accN");
        QVector<double> accE = session.getMeasurement("GNSS", "accE");
        QVector<double> accD = session.getMeasurement("GNSS", "accD");
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (accN.isEmpty() || accE.isEmpty() || accD.isEmpty() ||
            velN.isEmpty() || velE.isEmpty() || velD.isEmpty()) {
            return std::nullopt;
        }

        int n = accN.size();
        if (accE.size() != n || accD.size() != n ||
            velN.size() != n || velE.size() != n || velD.size() != n) {
            return std::nullopt;
        }

        bool ok;
        double windN = session.getAttribute(SessionKeys::WindN).toDouble(&ok);
        if (!ok) windN = 0.0;
        double windE = session.getAttribute(SessionKeys::WindE).toDouble(&ok);
        if (!ok) windE = 0.0;

        QVector<double> result;
        result.reserve(n);
        for (int i = 0; i < n; ++i) {
            double wcN = velN[i] - windN;
            double wcE = velE[i] - windE;
            double wcD = velD[i];
            double wcMag = std::sqrt(wcN * wcN + wcE * wcE + wcD * wcD);

            if (wcMag < 1e-9) {
                result.append(0.0);
            } else {
                double uN = wcN / wcMag;
                double uE = wcE / wcMag;
                double uD = wcD / wcMag;
                double dot = accN[i] * uN + accE[i] * uE + accD[i] * uD;
                result.append(dot);
            }
        }
        return result;
    });

    // GNSS cross-track acceleration (accCrossTrack)
    SessionData::registerCalculatedMeasurement(
        "GNSS", "accCrossTrack",
        {
            DependencyKey::measurement("GNSS", "accN"),
            DependencyKey::measurement("GNSS", "accE"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::attribute(SessionKeys::WindN),
            DependencyKey::attribute(SessionKeys::WindE)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> accN = session.getMeasurement("GNSS", "accN");
        QVector<double> accE = session.getMeasurement("GNSS", "accE");
        QVector<double> accD = session.getMeasurement("GNSS", "accD");
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");

        if (accN.isEmpty() || accE.isEmpty() || accD.isEmpty() ||
            velN.isEmpty() || velE.isEmpty() || velD.isEmpty()) {
            return std::nullopt;
        }

        int n = accN.size();
        if (accE.size() != n || accD.size() != n ||
            velN.size() != n || velE.size() != n || velD.size() != n) {
            return std::nullopt;
        }

        bool ok;
        double windN = session.getAttribute(SessionKeys::WindN).toDouble(&ok);
        if (!ok) windN = 0.0;
        double windE = session.getAttribute(SessionKeys::WindE).toDouble(&ok);
        if (!ok) windE = 0.0;

        QVector<double> result;
        result.reserve(n);
        for (int i = 0; i < n; ++i) {
            double wcN = velN[i] - windN;
            double wcE = velE[i] - windE;
            double wcD = velD[i];
            double wcMag = std::sqrt(wcN * wcN + wcE * wcE + wcD * wcD);

            double alongTrack;
            if (wcMag < 1e-9) {
                alongTrack = 0.0;
            } else {
                double uN = wcN / wcMag;
                double uE = wcE / wcMag;
                double uD = wcD / wcMag;
                double dot = accN[i] * uN + accE[i] * uE + accD[i] * uD;
                alongTrack = -dot;
            }

            double aMag2 = accN[i] * accN[i] + accE[i] * accE[i] + accD[i] * accD[i];
            result.append(std::sqrt(std::max(0.0, aMag2 - alongTrack * alongTrack)));
        }
        return result;
    });

    // GNSS lift coefficient (lift)
    // Uses gravity-corrected acceleration: aeroD = accD - g, so that
    // level constant-speed flight (zero kinematic acceleration) correctly
    // shows ~1g of lift.
    SessionData::registerCalculatedMeasurement(
        "GNSS", "lift",
        {
            DependencyKey::measurement("GNSS", "accN"),
            DependencyKey::measurement("GNSS", "accE"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "wcVel"),
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::attribute(SessionKeys::WindN),
            DependencyKey::attribute(SessionKeys::WindE),
            DependencyKey::attribute(SessionKeys::JumperMass),
            DependencyKey::attribute(SessionKeys::PlanformArea)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        constexpr double g = 9.80665;

        QVector<double> accN = session.getMeasurement("GNSS", "accN");
        QVector<double> accE = session.getMeasurement("GNSS", "accE");
        QVector<double> accD = session.getMeasurement("GNSS", "accD");
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> wcVel = session.getMeasurement("GNSS", "wcVel");
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");

        if (accN.isEmpty() || accE.isEmpty() || accD.isEmpty() ||
            velN.isEmpty() || velE.isEmpty() || velD.isEmpty() ||
            wcVel.isEmpty() || hMSL.isEmpty()) {
            return std::nullopt;
        }

        int n = accN.size();
        if (accE.size() != n || accD.size() != n ||
            velN.size() != n || velE.size() != n || velD.size() != n ||
            wcVel.size() != n || hMSL.size() != n) {
            return std::nullopt;
        }

        bool ok;
        double windN = session.getAttribute(SessionKeys::WindN).toDouble(&ok);
        if (!ok) windN = 0.0;
        double windE = session.getAttribute(SessionKeys::WindE).toDouble(&ok);
        if (!ok) windE = 0.0;

        QVariant massVar = session.getAttribute(SessionKeys::JumperMass);
        QVariant areaVar = session.getAttribute(SessionKeys::PlanformArea);
        if (!massVar.isValid() || !areaVar.isValid()) return std::nullopt;
        double mass = massVar.toDouble();
        double area = areaVar.toDouble();

        QVector<double> result;
        result.reserve(n);
        for (int i = 0; i < n; ++i) {
            double rho = Calculations::isaDensity(hMSL[i]);
            double mu = wcVel[i];
            if (mu < 1e-9 || rho < 1e-12 || area < 1e-12) {
                result.append(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            // Gravity-corrected acceleration
            double aeroN = accN[i];
            double aeroE = accE[i];
            double aeroD = accD[i] - g;

            // Wind-corrected velocity unit vector
            double wcN = velN[i] - windN;
            double wcE = velE[i] - windE;
            double wcD = velD[i];
            double wcMag = std::sqrt(wcN * wcN + wcE * wcE + wcD * wcD);

            if (wcMag < 1e-9) {
                result.append(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            double uN = wcN / wcMag;
            double uE = wcE / wcMag;
            double uD = wcD / wcMag;

            // Cross-track magnitude
            double dot = aeroN * uN + aeroE * uE + aeroD * uD;
            double aMag2 = aeroN * aeroN + aeroE * aeroE + aeroD * aeroD;
            double crossTrack = std::sqrt(std::max(0.0, aMag2 - dot * dot));

            result.append(2.0 * mass * crossTrack / (rho * mu * mu * area));
        }
        return result;
    });

    // GNSS drag coefficient (drag)
    // Uses gravity-corrected acceleration, same as lift.
    // Negated so that drag (deceleration along track) gives positive values.
    SessionData::registerCalculatedMeasurement(
        "GNSS", "drag",
        {
            DependencyKey::measurement("GNSS", "accN"),
            DependencyKey::measurement("GNSS", "accE"),
            DependencyKey::measurement("GNSS", "accD"),
            DependencyKey::measurement("GNSS", "velN"),
            DependencyKey::measurement("GNSS", "velE"),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", "wcVel"),
            DependencyKey::measurement("GNSS", "hMSL"),
            DependencyKey::attribute(SessionKeys::WindN),
            DependencyKey::attribute(SessionKeys::WindE),
            DependencyKey::attribute(SessionKeys::JumperMass),
            DependencyKey::attribute(SessionKeys::PlanformArea)
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        constexpr double g = 9.80665;

        QVector<double> accN = session.getMeasurement("GNSS", "accN");
        QVector<double> accE = session.getMeasurement("GNSS", "accE");
        QVector<double> accD = session.getMeasurement("GNSS", "accD");
        QVector<double> velN = session.getMeasurement("GNSS", "velN");
        QVector<double> velE = session.getMeasurement("GNSS", "velE");
        QVector<double> velD = session.getMeasurement("GNSS", "velD");
        QVector<double> wcVel = session.getMeasurement("GNSS", "wcVel");
        QVector<double> hMSL = session.getMeasurement("GNSS", "hMSL");

        if (accN.isEmpty() || accE.isEmpty() || accD.isEmpty() ||
            velN.isEmpty() || velE.isEmpty() || velD.isEmpty() ||
            wcVel.isEmpty() || hMSL.isEmpty()) {
            return std::nullopt;
        }

        int n = accN.size();
        if (accE.size() != n || accD.size() != n ||
            velN.size() != n || velE.size() != n || velD.size() != n ||
            wcVel.size() != n || hMSL.size() != n) {
            return std::nullopt;
        }

        bool ok;
        double windN = session.getAttribute(SessionKeys::WindN).toDouble(&ok);
        if (!ok) windN = 0.0;
        double windE = session.getAttribute(SessionKeys::WindE).toDouble(&ok);
        if (!ok) windE = 0.0;

        QVariant massVar = session.getAttribute(SessionKeys::JumperMass);
        QVariant areaVar = session.getAttribute(SessionKeys::PlanformArea);
        if (!massVar.isValid() || !areaVar.isValid()) return std::nullopt;
        double mass = massVar.toDouble();
        double area = areaVar.toDouble();

        QVector<double> result;
        result.reserve(n);
        for (int i = 0; i < n; ++i) {
            double rho = Calculations::isaDensity(hMSL[i]);
            double mu = wcVel[i];
            if (mu < 1e-9 || rho < 1e-12 || area < 1e-12) {
                result.append(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            // Gravity-corrected acceleration
            double aeroN = accN[i];
            double aeroE = accE[i];
            double aeroD = accD[i] - g;

            // Wind-corrected velocity unit vector
            double wcN = velN[i] - windN;
            double wcE = velE[i] - windE;
            double wcD = velD[i];
            double wcMag = std::sqrt(wcN * wcN + wcE * wcE + wcD * wcD);

            if (wcMag < 1e-9) {
                result.append(std::numeric_limits<double>::quiet_NaN());
                continue;
            }

            double uN = wcN / wcMag;
            double uE = wcE / wcMag;
            double uD = wcD / wcMag;

            // Along-track component (negated: drag positive)
            double alongTrack = aeroN * uN + aeroE * uE + aeroD * uD;
            result.append(-2.0 * mass * alongTrack / (rho * mu * mu * area));
        }
        return result;
    });

    // GNSS specific energy
    SessionData::registerCalculatedMeasurement(
        "GNSS", "specificEnergy",
        {
            DependencyKey::measurement("GNSS", "vel"),
            DependencyKey::measurement("GNSS", "z")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> vel = session.getMeasurement("GNSS", "vel");
        QVector<double> z = session.getMeasurement("GNSS", "z");

        if (vel.isEmpty() || z.isEmpty()) {
            return std::nullopt;
        }
        if (vel.size() != z.size()) {
            return std::nullopt;
        }

        const double g = 9.80665;

        QVector<double> result;
        result.reserve(vel.size());
        for (int i = 0; i < vel.size(); ++i) {
            result.append(0.5 * vel[i] * vel[i] + g * z[i]);
        }
        return result;
    });

    // GNSS specific energy rate
    SessionData::registerCalculatedMeasurement(
        "GNSS", "specificEnergyRate",
        {
            DependencyKey::measurement("GNSS", "specificEnergy"),
            DependencyKey::measurement("GNSS", "time")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> specificEnergy = session.getMeasurement("GNSS", "specificEnergy");
        QVector<double> time = session.getMeasurement("GNSS", "time");

        if (specificEnergy.isEmpty()) {
            return std::nullopt;
        }

        return Calculations::computeDerivative(specificEnergy, time);
    });
}
