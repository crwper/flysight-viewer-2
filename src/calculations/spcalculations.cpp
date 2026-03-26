#include "spcalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include "../markerregistry.h"
#include "../attributeregistry.h"
#include "../units/unitdefinitions.h"
#include <QColor>
#include <cmath>

using namespace FlySight;

void Calculations::registerSpCalculations()
{
    // ── Group A: Parameter defaults (3 calculated attributes) ────────────

    // Performance window height in metres (default 7400 ft = 2255.52 m)
    SessionData::registerCalculatedAttribute(
        SessionKeys::SpPerfWindowHeight,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(7400.0 / 3.28084);
        });

    // Validation window height in metres (default 3300 ft = 1005.84 m)
    SessionData::registerCalculatedAttribute(
        SessionKeys::SpValWindowHeight,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(3300.0 / 3.28084);
        });

    // Breakoff altitude AGL in metres (default 5600 ft = 1706.88 m)
    SessionData::registerCalculatedAttribute(
        SessionKeys::SpBreakoffAlt,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(5600.0 / 3.28084);
        });

    // ── Group A2: Performance window start ───────────────────────────────
    // Find when velD first reaches 10 m/s after exit.
    // Also stores the MSL altitude at that crossing point.

    SessionData::registerCalculatedAttribute(
        SessionKeys::SpWindowStartTime,
        {
            DependencyKey::attribute(SessionKeys::ExitTime),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time),
            DependencyKey::measurement("GNSS", "z")
        },
        [](SessionData& session) -> std::optional<QVariant> {
            QVariant exitVar = session.getAttribute(SessionKeys::ExitTime);
            if (!exitVar.canConvert<double>())
                return std::nullopt;
            double exitTime = exitVar.toDouble();

            QVector<double> velD = session.getMeasurement("GNSS", "velD");
            QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);
            QVector<double> z    = session.getMeasurement("GNSS", "z");

            if (velD.isEmpty() || time.isEmpty() || z.isEmpty()
                || velD.size() != time.size() || z.size() != time.size())
                return std::nullopt;

            const int n = velD.size();
            const double vThreshold = 10.0;

            for (int i = 1; i < n; ++i) {
                if (time[i] < exitTime)
                    continue;
                if (velD[i - 1] < vThreshold && velD[i] >= vThreshold) {
                    double a = (vThreshold - velD[i - 1]) / (velD[i] - velD[i - 1]);
                    double crossingTime = time[i - 1] + a * (time[i] - time[i - 1]);
                    double crossingAlt  = z[i - 1]    + a * (z[i]    - z[i - 1]);

                    session.setCalculatedAttribute(SessionKeys::SpWindowStartAlt,
                                                   crossingAlt);
                    return QVariant(crossingTime);
                }
            }

            session.setCalculatedAttribute(SessionKeys::SpWindowStartAlt, QVariant());
            return std::nullopt;
        });

    // SpWindowStartAlt is set as a side-effect of SpWindowStartTime above.
    // Register it with the same dependencies so the dependency system knows about it.
    SessionData::registerCalculatedAttribute(
        SessionKeys::SpWindowStartAlt,
        {
            DependencyKey::attribute(SessionKeys::SpWindowStartTime)
        },
        [](SessionData& session) -> std::optional<QVariant> {
            // Force computation of SpWindowStartTime which sets SpWindowStartAlt
            QVariant v = session.getAttribute(SessionKeys::SpWindowStartTime);
            QVariant alt = session.getAttribute(SessionKeys::SpWindowStartAlt);
            if (alt.isValid() && alt.canConvert<double>())
                return alt;
            return std::nullopt;
        });

    // ── Group B: Result calculations ─────────────────────────────────────

    auto computeSpResults = [](SessionData& session, const QString& outputKey) -> std::optional<QVariant> {
        // Retrieve parameters
        QVariant perfHVar = session.getAttribute(SessionKeys::SpPerfWindowHeight);
        if (!perfHVar.canConvert<double>())
            return std::nullopt;
        double perfWindowHeight = perfHVar.toDouble();

        QVariant valHVar = session.getAttribute(SessionKeys::SpValWindowHeight);
        if (!valHVar.canConvert<double>())
            return std::nullopt;
        double valWindowHeight = valHVar.toDouble();

        QVariant breakoffVar = session.getAttribute(SessionKeys::SpBreakoffAlt);
        if (!breakoffVar.canConvert<double>())
            return std::nullopt;
        double breakoffAlt = breakoffVar.toDouble();

        QVariant startTimeVar = session.getAttribute(SessionKeys::SpWindowStartTime);
        if (!startTimeVar.canConvert<double>())
            return std::nullopt;
        double windowStartTime = startTimeVar.toDouble();

        QVariant startAltVar = session.getAttribute(SessionKeys::SpWindowStartAlt);
        if (!startAltVar.canConvert<double>())
            return std::nullopt;
        double windowStartAlt = startAltVar.toDouble();

        // Retrieve measurement vectors
        QVector<double> z    = session.getMeasurement("GNSS", "z");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);
        QVector<double> vAcc = session.getMeasurement("GNSS", "vAcc");

        if (z.isEmpty() || time.isEmpty())
            return std::nullopt;
        if (z.size() != time.size())
            return std::nullopt;

        const int n = z.size();

        // Determine performance window end altitude.
        // z is already AGL (hMSL - groundElev), so breakoffAlt is compared directly.
        // Higher altitude = reached first during descent.
        double windowEndAlt = std::max(windowStartAlt - perfWindowHeight,
                                       breakoffAlt);

        // Find window end time (first downward crossing of windowEndAlt after window start)
        double windowEndTime = -1.0;
        int windowEndIdx = -1;
        for (int i = 1; i < n; ++i) {
            if (time[i] < windowStartTime)
                continue;
            if (z[i - 1] >= windowEndAlt && z[i] < windowEndAlt) {
                double a = (windowEndAlt - z[i - 1]) / (z[i] - z[i - 1]);
                windowEndTime = time[i - 1] + a * (time[i] - time[i - 1]);
                windowEndIdx = i;
                break;
            }
        }

        auto setAllNull = [&]() {
            session.setCalculatedAttribute(SessionKeys::SpWindowEndTime, QVariant());
            session.setCalculatedAttribute(SessionKeys::SpBestStartTime, QVariant());
            session.setCalculatedAttribute(SessionKeys::SpBestEndTime,   QVariant());
            session.setCalculatedAttribute(SessionKeys::SpSpeedResult,   QVariant());
            session.setCalculatedAttribute(SessionKeys::SpMaxSpeedAcc,   QVariant());
        };

        if (windowEndTime < 0.0) {
            setAllNull();
            return session.getAttribute(outputKey);
        }

        session.setCalculatedAttribute(SessionKeys::SpWindowEndTime, windowEndTime);

        // Find the fastest 3-second average vertical speed within the performance window.
        // For each sample i within the window, compute the altitude drop over the next 3 seconds.
        double bestSpeed = -1.0;
        double bestStartTime = 0.0;
        double bestEndTime = 0.0;

        // Find first sample at or after window start time
        int firstIdx = 0;
        for (int i = 0; i < n; ++i) {
            if (time[i] >= windowStartTime) {
                firstIdx = i;
                break;
            }
        }

        // Use a advancing-index approach for the +3s endpoint
        int j = firstIdx;
        for (int i = firstIdx; i < n; ++i) {
            if (time[i] > windowEndTime)
                break;

            double tStart = time[i];
            double tEnd = tStart + 3.0;

            // The 3s interval must fit within the performance window
            if (tEnd > windowEndTime)
                break;

            // Advance j until time[j] >= tEnd
            while (j < n - 1 && time[j] < tEnd)
                ++j;

            if (j >= n || time[j] < tEnd)
                break;

            // Interpolate altitude at tEnd
            // j is the first sample with time[j] >= tEnd
            double zEnd;
            if (j > 0 && time[j - 1] < tEnd) {
                double a = (tEnd - time[j - 1]) / (time[j] - time[j - 1]);
                zEnd = z[j - 1] + a * (z[j] - z[j - 1]);
            } else {
                zEnd = z[j];
            }

            // speed = altitude drop / 3 seconds (positive during descent)
            double speed = (z[i] - zEnd) / 3.0;

            if (speed > bestSpeed) {
                bestSpeed = speed;
                bestStartTime = tStart;
                bestEndTime = tEnd;
            }
        }

        if (bestSpeed >= 0.0) {
            session.setCalculatedAttribute(SessionKeys::SpBestStartTime, bestStartTime);
            session.setCalculatedAttribute(SessionKeys::SpBestEndTime,   bestEndTime);
            session.setCalculatedAttribute(SessionKeys::SpSpeedResult,   bestSpeed);
        } else {
            session.setCalculatedAttribute(SessionKeys::SpBestStartTime, QVariant());
            session.setCalculatedAttribute(SessionKeys::SpBestEndTime,   QVariant());
            session.setCalculatedAttribute(SessionKeys::SpSpeedResult,   QVariant());
        }

        // Compute max speed accuracy within the validation window.
        // Start at the window end crossing and walk backward until altitude
        // exceeds windowEndAlt + valWindowHeight.  This avoids including
        // samples from the aircraft climb that happen to be at the same altitude.
        double valTopAlt = windowEndAlt + valWindowHeight;

        if (vAcc.size() == n && windowEndIdx > 0) {
            double maxSpeedAcc = -1.0;
            for (int i = windowEndIdx; i >= 0; --i) {
                if (z[i] > valTopAlt)
                    break;
                double speedAcc = std::sqrt(2.0) * vAcc[i] / 3.0;
                if (speedAcc > maxSpeedAcc)
                    maxSpeedAcc = speedAcc;
            }

            if (maxSpeedAcc >= 0.0)
                session.setCalculatedAttribute(SessionKeys::SpMaxSpeedAcc, maxSpeedAcc);
            else
                session.setCalculatedAttribute(SessionKeys::SpMaxSpeedAcc, QVariant());
        } else {
            session.setCalculatedAttribute(SessionKeys::SpMaxSpeedAcc, QVariant());
        }

        return session.getAttribute(outputKey);
    };

    // Dependency list shared by all result attributes
    QVector<DependencyKey> spResultDeps = {
        DependencyKey::attribute(SessionKeys::SpPerfWindowHeight),
        DependencyKey::attribute(SessionKeys::SpValWindowHeight),
        DependencyKey::attribute(SessionKeys::SpBreakoffAlt),
        DependencyKey::attribute(SessionKeys::ExitTime),
        DependencyKey::attribute(SessionKeys::SpWindowStartTime),
        DependencyKey::attribute(SessionKeys::SpWindowStartAlt),
        DependencyKey::measurement("GNSS", "z"),
        DependencyKey::measurement("GNSS", SessionKeys::Time),
        DependencyKey::measurement("GNSS", "vAcc")
    };

    SessionData::registerCalculatedAttribute(
        SessionKeys::SpWindowEndTime,
        spResultDeps,
        [computeSpResults](SessionData& session) -> std::optional<QVariant> {
            return computeSpResults(session, SessionKeys::SpWindowEndTime);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::SpBestStartTime,
        spResultDeps,
        [computeSpResults](SessionData& session) -> std::optional<QVariant> {
            return computeSpResults(session, SessionKeys::SpBestStartTime);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::SpBestEndTime,
        spResultDeps,
        [computeSpResults](SessionData& session) -> std::optional<QVariant> {
            return computeSpResults(session, SessionKeys::SpBestEndTime);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::SpSpeedResult,
        spResultDeps,
        [computeSpResults](SessionData& session) -> std::optional<QVariant> {
            return computeSpResults(session, SessionKeys::SpSpeedResult);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::SpMaxSpeedAcc,
        spResultDeps,
        [computeSpResults](SessionData& session) -> std::optional<QVariant> {
            return computeSpResults(session, SessionKeys::SpMaxSpeedAcc);
        });

    // ── Group C: Marker registration (4 markers) ────────────────────────

    QVector<MarkerDefinition> defs;

    MarkerDefinition windowStartDef;
    windowStartDef.category       = QStringLiteral("Speed Skydiving");
    windowStartDef.displayName    = QStringLiteral("Window Start");
    windowStartDef.shortLabel     = QStringLiteral("Start");
    windowStartDef.color          = QColor(0, 128, 0);
    windowStartDef.attributeKey   = SessionKeys::SpWindowStartTime;
    windowStartDef.measurements   = {};
    windowStartDef.editable       = false;
    windowStartDef.groupId        = QStringLiteral("sp");
    windowStartDef.defaultEnabled = true;
    defs.append(windowStartDef);

    MarkerDefinition windowEndDef;
    windowEndDef.category       = QStringLiteral("Speed Skydiving");
    windowEndDef.displayName    = QStringLiteral("Window End");
    windowEndDef.shortLabel     = QStringLiteral("End");
    windowEndDef.color          = QColor(0, 128, 0);
    windowEndDef.attributeKey   = SessionKeys::SpWindowEndTime;
    windowEndDef.measurements   = {};
    windowEndDef.editable       = false;
    windowEndDef.groupId        = QStringLiteral("sp");
    windowEndDef.defaultEnabled = true;
    defs.append(windowEndDef);

    MarkerDefinition bestStartDef;
    bestStartDef.category       = QStringLiteral("Speed Skydiving");
    bestStartDef.displayName    = QStringLiteral("Best 3s Start");
    bestStartDef.shortLabel     = QStringLiteral("3s\u2191");
    bestStartDef.color          = QColor(0, 0, 192);
    bestStartDef.attributeKey   = SessionKeys::SpBestStartTime;
    bestStartDef.measurements   = {};
    bestStartDef.editable       = false;
    bestStartDef.groupId        = QStringLiteral("sp");
    bestStartDef.defaultEnabled = true;
    defs.append(bestStartDef);

    MarkerDefinition bestEndDef;
    bestEndDef.category       = QStringLiteral("Speed Skydiving");
    bestEndDef.displayName    = QStringLiteral("Best 3s End");
    bestEndDef.shortLabel     = QStringLiteral("3s\u2193");
    bestEndDef.color          = QColor(0, 0, 192);
    bestEndDef.attributeKey   = SessionKeys::SpBestEndTime;
    bestEndDef.measurements   = {};
    bestEndDef.editable       = false;
    bestEndDef.groupId        = QStringLiteral("sp");
    bestEndDef.defaultEnabled = true;
    defs.append(bestEndDef);

    MarkerRegistry::instance()->replaceMarkerGroup(QStringLiteral("sp"), defs);

    // ── Group D: Attribute registry entries ──────────────────────────────

    auto& reg = AttributeRegistry::instance();

    reg.registerAttribute({
        QStringLiteral("Speed Skydiving"),
        QStringLiteral("Speed"),
        SessionKeys::SpSpeedResult,
        AttributeFormatType::Double,
        false,
        MeasurementTypes::SpSpeed
    });

    reg.registerAttribute({
        QStringLiteral("Speed Skydiving"),
        QStringLiteral("Speed Accuracy"),
        SessionKeys::SpMaxSpeedAcc,
        AttributeFormatType::Double,
        false,
        MeasurementTypes::SpSpeedAcc
    });
}
