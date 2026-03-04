#ifndef TIMECALCULATIONS_H
#define TIMECALCULATIONS_H

#include <optional>

namespace FlySight {

class SessionData;  // forward declaration

namespace Calculations {

/// Register time-related calculated measurements for all sensors.
/// This includes:
/// - _time: Converted UTC time for each sensor
/// - _system_time: System time for each sensor (passthrough for non-GNSS, inverse fit for GNSS)
void registerTimeCalculations();

/// Convert a single system-time value to UTC using cached linear-fit coefficients.
/// Returns std::nullopt if the fit coefficients are not available.
std::optional<double> systemTimeToUtc(const SessionData &session, double systemTime);

/// Convert a UTC value back to system time using cached linear-fit coefficients.
/// Returns std::nullopt if the fit coefficients are not available or if the fit is degenerate (a == 0).
std::optional<double> utcToSystemTime(const SessionData &session, double utc);

} // namespace Calculations
} // namespace FlySight

#endif // TIMECALCULATIONS_H
