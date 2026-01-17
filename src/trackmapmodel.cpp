#include "trackmapmodel.h"

#include "sessionmodel.h"
#include "sessiondata.h"
#include "plotrangemodel.h"

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

QColor TrackMapModel::colorForSession(const QString &sessionId)
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);
    c.setAlphaF(0.85);
    return c;
}

bool TrackMapModel::computeSessionUtcRange(const SessionData &session,
                                           double *outLower, double *outUpper) const
{
    if (!m_rangeModel || !m_rangeModel->hasRange())
        return false;

    const QString axisKey = m_rangeModel->axisKey();

    if (axisKey == SessionKeys::Time) {
        // UTC mode: range values are already in UTC seconds
        *outLower = m_rangeModel->rangeLower();
        *outUpper = m_rangeModel->rangeUpper();
        return true;
    }

    if (axisKey == SessionKeys::TimeFromExit) {
        // Time-from-exit mode: need to convert using session's exit time
        QVariant v = session.getAttribute(SessionKeys::ExitTime);
        if (!v.canConvert<QDateTime>())
            return false;

        QDateTime dt = v.toDateTime();
        if (!dt.isValid())
            return false;

        double exitUtcSeconds = dt.toMSecsSinceEpoch() / 1000.0;

        *outLower = m_rangeModel->rangeLower() + exitUtcSeconds;
        *outUpper = m_rangeModel->rangeUpper() + exitUtcSeconds;
        return true;
    }

    return false;
}

void TrackMapModel::rebuild()
{
    const bool oldHasData = m_hasData;
    const int oldCount = m_tracks.size();
    const QGeoCoordinate oldCenter = m_center;
    const QGeoRectangle oldBounds = m_bounds;

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
                pt.insert(QStringLiteral("t"), tt); // UTC seconds for QML hover interpolation
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
        const QGeoCoordinate topLeft(maxLat, minLon);
        const QGeoCoordinate bottomRight(minLat, maxLon);
        m_bounds = QGeoRectangle(topLeft, bottomRight);
        m_center = m_bounds.center();
    } else {
        m_bounds = QGeoRectangle();
        m_center = QGeoCoordinate();
    }

    if (oldHasData != m_hasData) emit hasDataChanged();
    if (oldCount != m_tracks.size()) emit countChanged();
    if (oldCenter != m_center) emit centerChanged();
    if (oldBounds != m_bounds) emit boundsChanged();
}

} // namespace FlySight
