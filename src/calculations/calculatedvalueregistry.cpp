#include "calculatedvalueregistry.h"
#include "attributecalculations.h"
#include "gnsscalculations.h"
#include "imucalculations.h"
#include "magcalculations.h"
#include "barocalculations.h"
#include "humcalculations.h"
#include "vbatcalculations.h"
#include "timecalculations.h"
#include "simplificationcalculations.h"

using namespace FlySight;

CalculatedValueRegistry& CalculatedValueRegistry::instance() {
    static CalculatedValueRegistry R;
    return R;
}

void CalculatedValueRegistry::registerBuiltInCalculations() {
    // Register in dependency order: attributes first, then single-sensor, then multi-sensor
    Calculations::registerAttributeCalculations();
    Calculations::registerGnssCalculations();
    Calculations::registerImuCalculations();
    Calculations::registerMagCalculations();
    Calculations::registerBaroCalculations();
    Calculations::registerHumCalculations();
    Calculations::registerVbatCalculations();
    Calculations::registerTimeCalculations();
    Calculations::registerSimplificationCalculations();
}
