#ifndef TIMECALCULATIONS_H
#define TIMECALCULATIONS_H

namespace FlySight {
namespace Calculations {

/// Register time-related calculated measurements for all sensors.
/// This includes:
/// - _time: Converted UTC time for each sensor
void registerTimeCalculations();

} // namespace Calculations
} // namespace FlySight

#endif // TIMECALCULATIONS_H
