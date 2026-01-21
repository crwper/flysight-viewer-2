#ifndef DATAIMPORTER_H
#define DATAIMPORTER_H

#include "sessiondata.h"
#include <QString>
#include <QTextStream>

namespace FlySight {

class DataImporter {
public:
    DataImporter() = default;

    // Method to import a file and get the session data
    bool importFile(const QString& fileName, SessionData& sessionData);

    // Report last import error
    QString getLastError() const;

private:
    // Enums and type definitions
    enum class FS_FileType { FS1, FS2 };
    enum class FS_Section { HEADER, DATA };

    // Last import error
    QString m_lastError;

    // Private helper methods
    void importSimple(QTextStream& in, SessionData& sessionData, const QString &sensorName);
    void importFS2(QTextStream& in, SessionData& sessionData);
    FS_Section importHeaderRow(const QString& line,
                               QMap<QString, QVector<QString>>& columnOrder,
                               QMap<QString, QVector<QString>>& columnUnits,
                               SessionData& sessionData);
    void importDataRow(const QString& line, const QMap<QString, QVector<QString>>& columnOrder, SessionData& sessionData, QString key);

    // Helper function to extract device ID
    void extractDeviceId(const QString& fileName, SessionData& sessionData, const QString& expectedKey);
    QString findFlySightRoot(const QString& filePath);

    // Default sensor ID for FS1 files
    static constexpr char DefaultSensorId[] = "GNSS";

    static QString getDescription(const QString& fileName);
};

} // namespace FlySight

#endif // DATAIMPORTER_H
