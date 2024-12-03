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

private:
    // Enums and type definitions
    enum class FS_Section { HEADER, DATA };
    using SensorData = SessionData::SensorData;

    // Private helper methods
    void importFS1(QTextStream& in, const QString& firstLine, SessionData& sessionData);
    void importFS2(QTextStream& in, const QString& firstLine, SessionData& sessionData);
    void importHeaderRow(const QString& row, FS_Section& section, QMap<QString, QVector<QString>>& columnOrder, SessionData& sessionData);
    void importDataRow(const QString& line, const QString& sensorName, const QMap<QString, QVector<QString>>& columnOrder, bool hasMessageKeyInLine, SessionData& sessionData);
};

} // namespace FSImport

#endif // IMPORT_H
