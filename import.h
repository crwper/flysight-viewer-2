#ifndef IMPORT_H
#define IMPORT_H

#include "sessiondata.h"
#include <QString>
#include <QTextStream>

namespace FSImport {

class FSDataImporter {
public:
    FSDataImporter() = default;

    // Method to import a file and get the session data
    bool importFile(const QString& fileName, SessionData& sessionData);

    // Report last import error
    QString getLastError() const;

private:
    // Enums and type definitions
    enum class FS_FileType { FS1, FS2 };
    enum class FS_Section { HEADER, DATA };
    using SensorData = SessionData::SensorData;

    // Last import error
    QString m_lastError;

    // Private helper methods
    void importFS1(QTextStream& in, SessionData& sessionData);
    void importFS2(QTextStream& in, SessionData& sessionData);
    void importHeaderRow(const QString& row, FS_Section& section, QMap<QString, QVector<QString>>& columnOrder, SessionData& sessionData);
    void importDataRow(const QString& line, const QString& sensorName, const QMap<QString, QVector<QString>>& columnOrder, bool hasMessageKeyInLine, SessionData& sessionData);

    // Helper function to extract device ID
    void extractDeviceId(const QString& fileName, SessionData& sessionData, const QString& expectedKey);
    QString findFlySightRoot(const QString& filePath);
};

} // namespace FSImport

#endif // IMPORT_H
