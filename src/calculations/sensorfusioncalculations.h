#ifndef SENSORFUSIONCALCULATIONS_H
#define SENSORFUSIONCALCULATIONS_H

namespace FlySight {
namespace Calculations {

/// Register IMU/GNSS Extended Kalman Filter fusion calculations.
/// Depends on both GNSS and IMU sensor data.
void registerSensorfusionCalculations();

} // namespace Calculations
} // namespace FlySight

#endif // SENSORFUSIONCALCULATIONS_H
