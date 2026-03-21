// dataexporter.cpp

#include "dataexporter.h"
#include <QSaveFile>
#include <QTextStream>
#include <QStringConverter>

namespace FlySight {

bool DataExporter::exportSession(const QString &filePath, const SessionData &sessionData)
{
    // Atomic write via QSaveFile
    QSaveFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream stream(&saveFile);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(false);
    stream.setRealNumberPrecision(15);
    stream.setRealNumberNotation(QTextStream::SmartNotation);

    // Version line
    stream << "$FLYS,1\n";

    // Attribute lines
    const QStringList attrKeys = sessionData.attributeKeys();
    for (const QString &key : attrKeys) {
        stream << "$VAR," << key << "," << sessionData.getAttribute(key).toString() << "\n";
    }

    // Column and unit headers
    const QStringList sensors = sessionData.sensorKeys();
    for (const QString &sensorKey : sensors) {
        const QStringList columns = sessionData.measurementKeys(sensorKey);

        // $COL line
        stream << "$COL," << sensorKey;
        for (const QString &col : columns) {
            stream << "," << col;
        }
        stream << "\n";

        // $UNIT line
        stream << "$UNIT," << sensorKey;
        for (const QString &col : columns) {
            stream << "," << sessionData.getUnit(sensorKey, col);
        }
        stream << "\n";
    }

    // Data marker
    stream << "$DATA\n";
    stream.flush();

    // Data rows: build into a QByteArray buffer, then write in bulk.
    // This avoids per-value QTextStream overhead for double formatting.
    QByteArray buf;
    buf.reserve(1024 * 1024); // pre-allocate 1 MB

    for (const QString &sensorKey : sensors) {
        const QStringList columns = sessionData.measurementKeys(sensorKey);
        if (columns.isEmpty()) {
            continue;
        }

        const QByteArray sensorPrefix = QByteArray("$") + sensorKey.toUtf8();

        // Determine sample count from the first column
        const QVector<double> firstCol = sessionData.getMeasurement(sensorKey, columns.first());
        const int sampleCount = firstCol.size();

        // Gather all column vectors to avoid repeated lookups
        QVector<QVector<double>> columnData;
        columnData.reserve(columns.size());
        for (const QString &col : columns) {
            columnData.append(sessionData.getMeasurement(sensorKey, col));
        }

        const int numCols = columnData.size();
        for (int i = 0; i < sampleCount; ++i) {
            buf.append(sensorPrefix);
            for (int c = 0; c < numCols; ++c) {
                buf.append(',');
                buf.append(QByteArray::number(columnData[c][i], 'g', 15));
            }
            buf.append('\n');

            // Flush buffer periodically to avoid excessive memory use
            if (buf.size() > 4 * 1024 * 1024) {
                saveFile.write(buf);
                buf.clear();
            }
        }
    }

    if (!buf.isEmpty()) {
        saveFile.write(buf);
    }

    // Flush and commit atomically
    if (saveFile.error() != QFileDevice::NoError) {
        saveFile.cancelWriting();
        return false;
    }

    return saveFile.commit();
}

} // namespace FlySight
