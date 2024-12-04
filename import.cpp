// fsdataimporter.cpp

#include "import.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QMessageBox>
#include <QStringTokenizer>
#include <QTextStream>

namespace FSImport {

bool FSDataImporter::importFile(const QString& fileName, SessionData& sessionData) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(nullptr, "Import failed", "Couldn't read file");
        return false;
    }

    // Read the entire file into a QByteArray
    QByteArray fileData = file.readAll();

    if (fileData.isEmpty()) {
        QMessageBox::critical(nullptr, "Import failed", "Empty file");
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
        // Unknown format
        QMessageBox::critical(nullptr, "Import failed", "Unknown file format");
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

    // After importing, check if SESSION_ID is set
    if (!sessionData.vars.contains("SESSION_ID")) {
        // Compute MD5 hash of fileData
        QByteArray md5Hash = QCryptographicHash::hash(fileData, QCryptographicHash::Md5);
        QString md5HashString = md5Hash.toHex();
        sessionData.vars["SESSION_ID"] = md5HashString;
    }

    return true;
}

void FSDataImporter::importFS1(QTextStream& in, SessionData& sessionData) {
    QString sensorName = "GNSS";
    QMap<QString, QVector<QString>> columnOrder;

    // Read the first line (column names)
    QString columnLine = in.readLine();
    QVector<QString> columns = columnLine.split(',', Qt::SkipEmptyParts).toVector().toList().toVector();
    columnOrder[sensorName] = columns;

    // Initialize the data map in sensors
    SessionData::SensorData& sensor = sessionData.sensors[sensorName];
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
        importDataRow(dataLine, sensorName, columnOrder, false, sessionData);
    }
}

void FSDataImporter::importFS2(QTextStream& in, SessionData& sessionData) {
    FS_Section section = FS_Section::HEADER;

    // Temporary map to store column order per sensor
    QMap<QString, QVector<QString>> columnOrder;

    // Read and process header lines
    while (!in.atEnd() && (section == FS_Section::HEADER)) {
        QString line = in.readLine();
        importHeaderRow(line, section, columnOrder, sessionData);
    }

    // Process data lines
    while (!in.atEnd()) {
        QString line = in.readLine();
        importDataRow(line, QString(), columnOrder, true, sessionData);
    }
}

void FSDataImporter::importHeaderRow(const QString& row, FS_Section& section, QMap<QString, QVector<QString>>& columnOrder, SessionData& sessionData) {
    if (row == "$DATA") {
        section = FS_Section::DATA;
    } else {
        QStringView rowView(row);
        QStringTokenizer tokenizer(rowView, u',');
        auto it = tokenizer.begin();
        if (it == tokenizer.end()) return;

        QStringView token0 = *it++;
        if (token0 == u"$VAR") {
            if (it == tokenizer.end()) return;
            QStringView varName = *it++;
            if (it == tokenizer.end()) return;
            QStringView varValue = *it;
            sessionData.vars[varName.toString()] = varValue.toString();
        } else if (token0 == u"$COL") {
            if (it == tokenizer.end()) return;
            QStringView sensorName = *it++;
            QVector<QString> columns;
            for (; it != tokenizer.end(); ++it) {
                columns.append(it->toString());
            }
            // Store the columns in the temporary columnOrder map
            columnOrder[sensorName.toString()] = columns;

            // Initialize the data map in sensors
            SessionData::SensorData& sensor = sessionData.sensors[sensorName.toString()];
            for (const QString& colName : columns) {
                sensor[colName]; // Initialize empty QVector<double> for each column
            }
        } else if (token0 == u"$UNIT") {
            // Ignore $UNIT lines
        }
    }
}

void FSDataImporter::importDataRow(const QString& line, const QString& sensorName, const QMap<QString, QVector<QString>>& columnOrder, bool hasSensorKeyInLine, SessionData& sessionData) {
    QStringView lineView(line);
    QStringTokenizer tokenizer(lineView, u',');
    auto it = tokenizer.begin();

    QString key = sensorName;

    // For FS2 format, the first token is the sensor key
    if (hasSensorKeyInLine) {
        if (it == tokenizer.end()) return;
        QStringView token0 = *it++;
        if (!token0.startsWith(u'$')) return; // Invalid format
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

} // namespace FSImport
