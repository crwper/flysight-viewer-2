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

// Returns the UTC-seconds value of the given marker attribute for a session,
// or std::nullopt if the attribute is missing / not a valid QDateTime.
// When referenceMarkerKey is empty, returns 0.0 (absolute mode).
// Note: QDateTime::toMSecsSinceEpoch() always returns ms since Unix epoch
// regardless of the QDateTime's timespec, so an explicit toUTC() is unnecessary.
std::optional<double> markerOffsetUtcSeconds(const SessionData &session,
                                             const QString &referenceMarkerKey);

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
