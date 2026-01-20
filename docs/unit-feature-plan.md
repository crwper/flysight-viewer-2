# Unit System Design Proposal

## Overview

This proposal describes a first-class unit conversion system for FlySight Viewer 2 that:
- Supports toggling between Metric and Imperial (with extensibility for more systems)
- Can be controlled via Preferences dialog or keyboard shortcut ("U")
- Is extensible for new calculated values
- Integrates cleanly with the existing architecture

---

## Current State Analysis

### What Exists
- **Preferences infrastructure**: `PreferencesManager` singleton with `general/units` key already defined (stores "Metric"/"Imperial" but is **unused**)
- **Plot metadata**: `PlotValue` struct with `plotUnits` field (display string only, no conversion factors)
- **Display formatting**: `formatValue()` in `legendpresenter.cpp` handles precision
- **Data storage**: Raw data in `SessionData` uses mixed units (e.g., acceleration in g's, not m/s²)

### Key Insight
The architecture separates data storage from display, but currently uses inconsistent base units. Normalizing to SI at import creates a cleaner contract:
- **Import layer**: Convert sensor units → SI
- **Calculation layer**: All inputs/outputs in SI
- **Display layer**: Convert SI → user's preferred units

---

## Proposed Architecture

### Core Concept: Unit Definitions Registry

Create a centralized **unit definitions system** that maps measurement types to unit specifications:

```
MeasurementType → {
    baseUnit: "m/s",
    systems: {
        metric:   { unit: "m/s", scale: 1.0,     offset: 0, precision: 1 },
        imperial: { unit: "mph", scale: 2.23694, offset: 0, precision: 1 }
    }
}
```

Conversion formula: `displayValue = (rawValue * scale) + offset`

This supports both simple scaling (speed, distance) and affine transforms (temperature: °F = °C × 1.8 + 32).

### Key Design Principles

1. **SI Normalization**: All data in `SessionData` is stored in SI units. Import converts sensor-native units (e.g., g's) to SI (m/s²). Calculated values operate on SI inputs and produce SI outputs.

2. **Measurement Types, Not Individual Plots**: Define unit conversions by *type* (e.g., "speed", "altitude", "temperature"), not per-plot. Multiple plots share the same conversion.

3. **Display-Layer Conversion**: Conversion to display units happens only when rendering:
   - Y-axis labels
   - Legend values
   - Axis tick values

4. **Plot Values Only**: Only `PlotValue` entries need `measurementType`. Intermediate calculated values that aren't displayed don't need unit metadata.

5. **Extensible System Selection**: Support arbitrary unit systems beyond just metric/imperial (e.g., future "aviation" with knots + feet).

---

## Proposed Components

### 1. UnitDefinitions (New File: `src/units/unitdefinitions.h`)

Central registry mapping measurement types to conversion specifications:

| Measurement Type | SI Base | Metric Display | Imperial Display |
|-----------------|---------|----------------|------------------|
| distance | m | m (×1, prec: 1) | ft (×3.28084, prec: 0) |
| altitude | m | m (×1, prec: 1) | ft (×3.28084, prec: 0) |
| speed | m/s | m/s (×1, prec: 1) | mph (×2.23694, prec: 1) |
| vertical_speed | m/s | m/s (×1, prec: 1) | mph (×2.23694, prec: 1) |
| acceleration | m/s² | g (×0.10197, prec: 2) | g (×0.10197, prec: 2) |
| temperature | °C | °C (×1, prec: 1) | °F (×1.8 +32, prec: 1) |
| pressure | Pa | Pa (×1, prec: 0) | inHg (×0.0002953, prec: 2) |
| rotation | deg/s | deg/s (×1, prec: 1) | deg/s (×1, prec: 1) |
| angle | deg | deg (×1, prec: 1) | deg (×1, prec: 1) |
| magnetic_field | T | gauss (×10000, prec: 4) | gauss (×10000, prec: 4) |
| voltage | V | V (×1, prec: 2) | V (×1, prec: 2) |
| percentage | % | % (×1, prec: 1) | % (×1, prec: 1) |
| time | s | s (×1, prec: 3) | s (×1, prec: 3) |
| count | — | (×1, prec: 0) | (×1, prec: 0) |

Note: Acceleration is stored in SI (m/s²) but displayed in g's for both systems—this is the conventional unit for skydiving. Magnetic field stored in Tesla, displayed in gauss.

### 2. UnitConverter (New File: `src/units/unitconverter.h/.cpp`)

Singleton service providing:
- `convert(value, measurementType)` → applies `(value * scale) + offset`
- `getUnitLabel(measurementType)` → current unit string (e.g., "m/s" or "mph")
- `getPrecision(measurementType)` → decimal places for formatting
- `format(value, measurementType)` → convenience method returning formatted string with unit
- `currentSystem()` / `setSystem(systemName)` → get/set active system
- `availableSystems()` → list of registered system names
- Signal: `systemChanged(QString systemName)` for reactive updates

The converter reads/writes the `general/units` preference and syncs with PreferencesManager.

### 3. Extended PlotValue

Add `measurementType` field to associate plots with unit categories:

```cpp
struct PlotValue {
    // ... existing fields ...
    QString measurementType;  // e.g., "speed", "altitude", "temperature"
};
```

Plots without a `measurementType` (or with empty string) remain unconverted.

### 4. Integration Points

| Component | Change |
|-----------|--------|
| **PlotWidget** | Convert Y-axis data and labels using UnitConverter |
| **LegendPresenter** | Convert displayed values and unit labels |
| **PreferencesManager** | Wire `general/units` to UnitConverter |
| **MainWindow** | Add "U" keyboard shortcut action |
| **GeneralSettingsPage** | Already has UI, just needs to trigger UnitConverter |

---

## Data Flow

```
Sensor Data (native units: g, gauss, etc.)
     ↓
DataImporter normalizes to SI (m/s², Tesla, etc.)
     ↓
SessionData stores SI values
     ↓
Calculations operate on SI, produce SI
     ↓
PlotWidget retrieves SI data
     ↓
UnitConverter.convert(value, measurementType) → display units
     ↓
Display (axis labels, tick values, legend)
```

---

## Extensibility

### Adding a New Unit System

1. Add system definition to UnitDefinitions (code change)
2. Update Preferences UI combobox options

Example future system:
```cpp
// "Aviation" system: knots for speed, feet for altitude
{ "aviation", {
    { "speed", { "kn", 1.94384, 1 } },
    { "altitude", { "ft", 3.28084, 0 } },
    { "vertical_speed", { "ft/min", 196.85, 0 } },
}}
```

### Adding a New Calculated Value

1. Register the plot with appropriate `measurementType`:
```cpp
PlotRegistry::instance().registerPlot({
    "GNSS", "Ground Speed", "", Qt::blue,
    "GNSS", "groundSpeed",
    "speed"  // measurementType - automatically gets m/s or mph
});
```

That's it - the unit system handles the rest.

---

## Keyboard Shortcut Implementation

The "U" key toggle would:
1. Be registered as a `QAction` in MainWindow (similar to existing plot shortcuts)
2. Cycle through available unit systems (or toggle between two)
3. Call `UnitConverter::setSystem()`
4. Emit `systemChanged()` signal
5. Connected components (PlotWidget, LegendPresenter) react and refresh

---

## Files to Create/Modify

### New Files
- `src/units/unitdefinitions.h` - Unit type definitions and conversion factors
- `src/units/unitconverter.h/.cpp` - Conversion service singleton

### Modified Files
- `src/dataimporter.cpp` - Normalize sensor data to SI during import (g→m/s², gauss→T, etc.)
- `src/plotregistry.h` - Add `measurementType` to PlotValue
- `src/mainwindow.cpp` - Add measurementType to built-in plot registrations, add "U" shortcut
- `src/plotwidget.cpp` - Apply conversions to axis labels and tick values
- `src/legendpresenter.cpp` - Apply conversions to displayed values
- `src/preferences/generalsettingspage.cpp` - Wire UI to UnitConverter
- `src/pluginhost.cpp` - Support measurementType from plugins

---

## Resolved Decisions

- **SI normalization**: All data normalized to SI at import; calculations operate on SI
- **Vertical speed**: Use mph for imperial (consistent with horizontal speed)
- **Acceleration**: Display in g's for both systems (conventional for skydiving)
- **System naming**: "Metric" and "Imperial"
- **Plugin API**: Plugins will use built-in measurement types only (no custom definitions)
- **Temperature conversion**: Use generic affine transforms (scale + offset) in UnitConverter
- **Precision**: Precision varies per unit system (e.g., "1234 ft" vs "376.1 m")

---

## Verification Plan

After implementation, verify:

1. **Preferences toggle**: Change units in Preferences → General, confirm plots and legends update immediately
2. **Keyboard shortcut**: Press "U", confirm unit system toggles and all displays update
3. **Plot axes**: Y-axis labels show correct units (e.g., "Elevation (ft)" in Imperial)
4. **Legend values**: Cursor position shows converted values with correct precision
5. **Axis tick values**: Tick labels reflect converted scale
6. **Temperature**: Verify °C → °F conversion includes offset (0°C should show 32°F)
7. **Persistence**: Close and reopen app, confirm unit preference persists
8. **Edge cases**: NaN values still display as "--", coordinate precision unchanged

---

## Summary

This design provides:

- **Clean separation**: Data stays in SI, conversion at display layer only
- **Type-based conversion**: Define once per measurement type, reuse across plots
- **Extensibility**: Easy to add new unit systems or measurement types
- **Minimal disruption**: Existing code paths largely unchanged
- **Reactive updates**: Signal-based propagation when unit system changes
