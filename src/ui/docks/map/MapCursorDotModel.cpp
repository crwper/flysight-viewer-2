#include "MapCursorDotModel.h"

#include "cursormodel.h"
#include "sessiondata.h"
#include "sessionmodel.h"
#include "plotutils.h"

#include <QtMath>
#include <QDateTime>

#include <algorithm>

namespace FlySight {

static constexpr const char *kDefaultSensor = "Simplified";
static constexpr const char *kLatKey = "lat";
static constexpr const char *kLonKey = "lon";

static bool isMonotonic(const QVector<double> &t, int n, bool ascending)
{
    if (n < 2)
        return false;

    for (int i = 1; i < n; ++i) {
        if (ascending) {
            if (t[i] < t[i - 1])
                return false;
        } else {
            if (t[i] > t[i - 1])
                return false;
        }
    }

    return true;
}

static bool sampleLatLonAtUtc(const SessionData &session, double utcSeconds, double *outLat, double *outLon)
{
    if (!outLat || !outLon)
        return false;

    const QVector<double> t =
        session.getMeasurement(QString::fromLatin1(kDefaultSensor), QString::fromLatin1(SessionKeys::Time));
    const QVector<double> lat =
        session.getMeasurement(QString::fromLatin1(kDefaultSensor), QString::fromLatin1(kLatKey));
    const QVector<double> lon =
        session.getMeasurement(QString::fromLatin1(kDefaultSensor), QString::fromLatin1(kLonKey));

    const int n = qMin(t.size(), qMin(lat.size(), lon.size()));
    if (n < 2)
        return false;

    const bool ascending = (t[n - 1] >= t[0]);
    if (!isMonotonic(t, n, ascending))
        return false;

    if (ascending) {
        if (utcSeconds < t[0] || utcSeconds > t[n - 1])
            return false;
    } else {
        if (utcSeconds > t[0] || utcSeconds < t[n - 1])
            return false;
    }

    int i0 = 0;
    int i1 = 1;

    if (ascending) {
        auto it = std::lower_bound(t.begin(), t.begin() + n, utcSeconds);
        if (it == t.begin() + n)
            return false;

        const int idx = static_cast<int>(it - t.begin());

        // Exact match
        if (*it == utcSeconds) {
            const double la = lat[idx];
            const double lo = lon[idx];
            if (!qIsFinite(la) || !qIsFinite(lo))
                return false;
            if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0)
                return false;

            *outLat = la;
            *outLon = lo;
            return true;
        }

        if (idx == 0) {
            i0 = 0;
            i1 = 1;
        } else {
            i0 = idx - 1;
            i1 = idx;
        }
    } else {
        auto it = std::lower_bound(
            t.begin(), t.begin() + n, utcSeconds,
            [](double a, double b) { return a > b; } // t[] is descending
        );
        if (it == t.begin() + n)
            return false;

        const int idx = static_cast<int>(it - t.begin());

        // Exact match
        if (*it == utcSeconds) {
            const double la = lat[idx];
            const double lo = lon[idx];
            if (!qIsFinite(la) || !qIsFinite(lo))
                return false;
            if (la < -90.0 || la > 90.0 || lo < -180.0 || lo > 180.0)
                return false;

            *outLat = la;
            *outLon = lo;
            return true;
        }

        if (idx == 0) {
            i0 = 0;
            i1 = 1;
        } else {
            i0 = idx - 1;
            i1 = idx;
        }
    }

    const double t0 = t[i0];
    const double t1 = t[i1];
    const double la0 = lat[i0];
    const double la1 = lat[i1];
    const double lo0 = lon[i0];
    const double lo1 = lon[i1];

    if (!qIsFinite(t0) || !qIsFinite(t1) ||
        !qIsFinite(la0) || !qIsFinite(la1) ||
        !qIsFinite(lo0) || !qIsFinite(lo1))
        return false;

    if (la0 < -90.0 || la0 > 90.0 || lo0 < -180.0 || lo0 > 180.0)
        return false;
    if (la1 < -90.0 || la1 > 90.0 || lo1 < -180.0 || lo1 > 180.0)
        return false;

    const double dt = (t1 - t0);
    if (dt == 0.0) {
        // Degenerate time span: fall back to nearest neighbor
        const double d0 = qAbs(utcSeconds - t0);
        const double d1 = qAbs(utcSeconds - t1);

        if (d0 <= d1) {
            *outLat = la0;
            *outLon = lo0;
        } else {
            *outLat = la1;
            *outLon = lo1;
        }
        return true;
    }

    const double alpha = (utcSeconds - t0) / dt;

    *outLat = la0 + alpha * (la1 - la0);
    *outLon = lo0 + alpha * (lo1 - lo0);
    return true;
}

static bool cursorUtcSecondsForSession(const CursorModel::Cursor &c,
                                      const SessionData &session,
                                      double *outUtcSeconds)
{
    if (!outUtcSeconds)
        return false;

    if (c.positionSpace == CursorModel::PositionSpace::UtcSeconds) {
        *outUtcSeconds = c.positionValue;
        return true;
    }

    if (c.positionSpace != CursorModel::PositionSpace::PlotAxisCoord)
        return false;

    const auto optOffset = markerOffsetUtcSeconds(session, c.referenceMarkerKey);
    if (!optOffset.has_value())
        return false;

    *outUtcSeconds = c.positionValue + *optOffset;
    return true;
}

MapCursorDotModel::MapCursorDotModel(SessionModel *sessionModel, CursorModel *cursorModel, QObject *parent)
    : QAbstractListModel(parent)
    , m_sessionModel(sessionModel)
    , m_cursorModel(cursorModel)
{
    // Zero-interval single-shot timer coalesces rapid cursorsChanged bursts
    // (e.g. 3 emissions per hover event) into a single rebuild.
    m_rebuildTimer.setInterval(0);
    m_rebuildTimer.setSingleShot(true);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &MapCursorDotModel::rebuild);

    if (m_sessionModel) {
        connect(m_sessionModel, &SessionModel::modelChanged,
                this, &MapCursorDotModel::scheduleRebuild);
    }

    if (m_cursorModel) {
        connect(m_cursorModel, &CursorModel::cursorsChanged,
                this, &MapCursorDotModel::scheduleRebuild);
    }

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &MapCursorDotModel::onPreferenceChanged);

    rebuild();
}

void MapCursorDotModel::scheduleRebuild()
{
    m_rebuildTimer.start();
}

int MapCursorDotModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_dots.size();
}

QVariant MapCursorDotModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_dots.size())
        return QVariant();

    const Dot &d = m_dots.at(index.row());

    switch (role) {
    case SessionIdRole:
        return d.sessionId;
    case LatRole:
        return d.lat;
    case LonRole:
        return d.lon;
    case ColorRole:
        return d.color;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MapCursorDotModel::roleNames() const
{
    return {
        { SessionIdRole, "sessionId" },
        { LatRole,       "lat" },
        { LonRole,       "lon" },
        { ColorRole,     "color" }
    };
}

QColor MapCursorDotModel::colorForSession(const QString &sessionId)
{
    // Keep in sync with TrackMapModel::colorForSession
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);

    double opacity = PreferencesManager::instance().getValue(
        PreferenceKeys::MapTrackOpacity).toDouble();
    c.setAlphaF(opacity);

    return c;
}

void MapCursorDotModel::onPreferenceChanged(const QString &key, const QVariant &value)
{
    Q_UNUSED(value)

    if (key == PreferenceKeys::MapTrackOpacity) {
        rebuild(); // Rebuild dots with new opacity (affects color)
    }
}

void MapCursorDotModel::rebuild()
{
    QVector<Dot> newDots;

    if (m_sessionModel && m_cursorModel) {
        const CursorModel::Cursor c = chooseEffectiveCursor(m_cursorModel);

        if (!c.id.isEmpty() && c.active) {
            const bool explicitTargets =
                (c.targetPolicy == CursorModel::TargetPolicy::Explicit && !c.targetSessions.isEmpty());

            const auto &sessions = m_sessionModel->getAllSessions();

            for (const auto &session : sessions) {
                if (!session.isVisible())
                    continue;

                const QString sessionId =
                    session.getAttribute(SessionKeys::SessionId).toString();

                if (sessionId.isEmpty())
                    continue;

                if (explicitTargets && !c.targetSessions.contains(sessionId))
                    continue;

                double utcSeconds = 0.0;
                if (!cursorUtcSecondsForSession(c, session, &utcSeconds))
                    continue;

                double lat = 0.0;
                double lon = 0.0;
                if (!sampleLatLonAtUtc(session, utcSeconds, &lat, &lon))
                    continue;

                Dot d;
                d.sessionId = sessionId;
                d.lat = lat;
                d.lon = lon;
                d.color = colorForSession(sessionId);
                newDots.push_back(std::move(d));
            }
        }
    }

    // Skip the model reset if nothing changed
    if (newDots.size() == m_dots.size()) {
        bool same = true;
        for (int i = 0; i < newDots.size(); ++i) {
            const Dot &a = m_dots[i];
            const Dot &b = newDots[i];
            if (a.sessionId != b.sessionId || a.lat != b.lat
                || a.lon != b.lon || a.color != b.color) {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }

    beginResetModel();
    m_dots = std::move(newDots);
    endResetModel();
}

} // namespace FlySight
