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

    // Data rows: for each sensor, write all samples
    for (const QString &sensorKey : sensors) {
        const QStringList columns = sessionData.measurementKeys(sensorKey);
        if (columns.isEmpty()) {
            continue;
        }

        // Determine sample count from the first column
        const QVector<double> firstCol = sessionData.getMeasurement(sensorKey, columns.first());
        const int sampleCount = firstCol.size();

        // Gather all column vectors to avoid repeated lookups
        QVector<QVector<double>> columnData;
        columnData.reserve(columns.size());
        for (const QString &col : columns) {
            columnData.append(sessionData.getMeasurement(sensorKey, col));
        }

        for (int i = 0; i < sampleCount; ++i) {
            stream << "$" << sensorKey;
            for (int c = 0; c < columnData.size(); ++c) {
                stream << "," << columnData[c][i];
            }
            stream << "\n";
        }
    }

    // Flush and commit atomically
    stream.flush();
    if (saveFile.error() != QFileDevice::NoError) {
        saveFile.cancelWriting();
        return false;
    }

    return saveFile.commit();
}

} // namespace FlySight
