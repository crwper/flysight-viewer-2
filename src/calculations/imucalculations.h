#ifndef IMUCALCULATIONS_H
#define IMUCALCULATIONS_H

namespace FlySight {
namespace Calculations {

/// Register all IMU-based calculated measurements.
/// Depends only on IMU sensor data.
void registerImuCalculations();

} // namespace Calculations
} // namespace FlySight

#endif // IMUCALCULATIONS_H
