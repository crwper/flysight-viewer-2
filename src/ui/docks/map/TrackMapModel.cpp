#include "TrackMapModel.h"

#include "sessionmodel.h"
#include "sessiondata.h"
#include "plotrangemodel.h"
#include "plotutils.h"

#include <QtMath>
#include <QVariantMap>
#include <QDateTime>
#include <limits>

namespace FlySight {

static constexpr const char *kDefaultSensor = "Simplified";
static constexpr const char *kLatKey = "lat";
static constexpr const char *kLonKey = "lon";

TrackMapModel::TrackMapModel(SessionModel *sessionModel,
                             PlotRangeModel *rangeModel,
                             QObject *parent)
    : QAbstractListModel(parent)
    , m_sessionModel(sessionModel)
    , m_rangeModel(rangeModel)
{
    if (m_sessionModel) {
        connect(m_sessionModel, &SessionModel::modelChanged,
                this, &TrackMapModel::rebuild);
    }

    if (m_rangeModel) {
        connect(m_rangeModel, &PlotRangeModel::rangeChanged,
                this, &TrackMapModel::rebuild);
    }

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &TrackMapModel::onPreferenceChanged);

    // Load initial preference value
    m_trackOpacity = PreferencesManager::instance().getValue(
        PreferenceKeys::MapTrackOpacity).toDouble();

    rebuild();
}

int TrackMapModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tracks.size();
}

QVariant TrackMapModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return QVariant();

    const Track &t = m_tracks.at(index.row());

    switch (role) {
    case SessionIdRole:
        return t.sessionId;
    case TrackPointsRole:
        return t.points;
    case TrackColorRole:
        return t.color;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TrackMapModel::roleNames() const
{
    return {
        { SessionIdRole,    "sessionId" },
        { TrackPointsRole,  "trackPoints" },
        { TrackColorRole,   "trackColor" }
    };
}

QColor TrackMapModel::colorForSession(const QString &sessionId) const
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);
    c.setAlphaF(m_trackOpacity);
    return c;
}

void TrackMapModel::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::MapTrackOpacity) {
        m_trackOpacity = value.toDouble();
        rebuild(); // Rebuild tracks with new opacity
    }
}

bool TrackMapModel::computeSessionUtcRange(const SessionData &session,
                                           double *outLower, double *outUpper) const
{
    if (!m_rangeModel || !m_rangeModel->hasRange())
        return false;

    const QString refKey = m_rangeModel->referenceMarkerKey();
    const auto optOffset = markerOffsetUtcSeconds(session, refKey);
    if (!optOffset.has_value())
        return false;
    const double offset = *optOffset;

    *outLower = m_rangeModel->rangeLower() + offset;
    *outUpper = m_rangeModel->rangeUpper() + offset;
    return true;
}

void TrackMapModel::rebuild()
{
    const bool oldHasData = m_hasData;
    const int oldCount = m_tracks.size();
    const double oldCenterLat = m_centerLat;
    const double oldCenterLon = m_centerLon;
    const double oldBoundsNorth = m_boundsNorth;
    const double oldBoundsSouth = m_boundsSouth;
    const double oldBoundsEast = m_boundsEast;
    const double oldBoundsWest = m_boundsWest;

    beginResetModel();
    m_tracks.clear();

    bool haveBounds = false;
    double minLat = 0.0, maxLat = 0.0, minLon = 0.0, maxLon = 0.0;

    if (m_sessionModel) {
        const auto &sessions = m_sessionModel->getAllSessions();

        for (const auto &session : sessions) {
            if (!session.isVisible())
                continue;

            const QVector<double> lat =
                session.getMeasurement(QString::fromLatin1(kDefaultSensor), QString::fromLatin1(kLatKey));
            const QVector<double> lon =
                session.getMeasurement(QString::fromLatin1(kDefaultSensor), QString::fromLatin1(kLonKey));
            const QVector<double> tUtc =
                session.getMeasurement(QString::fromLatin1(kDefaultSensor), QString::fromLatin1(SessionKeys::Time));

            const int n = qMin(tUtc.size(), qMin(lat.size(), lon.size()));
            if (n < 2)
                continue;

            // Compute UTC range filter for this session
            double filterLower = -std::numeric_limits<double>::infinity();
            double filterUpper = std::numeric_limits<double>::infinity();
            computeSessionUtcRange(session, &filterLower, &filterUpper);

            QVariantList points;
            points.reserve(n);

            for (int i = 0; i < n; ++i) {
                const double la = lat[i];
                const double lo = lon[i];
                const double tt = tUtc[i];

                if (!qIsFinite(la) || !qIsFinite(lo) || !qIsFinite(tt))
                    continue;
                if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0)
                    continue;

                // Skip points outside the visible range
                if (tt < filterLower || tt > filterUpper)
                    continue;

                QVariantMap pt;
                pt.insert(QStringLiteral("lat"), la);
                pt.insert(QStringLiteral("lon"), lo);
                pt.insert(QStringLiteral("t"), tt); // UTC seconds for JS hover interpolation
                points.push_back(pt);

                if (!haveBounds) {
                    haveBounds = true;
                    minLat = maxLat = la;
                    minLon = maxLon = lo;
                } else {
                    minLat = qMin(minLat, la);
                    maxLat = qMax(maxLat, la);
                    minLon = qMin(minLon, lo);
                    maxLon = qMax(maxLon, lo);
                }
            }

            if (points.size() < 2)
                continue;

            const QString sessionId =
                session.getAttribute(SessionKeys::SessionId).toString();

            Track t;
            t.sessionId = sessionId;
            t.points = std::move(points);
            t.color = colorForSession(sessionId);
            m_tracks.push_back(std::move(t));
        }
    }

    endResetModel();

    m_hasData = !m_tracks.isEmpty();

    if (haveBounds) {
        m_boundsNorth = maxLat;
        m_boundsSouth = minLat;
        m_boundsEast  = maxLon;
        m_boundsWest  = minLon;
        m_centerLat   = (maxLat + minLat) / 2.0;
        m_centerLon   = (maxLon + minLon) / 2.0;
    } else {
        m_boundsNorth = 0.0;
        m_boundsSouth = 0.0;
        m_boundsEast  = 0.0;
        m_boundsWest  = 0.0;
        m_centerLat   = 0.0;
        m_centerLon   = 0.0;
    }

    if (oldHasData != m_hasData) emit hasDataChanged();
    if (oldCount != m_tracks.size()) emit countChanged();
    if (oldCenterLat != m_centerLat || oldCenterLon != m_centerLon)
        emit centerChanged();
    if (oldBoundsNorth != m_boundsNorth || oldBoundsSouth != m_boundsSouth ||
        oldBoundsEast != m_boundsEast || oldBoundsWest != m_boundsWest)
        emit boundsChanged();
}

} // namespace FlySight
