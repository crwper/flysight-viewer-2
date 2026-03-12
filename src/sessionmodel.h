#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "logbookcolumn.h"
#include "sessiondata.h"

namespace FlySight {

class SessionModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum CustomRoles {
        IsHoveredRole = Qt::UserRole + 100
    };

    SessionModel(QObject *parent = nullptr);

    // Data management
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void mergeSessionData(const SessionData& newSession);
    void mergeSessions(const QList<SessionData>& sessions);
    bool removeSessions(const QList<QString> &sessionIds);

    void setRowsVisibility(const QMap<int, bool>& rowVisibility);

    const QVector<SessionData>& getAllSessions() const;
    SessionData &sessionRef(int row);

    // Hovered session management
    QString hoveredSessionId() const;
    void setHoveredSessionId(const QString& sessionId);

    int getSessionRow(const QString& sessionId) const;

    bool updateAttribute(const QString &sessionId,
                         const QString &attributeKey,
                         const QVariant &newValue);

    bool removeAttribute(const QString &sessionId,
                         const QString &attributeKey);

    // Enable sorting
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

signals:
    void modelChanged();
    void hoveredSessionChanged(const QString& sessionId);
    void dependencyChanged(const QString &sessionId, const DependencyKey &key);

private:
    QVector<SessionData> m_sessionData;
    QVector<LogbookColumn> m_columns;
    QString m_hoveredSessionId;

    // Formatting helpers for data()
    QVariant formatAttributeValue(const SessionData &session, const LogbookColumn &col) const;
    QVariant formatMeasurementValue(const SessionData &session, const LogbookColumn &col) const;
    QVariant formatDeltaValue(const SessionData &session, const LogbookColumn &col) const;

private slots:
    void rebuildColumns();
};

} // namespace FlySight

#endif // SESSIONMODEL_H
