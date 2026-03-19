#ifndef LOGBOOKMANAGER_H
#define LOGBOOKMANAGER_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include "sessiondata.h"

namespace FlySight {

class LogbookManager : public QObject {
    Q_OBJECT

public:
    static LogbookManager& instance();

    // Creates sessions directory if missing, loads index.json or scans *.csv files
    void initialize();

    // Writes a session to disk as a UUID-based .csv file; returns true on success
    bool saveSession(const SessionData& session);

    // Reads all *.csv files from the sessions directory, returns parsed sessions
    QList<SessionData> loadAllSessions();

    // Deletes the .csv file for the given SESSION_ID; returns true on success
    bool removeSession(const QString& sessionId);

    // Writes index.json mapping SESSION_IDs to UUIDs
    void flushIndex();

private:
    LogbookManager();
    Q_DISABLE_COPY(LogbookManager)

    // Returns the full path to the sessions directory
    QString sessionsDirectory() const;

    // Maps SESSION_ID strings to UUID filename stems (without extension)
    QMap<QString, QString> m_sessionIdToUuid;
};

} // namespace FlySight

#endif // LOGBOOKMANAGER_H
