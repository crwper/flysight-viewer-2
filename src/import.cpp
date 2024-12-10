// import.cpp

#include "import.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStringTokenizer>
#include <QTextStream>

namespace FlySight {

bool DataImporter::importFile(const QString& fileName, SessionData& sessionData) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = "Couldn't read file";
        return false;
    }

    // Read the entire file into a QByteArray
    QByteArray fileData = file.readAll();

    if (fileData.isEmpty()) {
        m_lastError = "Empty file";
        return false;
    }

    // Extract the first line directly from QByteArray
    int firstNewline = fileData.indexOf('\n');
    QString firstLine;
    if (firstNewline != -1) {
        firstLine = QString::fromUtf8(fileData.left(firstNewline)).trimmed();
    } else {
        firstLine = QString::fromUtf8(fileData).trimmed();
    }

    // Determine file type based on the first line
    FS_FileType fileType;
    if (firstLine.startsWith("time")) {
        fileType = FS_FileType::FS1;
    } else if (firstLine.startsWith("$FLYS")) {
        fileType = FS_FileType::FS2;
    } else {
        m_lastError = "Unknown file format";
        return false;
    }

    // Create a QTextStream from the QByteArray
    QTextStream in(&fileData, QIODevice::ReadOnly);

    // Use the fileType to choose the import method
    switch (fileType) {
    case FS_FileType::FS1:
        importFS1(in, sessionData);
        break;
    case FS_FileType::FS2:
        importFS2(in, sessionData);
        break;
    }

    // Set default description
    sessionData.setVar("DESCRIPTION", getDescription(fileName));

    // After importing, attempt to extract DEVICE_ID based on file type
    if (!sessionData.hasVar("DEVICE_ID")) {
        switch (fileType) {
        case FS_FileType::FS1:
            extractDeviceId(fileName, sessionData, "Processor serial number");
            break;
        case FS_FileType::FS2:
            extractDeviceId(fileName, sessionData, "Device_ID");
            break;
        }

        // After extraction, check again if DEVICE_ID is present
        if (!sessionData.hasVar("DEVICE_ID")) {
            // Assign default DEVICE_ID
            sessionData.setVar("DEVICE_ID", SessionData::DEFAULT_DEVICE_ID);
            qDebug() << "Assigned default DEVICE_ID to session.";
        }
    }

    // After importing, check if SESSION_ID is set
    if (!sessionData.hasVar("SESSION_ID")) {
        // Compute MD5 hash of fileData
        QByteArray md5Hash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        QString md5HashString = md5Hash.toHex();
        sessionData.setVar("SESSION_ID", md5HashString);
    }

    // Initialize visibility
    sessionData.setVisible(true);

    return true;
}

void DataImporter::importFS1(QTextStream& in, SessionData& sessionData) {
    QMap<QString, QVector<QString>> columnOrder;

    // Read the first line (column names)
    QString columnLine = in.readLine();
    QVector<QString> columns = columnLine.split(',', Qt::SkipEmptyParts).toVector().toList().toVector();
    columnOrder[DefaultSensorId] = columns;

    // Initialize the data map in sensors
    SessionData::SensorData& sensor = sessionData.sensors[DefaultSensorId];
    for (const QString& colName : columns) {
        sensor[colName]; // Initialize empty QVector<double> for each column
    }

    // Read the second line (units), ignore
    if (in.atEnd()) {
        return;
    }
    in.readLine();

    // Process data lines
    while (!in.atEnd()) {
        QString dataLine = in.readLine();
        importDataRow(dataLine, columnOrder, sessionData);
    }
}

void DataImporter::importFS2(QTextStream& in, SessionData& sessionData) {
    FS_Section section = FS_Section::HEADER;

    // Temporary map to store column order per sensor
    QMap<QString, QVector<QString>> columnOrder;

    // Read and process header lines
    while (!in.atEnd() && (section == FS_Section::HEADER)) {
        QString line = in.readLine();
        section = importHeaderRow(line, columnOrder, sessionData);
    }

    // Process data lines
    while (!in.atEnd()) {
        QString line = in.readLine();
        importDataRow(line, columnOrder, sessionData);
    }
}

DataImporter::FS_Section DataImporter::importHeaderRow(
    const QString& line,
    QMap<QString, QVector<QString>>& columnOrder,
    SessionData& sessionData)
{
    // By default, stay in HEADER section
    FS_Section section = FS_Section::HEADER;

    if (line == "$DATA") {
        section = FS_Section::DATA;
    } else {
        QStringView lineView(line);
        QStringTokenizer tokenizer(lineView, u',');
        auto it = tokenizer.begin();

        if (it != tokenizer.end()) {
            QStringView token0 = *it++;
            if (token0 == u"$VAR") {
                if (it != tokenizer.end()) {
                    QStringView varName = *it++;
                    if (it != tokenizer.end()) {
                        QStringView varValue = *it;
                        sessionData.setVar(varName.toString(), varValue.toString());
                    }
                }
            } else if (token0 == u"$COL") {
                if (it != tokenizer.end()) {
                    QStringView sensorName = *it++;
                    QVector<QString> columns;

                    for (; it != tokenizer.end(); ++it) {
                        columns.append(it->toString());
                    }

                    columnOrder[sensorName.toString()] = columns;

                    // Initialize sensor measurements as empty
                    for (const QString &colName : columns) {
                        sessionData.setSensorMeasurement(sensorName.toString(), colName, {});
                    }
                }
            } else if (token0 == u"$UNIT") {
                // Ignore $UNIT lines
            }
        }
    }

    return section;
}

void DataImporter::importDataRow(const QString& line, const QMap<QString, QVector<QString>>& columnOrder, SessionData& sessionData) {
    QStringView lineView(line);
    QStringTokenizer tokenizer(lineView, u',');
    auto it = tokenizer.begin();

    QString key = DefaultSensorId;

    // For FS2 format, the first token is the sensor key
    if (line.startsWith("$")) {
        if (it == tokenizer.end()) return;
        QStringView token0 = *it++;
        key = token0.mid(1).toString(); // Remove the '$' at the start
    }

    // Get the column order for this sensor
    const QVector<QString>& cols = columnOrder.value(key);
    if (cols.isEmpty()) {
        // Unknown sensor or no columns defined
        return;
    }

    SessionData::SensorData& sensor = sessionData.sensors[key];

    QVector<QStringView> dataFields;
    for (; it != tokenizer.end(); ++it) {
        dataFields.append(*it);
    }

    if (dataFields.size() != cols.size()) {
        // Handle error: number of data fields does not match number of columns
        return;
    }

    for (int i = 0; i < cols.size(); ++i) {
        const QString& colName = cols[i];
        const QStringView& dataValueView = dataFields[i];

        if (dataValueView.isEmpty()) {
            // Empty field
            sensor[colName].append(0.0);
        } else if (dataValueView.endsWith(u'Z')) {
            QDateTime dt = QDateTime::fromString(dataValueView.toString(), Qt::ISODate);
            if (dt.isValid()) {
                sensor[colName].append(dt.toMSecsSinceEpoch() / 1000.0);
            } else {
                // Handle parsing error
                sensor[colName].append(0.0);
            }
        } else {
            bool ok;
            double val = dataValueView.toDouble(&ok);
            if (ok) {
                sensor[colName].append(val);
            } else {
                // Handle parsing error
                sensor[colName].append(0.0);
            }
        }
    }
}

void DataImporter::extractDeviceId(const QString& fileName, SessionData& sessionData, const QString& expectedKey) {
    // Find the root directory of the FlySight device
    QString flySightRoot = findFlySightRoot(fileName);

    if (flySightRoot.isEmpty()) {
        // FLYSIGHT.TXT not found
        qWarning() << "FLYSIGHT.TXT not found in any parent directories of:" << fileName;
        return;
    }

    QString flysightTxtPath = QDir(flySightRoot).absoluteFilePath("FLYSIGHT.TXT");

    // Open FLYSIGHT.TXT and search for the expected key
    QFile flysightFile(flysightTxtPath);
    if (!flysightFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // Fail silently if the file cannot be opened
        qWarning() << "Failed to open FLYSIGHT.TXT at:" << flysightTxtPath;
        return;
    }

    QTextStream in(&flysightFile);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Handle comments: ignore everything after ';'
        int commentIndex = line.indexOf(';');
        if (commentIndex != -1) {
            line = line.left(commentIndex).trimmed();
        }

        if (line.isEmpty()) {
            // Skip empty lines
            continue;
        }

        // Split the line into key and value at the first ':'
        int colonIndex = line.indexOf(':');
        if (colonIndex == -1) {
            // No colon found; invalid line format
            qWarning() << "Invalid line format (no colon found):" << line;
            continue;
        }

        QString key = line.left(colonIndex).trimmed();
        QString value = line.mid(colonIndex + 1).trimmed();

        if (key == expectedKey) {
            sessionData.setVar("DEVICE_ID", value);
            return;
        }
    }

    // Optionally, log that the expected key was not found
    qWarning() << expectedKey << "not found in FLYSIGHT.TXT";
}

QString DataImporter::findFlySightRoot(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QDir currentDir = fileInfo.absoluteDir();

    while (true) {
        QString flysightPath = currentDir.absoluteFilePath("FLYSIGHT.TXT");
        if (QFile::exists(flysightPath)) {
            // Found FLYSIGHT.TXT in the current directory
            return currentDir.absolutePath();
        }

        if (!currentDir.cdUp()) {
            // Reached the root of the filesystem without finding FLYSIGHT.TXT
            break;
        }
    }

    // FLYSIGHT.TXT not found
    return QString();
}

QString DataImporter::getLastError() const {
    return m_lastError;
}

QString DataImporter::getDescription(const QString& fileName)
{
    QString description;

    QFileInfo fileInfo(fileName);
    QDir parentDir = fileInfo.dir();

    QString baseName = fileInfo.baseName();
    QString parentName = parentDir.dirName();

    // Declare the regular expression as static to avoid recompiling it on each function call
    static const QRegularExpression timePattern("^\\d{2}-\\d{2}-\\d{2}$", QRegularExpression::CaseInsensitiveOption);

    if (timePattern.match(baseName).hasMatch()) {
        description = baseName;
    }

    while (timePattern.match(parentName).hasMatch()) {
        if (!description.isEmpty()) {
            description = QString("/") + description;
        }

        // Add folder name to description
        description = parentName + description;

        parentDir.cdUp();
        parentName = parentDir.dirName();
    }

    if (description.isEmpty()) {
        description = fileInfo.fileName();
    }

    return description;
}

} // namespace FlySight
