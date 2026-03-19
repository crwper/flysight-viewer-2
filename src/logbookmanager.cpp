#include "logbookmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include "dataimporter.h"
#include "dataexporter.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

using namespace FlySight;

// ============================================================================
// Singleton
// ============================================================================

LogbookManager& LogbookManager::instance()
{
    static LogbookManager manager;
    return manager;
}

LogbookManager::LogbookManager()
    : QObject(nullptr)
{
}

// ============================================================================
// Directory
// ============================================================================

QString LogbookManager::sessionsDirectory() const
{
    const QString logbookFolder = PreferencesManager::instance()
        .getValue(PreferenceKeys::GeneralLogbookFolder).toString();
    const QString dir = logbookFolder
        + QStringLiteral("/FlySight Viewer/logbook/sessions");
    QDir().mkpath(dir);
    return dir;
}

// ============================================================================
// Initialize
// ============================================================================

QList<SessionData> LogbookManager::initialize()
{
    const QString dir = sessionsDirectory();

    // Attempt to read index.json
    const QString indexPath = dir + QStringLiteral("/index.json");
    QFile indexFile(indexPath);
    if (indexFile.exists() && indexFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(indexFile.readAll(), &parseError);
        indexFile.close();

        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            const QJsonObject root = doc.object();
            for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
                const QString sessionId = it.key();
                const QJsonObject entry = it.value().toObject();
                const QString uuid = entry[QStringLiteral("uuid")].toString();
                if (!uuid.isEmpty()) {
                    m_sessionIdToUuid[sessionId] = uuid;
                }
            }
            return {};
        }
    }

    // Fallback: scan *.csv files and return parsed sessions
    QList<SessionData> result = scanSessionFiles();

    // Write the index so it's available on next launch
    flushIndex();

    return result;
}

// ============================================================================
// Save Session
// ============================================================================

bool LogbookManager::saveSession(const SessionData& session)
{
    const QString sessionId = session.getAttribute(SessionKeys::SessionId).toString();
    if (sessionId.isEmpty()) {
        return false;
    }

    // Reuse existing UUID or generate a new one
    QString uuid;
    if (m_sessionIdToUuid.contains(sessionId)) {
        uuid = m_sessionIdToUuid[sessionId];
    } else {
        uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    const QString filePath = sessionsDirectory()
        + QStringLiteral("/") + uuid + QStringLiteral(".csv");

    if (!DataExporter::exportSession(filePath, session)) {
        return false;
    }

    m_sessionIdToUuid[sessionId] = uuid;
    return true;
}

// ============================================================================
// Load All Sessions
// ============================================================================

QList<SessionData> LogbookManager::loadAllSessions()
{
    return scanSessionFiles();
}

// ============================================================================
// Scan Session Files
// ============================================================================

QList<SessionData> LogbookManager::scanSessionFiles()
{
    QList<SessionData> result;
    const QString dir = sessionsDirectory();
    const QDir sessDir(dir);
    const QStringList csvFiles = sessDir.entryList(
        QStringList() << QStringLiteral("*.csv"),
        QDir::Files, QDir::Name);

    for (const QString& filename : csvFiles) {
        const QString filePath = sessDir.absoluteFilePath(filename);
        DataImporter importer;
        SessionData sessionData;
        if (importer.readFile(filePath, sessionData)) {
            const QString sessionId = sessionData.getAttribute(SessionKeys::SessionId).toString();
            const QString uuid = QFileInfo(filename).completeBaseName();
            if (!sessionId.isEmpty()) {
                m_sessionIdToUuid[sessionId] = uuid;
            }
            result.append(sessionData);
        }
    }

    return result;
}

// ============================================================================
// Remove Session
// ============================================================================

bool LogbookManager::removeSession(const QString& sessionId)
{
    if (!m_sessionIdToUuid.contains(sessionId)) {
        return false;
    }

    const QString uuid = m_sessionIdToUuid[sessionId];
    const QString filePath = sessionsDirectory()
        + QStringLiteral("/") + uuid + QStringLiteral(".csv");

    if (!QFile::remove(filePath)) {
        qWarning("LogbookManager: failed to remove %s", qPrintable(filePath));
        return false;
    }

    m_sessionIdToUuid.remove(sessionId);
    return true;
}

// ============================================================================
// Flush Index
// ============================================================================

void LogbookManager::flushIndex()
{
    QJsonObject root;
    for (auto it = m_sessionIdToUuid.constBegin(); it != m_sessionIdToUuid.constEnd(); ++it) {
        QJsonObject entry;
        entry[QStringLiteral("uuid")] = it.value();
        root[it.key()] = entry;
    }

    const QString filePath = sessionsDirectory() + QStringLiteral("/index.json");

    // Atomic write via QSaveFile
    QSaveFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("LogbookManager: failed to open %s for writing", qPrintable(filePath));
        return;
    }
    saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!saveFile.commit()) {
        qWarning("LogbookManager: failed to commit %s", qPrintable(filePath));
    }
}
