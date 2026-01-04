#ifndef TRACKMAPMODEL_H
#define TRACKMAPMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QGeoCoordinate>
#include <QGeoRectangle>
#include <QVariantList>
#include <QVector>

namespace FlySight {

class SessionModel;

/**
 * Exposes visible session GNSS tracks to QML for display on a Qt Location Map.
 *
 * Each row corresponds to one visible session.
 * The "trackPoints" role is a QVariantList of QVariantMaps:
 *   { "lat": <double>, "lon": <double> }
 *
 * QML converts these into coordinates via QtPositioning.coordinate(lat, lon).
 */
class TrackMapModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QGeoCoordinate center READ center NOTIFY centerChanged)
    Q_PROPERTY(QGeoRectangle bounds READ bounds NOTIFY boundsChanged)

public:
    enum Roles {
        SessionIdRole = Qt::UserRole + 1,
        TrackPointsRole,
        TrackColorRole
    };

    explicit TrackMapModel(SessionModel *sessionModel, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool hasData() const { return m_hasData; }
    int count() const { return m_tracks.size(); }
    QGeoCoordinate center() const { return m_center; }
    QGeoRectangle bounds() const { return m_bounds; }

public slots:
    void rebuild();

signals:
    void hasDataChanged();
    void countChanged();
    void centerChanged();
    void boundsChanged();

private:
    struct Track {
        QString sessionId;
        QVariantList points; // [{lat:..., lon:...}, ...]
        QColor color;
    };

    SessionModel *m_sessionModel = nullptr;
    QVector<Track> m_tracks;

    bool m_hasData = false;
    QGeoCoordinate m_center;
    QGeoRectangle m_bounds;

    static QColor colorForSession(const QString &sessionId);
};

} // namespace FlySight

#endif // TRACKMAPMODEL_H
