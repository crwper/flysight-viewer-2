#ifndef DATAEXPORTER_H
#define DATAEXPORTER_H

#include "sessiondata.h"
#include <QString>

namespace FlySight {

class DataExporter {
public:
    DataExporter() = default;

    // Write a SessionData to disk in FS2 format
    static bool exportSession(const QString &filePath, const SessionData &sessionData);
};

} // namespace FlySight

#endif // DATAEXPORTER_H
