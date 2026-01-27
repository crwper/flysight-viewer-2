#ifndef ATTRIBUTECALCULATIONS_H
#define ATTRIBUTECALCULATIONS_H

namespace FlySight {
namespace Calculations {

/// Register all session-wide attribute calculations.
/// These attributes are computed from measurement data and preferences,
/// but do not themselves generate new measurement arrays.
void registerAttributeCalculations();

} // namespace Calculations
} // namespace FlySight

#endif // ATTRIBUTECALCULATIONS_H
