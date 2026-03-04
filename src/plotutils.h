#ifndef PLOTUTILS_H
#define PLOTUTILS_H

#include "cursormodel.h"

#include <QString>
#include <QVector>

#include <cmath>
#include <limits>
#include <optional>

namespace FlySight {

class SessionData;
struct PlotValue;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Returns the offset (in seconds) of the given marker attribute for a session,
// in the coordinate space of xVariable.
// When referenceMarkerKey is empty, returns 0.0 (absolute mode).
// When xVariable is SessionKeys::Time, the offset is in UTC seconds.
// When xVariable is SessionKeys::SystemTime, the UTC offset is converted
// to system time via Calculations::utcToSystemTime.
// Returns std::nullopt if the attribute is missing / not a valid QDateTime,
// or if the conversion to system time is unavailable.
std::optional<double> markerOffsetSeconds(const SessionData &session,
                                          const QString &referenceMarkerKey,
                                          const QString &xVariable);

QString seriesDisplayName(const PlotValue &pv);

double interpolateAtX(const QVector<double> &xData,
                      const QVector<double> &yData,
                      double x);

double interpolateSessionMeasurement(const SessionData &session,
                                     const QString &sensorId,
                                     const QString &xAxisKey,
                                     const QString &measurementId,
                                     double x);

QString formatValue(double value,
                    const QString &measurementId,
                    const QString &measurementType);

CursorModel::Cursor chooseEffectiveCursor(const CursorModel *cursorModel);

} // namespace FlySight

#endif // PLOTUTILS_H
