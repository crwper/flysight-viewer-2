#include "simplificationcalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <GeographicLib/LocalCartesian.hpp>
#include <vector>
#include <cmath>

// --- FIX FOR GTSAM LINKING ERROR ---
// When GTSAM is built with TBB support, it uses tbb::tbb_allocator for its
// FastVector types (including std::vector<size_t>). To avoid linker errors
// (LNK2019) when this file uses std::vector<size_t> and indirectly pulls in
// GTSAM's exported symbols, we must import the TBB-allocated version.
#ifdef _MSC_VER
#include <tbb/tbb_allocator.h>
template class __declspec(dllimport) std::vector<size_t, tbb::tbb_allocator<size_t>>;
#endif
// -----------------------------------

using namespace FlySight;

void Calculations::registerSimplificationCalculations()
{
    // Define the keys we want to expose
    struct SimpOutputMapping {
        QString key;
        int vectorIndex; // 0=lat, 1=lon, 2=alt, 3=time
    };

    // We will generate 4 vectors.
    // We register the calculation trigger on "lat", but it populates all 4.
    static const std::vector<SimpOutputMapping> simp_outputs = {
        {"lat", 0}, {"lon", 1}, {"hMSL", 2}, {SessionKeys::Time, 3}
    };

    auto compute_simplified_track = [](SessionData &session, const QString &outputKey) -> std::optional<QVector<double>> {
        // 1. Gather Dependencies
        QVector<double> rawLat = session.getMeasurement("GNSS", "lat");
        QVector<double> rawLon = session.getMeasurement("GNSS", "lon");
        QVector<double> rawAlt = session.getMeasurement("GNSS", "hMSL");
        QVector<double> rawTime = session.getMeasurement("GNSS", SessionKeys::Time);

        if (rawLat.isEmpty() || rawLat.size() != rawLon.size() ||
            rawLat.size() != rawAlt.size() || rawLat.size() != rawTime.size()) {
            return std::nullopt;
        }

        // 2. Setup Geometry Types
        namespace bg = boost::geometry;
        using PointXY = bg::model::d2::point_xy<double>;
        using LineString = bg::model::linestring<PointXY>;

        // 3. Project to Local Cartesian (Meters)
        // Center projection on the first point
        GeographicLib::LocalCartesian proj(rawLat[0], rawLon[0], rawAlt[0]);

        LineString pathInMeters;
        pathInMeters.reserve(rawLat.size());

        // We store the original index to retrieve the correct timestamp/altitude later
        // Map: PointIndex -> OriginalIndex
        std::vector<size_t> indexMap;
        indexMap.reserve(rawLat.size());

        for (int i = 0; i < rawLat.size(); ++i) {
            double x, y, z;
            proj.Forward(rawLat[i], rawLon[i], rawAlt[i], x, y, z);

            // Note: RDP is 2D simplification. If vertical simplifcation is critical,
            // you need a 3D point type, but usually 2D (ground track) is sufficient for maps.
            bg::append(pathInMeters, PointXY(x, y));
        }

        // 4. Run RDP Simplification
        // Epsilon: 0.5 meters (Sensor noise floor)
        LineString simplifiedPath;
        bg::simplify(pathInMeters, simplifiedPath, 0.5);

        // 5. Unproject & Reconstruct
        // WARNING: RDP destroys indices. However, Boost's simplified points
        // are a subset of original points (usually).
        // BUT: Floating point errors during projection/unprojection might make
        // strict equality checks on lat/lon risky for finding the original timestamp.

        // ROBUST APPROACH:
        // Since we need Time and Alt synced, we should ideally simplify the 3D structure
        // or map back to the nearest original point.
        // For strict RDP, the points in `simplifiedPath` correspond exactly to specific
        // indices in `pathInMeters`.

        // To keep this snippet simple, let's reverse project the X/Y to get Lat/Lon,
        // and linearly interpolate Time/Alt based on the cumulative distance or
        // simply perform a nearest-neighbor search on the original raw data
        // to recover the timestamp.

        // Let's use a simplified strategy:
        // Re-project the simplified XY back to Lat/Lon.
        // For Time/Alt, we must rely on the fact that RDP preserves vertices.
        // We can iterate the original list to find the matching points.

        QVector<double> outLat, outLon, outAlt, outTime;
        outLat.reserve(simplifiedPath.size());
        outLon.reserve(simplifiedPath.size());
        outAlt.reserve(simplifiedPath.size());
        outTime.reserve(simplifiedPath.size());

        size_t rawIdx = 0;
        for (const auto& pt : simplifiedPath) {
            // Find this point in the original path (it must exist, in order)
            // We search forward from the last found index.
            // We compare in Projected Meter Space to avoid float fuzziness.

            double targetX = pt.x();
            double targetY = pt.y();

            // Simple tolerance search
            for (; rawIdx < rawLat.size(); ++rawIdx) {
                double x, y, z;
                proj.Forward(rawLat[rawIdx], rawLon[rawIdx], rawAlt[rawIdx], x, y, z);

                if (std::abs(x - targetX) < 1e-3 && std::abs(y - targetY) < 1e-3) {
                    // Match found
                    outLat.append(rawLat[rawIdx]);
                    outLon.append(rawLon[rawIdx]);
                    outAlt.append(rawAlt[rawIdx]);
                    outTime.append(rawTime[rawIdx]);
                    break;
                }
            }
        }

        // 6. Store outputs in SessionData (Cache)
        // Store everything *except* the one we are about to return
        // (to avoid double-setting logic if you want, though setCalculatedMeasurement is safe)

        session.setCalculatedMeasurement("Simplified", "lat", outLat);
        session.setCalculatedMeasurement("Simplified", "lon", outLon);
        session.setCalculatedMeasurement("Simplified", "hMSL", outAlt);
        session.setCalculatedMeasurement("Simplified", SessionKeys::Time, outTime);

        // 7. Return the specific requested vector
        if (outputKey == "lat") return outLat;
        if (outputKey == "lon") return outLon;
        if (outputKey == "hMSL") return outAlt;
        if (outputKey == SessionKeys::Time) return outTime;

        return std::nullopt;
    };

    // Register the dependency
    for (const auto &entry : simp_outputs) {
        SessionData::registerCalculatedMeasurement(
            "Simplified", entry.key,
            {
                // Dependency on RAW GNSS data
                DependencyKey::measurement("GNSS", "lat"),
                DependencyKey::measurement("GNSS", "lon"),
                DependencyKey::measurement("GNSS", "hMSL"),
                DependencyKey::measurement("GNSS", SessionKeys::Time)
            },
            [compute_simplified_track, key = entry.key](SessionData &s) {
                return compute_simplified_track(s, key);
            });
    }
}
