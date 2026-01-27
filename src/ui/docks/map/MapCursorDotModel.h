#ifndef MAPCURSORDOTMODEL_H
#define MAPCURSORDOTMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QHash>
#include <QString>
#include <QVariant>
#include <QVector>

#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

class SessionModel;
class CursorModel;

/**
 * Exposes cursor-driven dot markers to QML for display on a Qt Location Map.
 *
 * Dots are derived from the effective cursor (same precedence rules as the legend)
 * and sampled from the "Simplified" GNSS track (lat/lon vs SessionKeys::Time) for
 * each targeted session.
 */
class MapCursorDotModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        SessionIdRole = Qt::UserRole + 1,
        LatRole,
        LonRole,
        ColorRole
    };

    explicit MapCursorDotModel(SessionModel *sessionModel, CursorModel *cursorModel, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void rebuild();

private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    struct Dot {
        QString sessionId;
        double lat = 0.0;
        double lon = 0.0;
        QColor color;
    };

    SessionModel *m_sessionModel = nullptr;
    CursorModel *m_cursorModel = nullptr;
    QVector<Dot> m_dots;

    static QColor colorForSession(const QString &sessionId);
};

} // namespace FlySight

#endif // MAPCURSORDOTMODEL_H
