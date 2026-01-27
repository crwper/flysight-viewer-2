#ifndef SIMPLIFICATIONCALCULATIONS_H
#define SIMPLIFICATIONCALCULATIONS_H

namespace FlySight {
namespace Calculations {

/// Register track simplification calculations.
/// Uses Ramer-Douglas-Peucker algorithm to simplify GNSS track data.
/// Depends on GNSS sensor data.
void registerSimplificationCalculations();

} // namespace Calculations
} // namespace FlySight

#endif // SIMPLIFICATIONCALCULATIONS_H
