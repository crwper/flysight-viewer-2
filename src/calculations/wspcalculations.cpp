#include "wspcalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include "../markerregistry.h"
#include "../attributeregistry.h"
#include "../units/unitdefinitions.h"
#include <GeographicLib/Geodesic.hpp>
#include <QColor>

using namespace FlySight;

void Calculations::registerWspCalculations()
{
    // ── Group A: Parameter defaults (5 calculated attributes) ──────────

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspVersion,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(QStringLiteral("1.0"));
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspTopAlt,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(2500.0);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspBottomAlt,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(1500.0);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspTask,
        {},
        [](SessionData&) -> std::optional<QVariant> {
            return QVariant(QStringLiteral("Time"));
        });

    // ── Group A2: Lane Reference 1 (Ref1) ─────────────────────────────
    // Ref1 = 9 seconds after the competitor's vertical speed first
    // reaches 10 m/s (starting from exit).  Used as the validation
    // window start and, later, as a lane reference endpoint.

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspRef1Time,
        {
            DependencyKey::attribute(SessionKeys::ExitTime),
            DependencyKey::measurement("GNSS", "velD"),
            DependencyKey::measurement("GNSS", SessionKeys::Time)
        },
        [](SessionData& session) -> std::optional<QVariant> {
            QVariant exitVar = session.getAttribute(SessionKeys::ExitTime);
            if (!exitVar.canConvert<double>())
                return std::nullopt;
            double exitTime = exitVar.toDouble();

            QVector<double> velD = session.getMeasurement("GNSS", "velD");
            QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

            if (velD.isEmpty() || time.isEmpty() || velD.size() != time.size())
                return std::nullopt;

            const int n = velD.size();
            const double vThreshold = 10.0;

            for (int i = 1; i < n; ++i) {
                if (time[i] < exitTime)
                    continue;
                if (velD[i - 1] < vThreshold && velD[i] >= vThreshold) {
                    double a = (vThreshold - velD[i - 1]) / (velD[i] - velD[i - 1]);
                    double crossingTime = time[i - 1] + a * (time[i] - time[i - 1]);
                    return QVariant(crossingTime + 9.0);
                }
            }

            return std::nullopt;
        });

    // ── Group B: Window gate crossing and result calculations ──────────

    auto computeWspResults = [](SessionData& session, const QString& outputKey) -> std::optional<QVariant> {
        // Retrieve parameters
        QVariant topVar = session.getAttribute(SessionKeys::WspTopAlt);
        if (!topVar.canConvert<double>())
            return std::nullopt;
        double topAlt = topVar.toDouble();

        QVariant botVar = session.getAttribute(SessionKeys::WspBottomAlt);
        if (!botVar.canConvert<double>())
            return std::nullopt;
        double bottomAlt = botVar.toDouble();

        QVariant exitVar = session.getAttribute(SessionKeys::ExitTime);
        if (!exitVar.canConvert<double>())
            return std::nullopt;
        double exitTime = exitVar.toDouble();

        QVariant geVar = session.getAttribute(SessionKeys::GroundElev);
        if (!geVar.canConvert<double>())
            return std::nullopt;

        // Retrieve measurement vectors
        QVector<double> z    = session.getMeasurement("GNSS", "z");
        QVector<double> lat  = session.getMeasurement("GNSS", "lat");
        QVector<double> lon  = session.getMeasurement("GNSS", "lon");
        QVector<double> time = session.getMeasurement("GNSS", SessionKeys::Time);

        if (z.isEmpty() || lat.isEmpty() || lon.isEmpty() || time.isEmpty())
            return std::nullopt;
        if (z.size() != time.size() || lat.size() != time.size() || lon.size() != time.size())
            return std::nullopt;

        const int n = z.size();

        // Find the starting index: first sample where time >= exitTime
        int startIdx = -1;
        for (int i = 0; i < n; ++i) {
            if (time[i] >= exitTime) {
                startIdx = i;
                break;
            }
        }
        if (startIdx < 0)
            return std::nullopt;

        // Find window entry (first downward crossing of topAlt starting from startIdx)
        double entryTime = 0.0;
        double entryLat  = 0.0;
        double entryLon  = 0.0;
        bool foundEntry  = false;

        for (int i = std::max(startIdx, 1); i < n; ++i) {
            if (z[i - 1] >= topAlt && z[i] < topAlt) {
                double a = (topAlt - z[i - 1]) / (z[i] - z[i - 1]);
                entryTime = time[i - 1] + a * (time[i] - time[i - 1]);
                entryLat  = lat[i - 1]  + a * (lat[i]  - lat[i - 1]);
                entryLon  = lon[i - 1]  + a * (lon[i]  - lon[i - 1]);
                foundEntry = true;
                startIdx = i; // continue search from here for the bottom gate
                break;
            }
        }

        if (!foundEntry) {
            // No entry crossing found -- all results are nullopt
            session.setCalculatedAttribute(SessionKeys::WspEntryTime,   QVariant());
            session.setCalculatedAttribute(SessionKeys::WspExitTime,    QVariant());
            session.setCalculatedAttribute(SessionKeys::WspEntryLat,    QVariant());
            session.setCalculatedAttribute(SessionKeys::WspEntryLon,    QVariant());
            session.setCalculatedAttribute(SessionKeys::WspExitLat,     QVariant());
            session.setCalculatedAttribute(SessionKeys::WspExitLon,     QVariant());
            session.setCalculatedAttribute(SessionKeys::WspTimeResult,  QVariant());
            session.setCalculatedAttribute(SessionKeys::WspDistResult,  QVariant());
            session.setCalculatedAttribute(SessionKeys::WspSpeedResult, QVariant());
            session.setCalculatedAttribute(SessionKeys::WspSepResult,   QVariant());
            return session.getAttribute(outputKey);
        }

        // Store entry results
        session.setCalculatedAttribute(SessionKeys::WspEntryTime, entryTime);
        session.setCalculatedAttribute(SessionKeys::WspEntryLat,  entryLat);
        session.setCalculatedAttribute(SessionKeys::WspEntryLon,  entryLon);

        // Find window exit (first downward crossing of bottomAlt after entry)
        double wspExitTime = 0.0;
        double exitLat     = 0.0;
        double exitLon     = 0.0;
        bool foundExit     = false;
        int exitIdx        = -1;

        for (int i = startIdx; i < n; ++i) {
            if (i < 1) continue;
            if (z[i - 1] >= bottomAlt && z[i] < bottomAlt) {
                double a = (bottomAlt - z[i - 1]) / (z[i] - z[i - 1]);
                wspExitTime = time[i - 1] + a * (time[i] - time[i - 1]);
                exitLat     = lat[i - 1]  + a * (lat[i]  - lat[i - 1]);
                exitLon     = lon[i - 1]  + a * (lon[i]  - lon[i - 1]);
                foundExit   = true;
                exitIdx     = i;
                break;
            }
        }

        if (!foundExit) {
            // Entry found but no exit crossing
            session.setCalculatedAttribute(SessionKeys::WspExitTime,    QVariant());
            session.setCalculatedAttribute(SessionKeys::WspExitLat,     QVariant());
            session.setCalculatedAttribute(SessionKeys::WspExitLon,     QVariant());
            session.setCalculatedAttribute(SessionKeys::WspTimeResult,  QVariant());
            session.setCalculatedAttribute(SessionKeys::WspDistResult,  QVariant());
            session.setCalculatedAttribute(SessionKeys::WspSpeedResult, QVariant());
            session.setCalculatedAttribute(SessionKeys::WspSepResult,   QVariant());
            return session.getAttribute(outputKey);
        }

        // Store exit results
        session.setCalculatedAttribute(SessionKeys::WspExitTime, wspExitTime);
        session.setCalculatedAttribute(SessionKeys::WspExitLat,  exitLat);
        session.setCalculatedAttribute(SessionKeys::WspExitLon,  exitLon);

        // Compute derived results
        double timeResult = wspExitTime - entryTime;
        session.setCalculatedAttribute(SessionKeys::WspTimeResult, timeResult);

        double dist = 0.0;
        GeographicLib::Geodesic::WGS84().Inverse(entryLat, entryLon, exitLat, exitLon, dist);
        session.setCalculatedAttribute(SessionKeys::WspDistResult, dist);

        if (timeResult > 0.0) {
            session.setCalculatedAttribute(SessionKeys::WspSpeedResult, dist / timeResult);
        } else {
            session.setCalculatedAttribute(SessionKeys::WspSpeedResult, QVariant());
        }

        // Compute max SEP within the validation window.
        // Start: Lane Reference 1 (9 s after vertical speed first reaches 10 m/s).
        // End:   20 m below the bottom of the competition window.
        QVariant ref1Var = session.getAttribute(SessionKeys::WspRef1Time);
        QVector<double> hAcc = session.getMeasurement("GNSS", "hAcc");
        QVector<double> vAcc = session.getMeasurement("GNSS", "vAcc");

        if (ref1Var.canConvert<double>() && hAcc.size() == n && vAcc.size() == n) {
            double ref1Time = ref1Var.toDouble();

            // Find first sample at or after Ref1 time
            int valStart = -1;
            for (int i = 0; i < n; ++i) {
                if (time[i] >= ref1Time) {
                    valStart = i;
                    break;
                }
            }

            // Search forward from exit crossing until altitude <= bottomAlt - 20
            int valEnd = exitIdx;
            for (int i = exitIdx + 1; i < n; ++i) {
                if (z[i] <= bottomAlt - 20.0)
                    break;
                valEnd = i;
            }

            double maxSep = -1.0;
            if (valStart >= 0 && valStart <= valEnd) {
                for (int i = valStart; i <= valEnd; ++i) {
                    double sep = 0.5127 * (2.0 * hAcc[i] + vAcc[i]);
                    if (sep > maxSep)
                        maxSep = sep;
                }
            }

            if (maxSep >= 0.0)
                session.setCalculatedAttribute(SessionKeys::WspSepResult, maxSep);
            else
                session.setCalculatedAttribute(SessionKeys::WspSepResult, QVariant());
        } else {
            session.setCalculatedAttribute(SessionKeys::WspSepResult, QVariant());
        }

        return session.getAttribute(outputKey);
    };

    // Dependency list shared by all result attributes
    QVector<DependencyKey> wspResultDeps = {
        DependencyKey::attribute(SessionKeys::WspTopAlt),
        DependencyKey::attribute(SessionKeys::WspBottomAlt),
        DependencyKey::attribute(SessionKeys::ExitTime),
        DependencyKey::attribute(SessionKeys::GroundElev),
        DependencyKey::measurement("GNSS", "z"),
        DependencyKey::measurement("GNSS", "lat"),
        DependencyKey::measurement("GNSS", "lon"),
        DependencyKey::measurement("GNSS", SessionKeys::Time),
        DependencyKey::measurement("GNSS", "hAcc"),
        DependencyKey::measurement("GNSS", "vAcc"),
        DependencyKey::attribute(SessionKeys::WspRef1Time)
    };

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspEntryTime,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspEntryTime);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspExitTime,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspExitTime);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspEntryLat,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspEntryLat);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspEntryLon,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspEntryLon);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspExitLat,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspExitLat);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspExitLon,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspExitLon);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspTimeResult,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspTimeResult);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspDistResult,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspDistResult);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspSpeedResult,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspSpeedResult);
        });

    SessionData::registerCalculatedAttribute(
        SessionKeys::WspSepResult,
        wspResultDeps,
        [computeWspResults](SessionData& session) -> std::optional<QVariant> {
            return computeWspResults(session, SessionKeys::WspSepResult);
        });

    // ── Group C: Marker registration (3 markers) ──────────────────────

    QVector<MarkerDefinition> defs;

    MarkerDefinition ref1Def;
    ref1Def.category       = QStringLiteral("Wingsuit Performance");
    ref1Def.displayName    = QStringLiteral("Lane Reference 1");
    ref1Def.shortLabel     = QStringLiteral("Ref1");
    ref1Def.color          = QColor(0, 128, 0);
    ref1Def.attributeKey   = SessionKeys::WspRef1Time;
    ref1Def.measurements   = {};
    ref1Def.editable       = false;
    ref1Def.groupId        = QStringLiteral("wsp");
    ref1Def.defaultEnabled = true;
    defs.append(ref1Def);

    MarkerDefinition topDef;
    topDef.category       = QStringLiteral("Wingsuit Performance");
    topDef.displayName    = QStringLiteral("Window Top");
    topDef.shortLabel     = QStringLiteral("Top");
    topDef.color          = QColor(0, 128, 0);
    topDef.attributeKey   = SessionKeys::WspEntryTime;
    topDef.measurements   = {};
    topDef.editable       = false;
    topDef.groupId        = QStringLiteral("wsp");
    topDef.defaultEnabled = true;
    defs.append(topDef);

    MarkerDefinition botDef;
    botDef.category       = QStringLiteral("Wingsuit Performance");
    botDef.displayName    = QStringLiteral("Window Bottom");
    botDef.shortLabel     = QStringLiteral("Bot");
    botDef.color          = QColor(0, 128, 0);
    botDef.attributeKey   = SessionKeys::WspExitTime;
    botDef.measurements   = {};
    botDef.editable       = false;
    botDef.groupId        = QStringLiteral("wsp");
    botDef.defaultEnabled = true;
    defs.append(botDef);

    MarkerRegistry::instance()->replaceMarkerGroup(QStringLiteral("wsp"), defs);

    // ── Group D: Attribute registry entries (3 registrations) ─────────

    auto& reg = AttributeRegistry::instance();

    reg.registerAttribute({
        QStringLiteral("Wingsuit Performance"),
        QStringLiteral("Task"),
        SessionKeys::WspTask,
        AttributeFormatType::Text,
        true,
        {}
    });

    reg.registerAttribute({
        QStringLiteral("Wingsuit Performance"),
        QStringLiteral("Time"),
        SessionKeys::WspTimeResult,
        AttributeFormatType::Double,
        false,
        MeasurementTypes::WspTime
    });

    reg.registerAttribute({
        QStringLiteral("Wingsuit Performance"),
        QStringLiteral("Distance"),
        SessionKeys::WspDistResult,
        AttributeFormatType::Double,
        false,
        MeasurementTypes::WspDistance
    });

    reg.registerAttribute({
        QStringLiteral("Wingsuit Performance"),
        QStringLiteral("Speed"),
        SessionKeys::WspSpeedResult,
        AttributeFormatType::Double,
        false,
        MeasurementTypes::WspSpeed
    });

    reg.registerAttribute({
        QStringLiteral("Wingsuit Performance"),
        QStringLiteral("SEP"),
        SessionKeys::WspSepResult,
        AttributeFormatType::Double,
        false,
        MeasurementTypes::WspSep
    });
}
