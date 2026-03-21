#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <optional>

#include <QAbstractTableModel>
#include <QMap>
#include <QTimer>
#include <QVector>
#include "logbookcolumn.h"
#include "sessiondata.h"

namespace FlySight {

struct SessionRow {
    QString sessionId;
    QMap<int, QVariant> cachedValues;  // column index -> cached value from index
    std::optional<SessionData> session; // std::nullopt = stub, has_value = loaded
    bool visible = false;  // all sessions default to not visible
    bool dirty = false;    // true when in-memory data has not been persisted

    bool isLoaded() const { return session.has_value(); }
};

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

    void mergeSessions(const QList<SessionData>& sessions);
    bool removeSessions(const QList<QString> &sessionIds);

    void setRowsVisibility(const QMap<int, bool>& rowVisibility);

    const SessionRow& rowAt(int row) const;
    SessionRow& rowAt(int row);
    SessionData &sessionRef(int row);

    // Populate model from cached index data (stubs only, no CSV parsing)
    void populateFromIndex(const QMap<QString, QMap<int, QVariant>> &cachedValues,
                           const QMap<QString, double> &lastAccessed);

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

    // Start background loading of all stub sessions, ordered by lastAccessed descending
    void startBackgroundLoader(const QMap<QString, double> &lastAccessed);

    // Flush dirty sessions to disk (public so MainWindow can call on shutdown)
    void flushDirtySessions();

    // Count of sessions still in stub state (not yet loaded)
    int remainingStubCount() const;

signals:
    void modelChanged();
    void sessionLoaded(const QString &sessionId);
    void hoveredSessionChanged(const QString& sessionId);
    void dependencyChanged(const QString &sessionId, const DependencyKey &key);
    void saveProgressChanged(int remaining, int total);
    void loadProgressChanged(int remaining, int total);

private:
    QVector<SessionRow> m_rows;
    QVector<LogbookColumn> m_columns;
    QString m_hoveredSessionId;

    // Deferred logbook persistence (saver worker)
    QTimer m_saveTimer;
    int m_saveHighWater = 0;   // total dirty sessions in current save wave
    int m_saveRemaining = 0;   // dirty sessions remaining in current wave
    void scheduleSave(const QString &sessionId);

    // Background loader
    QTimer m_loadTimer;
    QVector<QString> m_loadQueue;  // session IDs ordered by lastAccessed descending
    int m_loadHighWater = 0;   // total stubs in current load wave
    int m_loadRemaining = 0;   // stubs remaining in current wave

    // Formatting helpers for data()
    QVariant formatAttributeValue(const SessionData &session, const LogbookColumn &col) const;
    QVariant formatMeasurementValue(const SessionData &session, const LogbookColumn &col) const;
    QVariant formatDeltaValue(const SessionData &session, const LogbookColumn &col) const;
    QVariant formatRawValue(const QVariant &rawValue, const LogbookColumn &col) const;
    QString columnUnitLabel(const LogbookColumn &col) const;

    // Extract raw column values from a loaded session (used by index maintenance)
    QMap<LogbookColumn, QVariant> computeColumnValues(const SessionData &session) const;

private slots:
    void rebuildColumns();
    void loadNextSession();
    void saveNextSession();
};

} // namespace FlySight

#endif // SESSIONMODEL_H
