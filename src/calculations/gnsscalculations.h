#ifndef GNSSCALCULATIONS_H
#define GNSSCALCULATIONS_H

namespace FlySight {
namespace Calculations {

/// Register all GNSS-based calculated measurements.
/// Depends only on GNSS sensor data.
void registerGnssCalculations();

} // namespace Calculations
} // namespace FlySight

#endif // GNSSCALCULATIONS_H
