#ifndef CURSORMODEL_H
#define CURSORMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace FlySight {

class CursorModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum class CursorType {
        MouseHover = 0,
        VideoPlayback = 1,
        Pinned = 2
    };
    Q_ENUM(CursorType)

    enum class PositionSpace {
        PlotAxisCoord = 0,
        UtcSeconds = 1
    };
    Q_ENUM(PositionSpace)

    enum class TargetPolicy {
        Explicit = 0,
        AutoVisibleOverlap = 1
    };
    Q_ENUM(TargetPolicy)

    struct Cursor {
        QString id;
        QString label;

        CursorType type = CursorType::MouseHover;
        bool active = false;

        PositionSpace positionSpace = PositionSpace::PlotAxisCoord;
        double positionValue = 0.0;
        QString axisKey; // only meaningful when positionSpace == PlotAxisCoord

        TargetPolicy targetPolicy = TargetPolicy::Explicit;
        QSet<QString> targetSessions; // meaningful when targetPolicy == Explicit
    };

    enum Roles {
        IdRole = Qt::UserRole + 1,
        LabelRole,
        TypeRole,
        ActiveRole,
        PositionSpaceRole,
        PositionValueRole,
        AxisKeyRole,
        TargetPolicyRole,
        TargetSessionsRole
    };

    explicit CursorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Public API (minimum per spec)
    int ensureCursor(const Cursor &initial);
    void updateCursor(const Cursor &updated);

    void setCursorActive(const QString &id, bool active);
    void setCursorPositionPlotAxis(const QString &id, const QString &axisKey, double x);
    void setCursorPositionUtc(const QString &id, double utcSeconds);

    void setCursorTargetsExplicit(const QString &id, const QSet<QString> &sessionIds);
    void setCursorTargetPolicy(const QString &id, TargetPolicy policy);

    // Convenience
    bool hasCursor(const QString &id) const;
    Cursor cursorById(const QString &id) const;

signals:
    void cursorsChanged();

private:
    int rowForId(const QString &id) const;

    QVector<Cursor> m_cursors;
    QHash<QString, int> m_rowById;
};

} // namespace FlySight

#endif // CURSORMODEL_H
