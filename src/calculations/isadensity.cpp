#include "isadensity.h"
#include <cmath>
#include <algorithm>

namespace FlySight {
namespace Calculations {

// ISA constants
static constexpr double T0 = 288.15;     // K, sea level temperature
static constexpr double P0 = 101325.0;   // Pa, sea level pressure
static constexpr double g  = 9.80665;    // m/s^2
static constexpr double R  = 287.058;    // J/(kg*K), specific gas constant for dry air

// Layer definitions: base altitude (m), base temperature (K), base pressure (Pa), lapse rate (K/m)
struct ISALayer {
    double hBase;
    double tBase;
    double pBase;
    double lapse;
};

// Compute pressure at transition altitude from one layer to the next
static double pressureAtAltitude(const ISALayer& layer, double h)
{
    double dh = h - layer.hBase;
    if (layer.lapse != 0.0) {
        double T = layer.tBase + layer.lapse * dh;
        return layer.pBase * std::pow(T / layer.tBase, -g / (R * layer.lapse));
    } else {
        return layer.pBase * std::exp(-g * dh / (R * layer.tBase));
    }
}

double isaDensity(double altitudeMSL)
{
    // Clamp altitude to [0, 47000] m
    altitudeMSL = std::clamp(altitudeMSL, 0.0, 47000.0);

    // Precompute layer base pressures
    static const double P_11000 = pressureAtAltitude({0, 288.15, P0, -0.0065}, 11000.0);
    static const double P_20000 = pressureAtAltitude({11000, 216.65, P_11000, 0.0}, 20000.0);
    static const double P_32000 = pressureAtAltitude({20000, 216.65, P_20000, 0.001}, 32000.0);

    static const ISALayer layers[] = {
        {    0, 288.15, P0,      -0.0065},
        {11000, 216.65, P_11000,  0.0},
        {20000, 216.65, P_20000,  0.001},
        {32000, 228.65, P_32000,  0.0028}
    };

    // Find the applicable layer
    const ISALayer* layer = &layers[0];
    for (int i = 3; i >= 0; --i) {
        if (altitudeMSL >= layers[i].hBase) {
            layer = &layers[i];
            break;
        }
    }

    double dh = altitudeMSL - layer->hBase;
    double T, P;

    if (layer->lapse != 0.0) {
        T = layer->tBase + layer->lapse * dh;
        P = layer->pBase * std::pow(T / layer->tBase, -g / (R * layer->lapse));
    } else {
        T = layer->tBase;
        P = layer->pBase * std::exp(-g * dh / (R * layer->tBase));
    }

    return P / (R * T);
}

} // namespace Calculations
} // namespace FlySight
