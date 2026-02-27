#include "markerregistry.h"
#include "sessiondata.h"
#include "dependencykey.h"

#include <QDateTime>
#include <QTimeZone>
#include <algorithm>

using namespace FlySight;

namespace {

/// Build a lambda that interpolates a sensor measurement at a marker's time.
/// The marker time is read from the session attribute identified by
/// \a timeAttributeKey (a QDateTime stored in UTC seconds).  The measurement
/// is read from the sensor/measurement pair.  Linear interpolation between
/// the two bracketing samples is performed; std::nullopt is returned for
/// every failure case.
SessionData::AttributeFunction makeInterpolationFunction(
    const QString &timeAttributeKey,
    const QString &sensor,
    const QString &measurement)
{
    return [timeAttributeKey, sensor, measurement](SessionData &session)
        -> std::optional<QVariant>
    {
        // 1. Read the marker's time attribute
        QVariant var = session.getAttribute(timeAttributeKey);
        if (!var.canConvert<QDateTime>())
            return std::nullopt;

        QDateTime dt = var.toDateTime();
        if (!dt.isValid())
            return std::nullopt;

        // 2. Convert QDateTime to UTC seconds (double)
        const double markerTime = dt.toMSecsSinceEpoch() / 1000.0;

        // 3. Read the sensor's time vector and measurement vector
        const QVector<double> timeVec = session.getMeasurement(sensor, SessionKeys::Time);
        if (timeVec.isEmpty())
            return std::nullopt;

        const QVector<double> dataVec = session.getMeasurement(sensor, measurement);
        if (dataVec.isEmpty() || dataVec.size() != timeVec.size())
            return std::nullopt;

        // 4. Find bracketing samples and linearly interpolate
        auto it = std::lower_bound(timeVec.cbegin(), timeVec.cend(), markerTime);
        if (it == timeVec.cbegin() || it == timeVec.cend())
            return std::nullopt;

        const int idx = static_cast<int>(std::distance(timeVec.cbegin(), it));
        const double t1 = timeVec[idx - 1], v1 = dataVec[idx - 1];
        const double t2 = timeVec[idx],     v2 = dataVec[idx];

        // Guard against degenerate segment
        if (t2 == t1)
            return std::nullopt;

        const double result = v1 + (v2 - v1) * (markerTime - t1) / (t2 - t1);

        // 5. Return the interpolated value
        return QVariant(result);
    };
}

} // anonymous namespace

MarkerRegistry& MarkerRegistry::instance() {
    static MarkerRegistry R;
    return R;
}

void MarkerRegistry::registerMarker(const MarkerDefinition& def) {
    m_markers.append(def);

    // Auto-register interpolation-based calculated attributes for each
    // measurement associated with this marker.
    for (const auto &mk : def.measurements) {
        QString valueKey = def.attributeKey
            + QStringLiteral(":")
            + mk.first
            + QStringLiteral("/")
            + mk.second;

        QList<DependencyKey> deps = {
            DependencyKey::attribute(def.attributeKey),
            DependencyKey::measurement(mk.first, SessionKeys::Time),
            DependencyKey::measurement(mk.first, mk.second)
        };

        SessionData::registerCalculatedAttribute(
            valueKey,
            deps,
            makeInterpolationFunction(def.attributeKey, mk.first, mk.second));
    }
}

QVector<MarkerDefinition> MarkerRegistry::allMarkers() const {
    return m_markers;
}
