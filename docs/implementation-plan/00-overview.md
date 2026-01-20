# Implementation Plan: Unit Conversion System

## Feature Specification

This proposal describes a first-class unit conversion system for FlySight Viewer 2 that:
- Supports toggling between Metric and Imperial (with extensibility for more systems)
- Can be controlled via Preferences dialog or keyboard shortcut ("U")
- Is extensible for new calculated values
- Integrates cleanly with the existing architecture

---

### Current State Analysis

#### What Exists
- **Preferences infrastructure**: `PreferencesManager` singleton with `general/units` key already defined (stores "Metric"/"Imperial" but is **unused**)
- **Plot metadata**: `PlotValue` struct with `plotUnits` field (display string only, no conversion factors)
- **Display formatting**: `formatValue()` in `legendpresenter.cpp` handles precision
- **Data storage**: Raw data in `SessionData` uses mixed units (e.g., acceleration in g's, not m/s²)

#### Key Insight
The architecture separates data storage from display, but currently uses inconsistent base units. Normalizing to SI at import creates a cleaner contract:
- **Import layer**: Convert sensor units → SI
- **Calculation layer**: All inputs/outputs in SI
- **Display layer**: Convert SI → user's preferred units

---

### Proposed Architecture

#### Core Concept: Unit Definitions Registry

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

#### Key Design Principles

1. **SI Normalization**: All data in `SessionData` is stored in SI units. Import converts sensor-native units (e.g., g's) to SI (m/s²). Calculated values operate on SI inputs and produce SI outputs.

2. **Measurement Types, Not Individual Plots**: Define unit conversions by *type* (e.g., "speed", "altitude", "temperature"), not per-plot. Multiple plots share the same conversion.

3. **Display-Layer Conversion**: Conversion to display units happens only when rendering:
   - Y-axis labels
   - Legend values
   - Axis tick values

4. **Plot Values Only**: Only `PlotValue` entries need `measurementType`. Intermediate calculated values that aren't displayed don't need unit metadata.

5. **Extensible System Selection**: Support arbitrary unit systems beyond just metric/imperial (e.g., future "aviation" with knots + feet).

---

### Proposed Components

#### 1. UnitDefinitions (New File: `src/units/unitdefinitions.h`)

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

#### 2. UnitConverter (New File: `src/units/unitconverter.h/.cpp`)

Singleton service providing:
- `convert(value, measurementType)` → applies `(value * scale) + offset`
- `getUnitLabel(measurementType)` → current unit string (e.g., "m/s" or "mph")
- `getPrecision(measurementType)` → decimal places for formatting
- `format(value, measurementType)` → convenience method returning formatted string with unit
- `currentSystem()` / `setSystem(systemName)` → get/set active system
- `availableSystems()` → list of registered system names
- Signal: `systemChanged(QString systemName)` for reactive updates

The converter reads/writes the `general/units` preference and syncs with PreferencesManager.

#### 3. Extended PlotValue

Add `measurementType` field to associate plots with unit categories:

```cpp
struct PlotValue {
    // ... existing fields ...
    QString measurementType;  // e.g., "speed", "altitude", "temperature"
};
```

Plots without a `measurementType` (or with empty string) remain unconverted.

#### 4. Integration Points

| Component | Change |
|-----------|--------|
| **PlotWidget** | Convert Y-axis data and labels using UnitConverter |
| **LegendPresenter** | Convert displayed values and unit labels |
| **PreferencesManager** | Wire `general/units` to UnitConverter |
| **MainWindow** | Add "U" keyboard shortcut action, add measurementType to built-in plot registrations |
| **GeneralSettingsPage** | Already has UI, just needs to trigger UnitConverter |

---

### Data Flow

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

### Extensibility

#### Adding a New Unit System

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

#### Adding a New Calculated Value

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

### Keyboard Shortcut Implementation

The "U" key toggle would:
1. Be registered as a `QAction` in MainWindow (similar to existing plot shortcuts)
2. Cycle through available unit systems (or toggle between two)
3. Call `UnitConverter::setSystem()`
4. Emit `systemChanged()` signal
5. Connected components (PlotWidget, LegendPresenter) react and refresh

---

### Files to Create/Modify

#### New Files
- `src/units/unitdefinitions.h` - Unit type definitions and conversion factors
- `src/units/unitconverter.h/.cpp` - Conversion service singleton

#### Modified Files
- `src/dataimporter.cpp` - Normalize sensor data to SI during import (g→m/s², gauss→T, etc.)
- `src/plotregistry.h` - Add `measurementType` to PlotValue
- `src/mainwindow.cpp` - Add measurementType to built-in plot registrations, add "U" shortcut
- `src/plotwidget.cpp` - Apply conversions to axis labels and tick values
- `src/legendpresenter.cpp` - Apply conversions to displayed values
- `src/preferences/generalsettingspage.cpp` - Wire UI to UnitConverter
- `src/pluginhost.cpp` - Support measurementType from plugins

---

### Resolved Decisions

- **SI normalization**: All data normalized to SI at import; calculations operate on SI
- **Vertical speed**: Use mph for imperial (consistent with horizontal speed)
- **Acceleration**: Display in g's for both systems (conventional for skydiving)
- **System naming**: "Metric" and "Imperial"
- **Plugin API**: Plugins will use built-in measurement types only (no custom definitions)
- **Temperature conversion**: Use generic affine transforms (scale + offset) in UnitConverter
- **Precision**: Precision varies per unit system (e.g., "1234 ft" vs "376.1 m")

---

### Verification Plan

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

## Phases

| Phase | Name | Purpose | Dependencies |
|-------|------|---------|--------------|
| 1 | Core Unit System | Create UnitDefinitions and UnitConverter classes with all conversion logic | None |
| 2 | Data Import Normalization | Normalize sensor data to SI units during import | None |
| 3 | PlotRegistry Extension | Add measurementType field to PlotValue and update registrations | Phase 1 |
| 4 | Display Integration | Integrate UnitConverter with PlotWidget and LegendPresenter | Phase 1, 3 |
| 5 | UI and Preferences | Wire GeneralSettingsPage to UnitConverter, add "U" keyboard shortcut | Phase 1, 4 |
| 6 | Plugin Support | Enable plugins to specify measurementType for registered plots | Phase 3 |

## Dependency Graph

```
Phase 1 (Core Unit System)     Phase 2 (Data Import)
         │                              │
         ▼                              │
Phase 3 (PlotRegistry Extension)        │
         │                              │
         ▼                              │
Phase 4 (Display Integration) ◄─────────┘
         │
         ▼
Phase 5 (UI and Preferences)
         │
         │                     Phase 6 (Plugin Support)
         │                              ▲
         └──────────────────────────────┘
                                        │
                              (Phase 3 also feeds Phase 6)
```

**Parallel execution possible:**
- Phases 1 and 2 can run in parallel (no dependencies)
- Phase 6 can start once Phase 3 completes (independent of 4, 5)

## Key Patterns & References

All files that exemplify patterns relevant to this feature, grouped by category.

### Singleton Implementation Pattern
- `src/preferences/preferencesmanager.h` — Meyer's singleton with Q_OBJECT, signals for change notification
- `src/plotregistry.cpp` — Simple Meyer's singleton without QObject
- `src/markerregistry.cpp` — Simple Meyer's singleton without QObject
- `src/pluginhost.cpp` — Singleton with initialization method

### Preferences System
- `src/preferences/preferencesmanager.h` — Core preferences manager with registerPreference/getValue/setValue
- `src/preferences/preferencesmanager.cpp` — Implementation with preferenceChanged signal
- `src/preferences/preferencekeys.h` — Centralized preference key definitions using inline const QString
- `src/preferences/preferencesdialog.h` — Dialog container with QListWidget + QStackedWidget
- `src/preferences/preferencesdialog.cpp` — Page registration and navigation
- `src/preferences/generalsettingspage.h` — Settings page header example
- `src/preferences/generalsettingspage.cpp` — Settings page with save/load pattern (units combobox already exists)

### Plot Registration System
- `src/plotregistry.h` — PlotValue struct definition, registry interface
- `src/plotregistry.cpp` — Registry implementation with registerPlot/allPlots
- `src/plotmodel.h` — Tree model for plot hierarchy
- `src/plotmodel.cpp` — Model implementation with enable/disable support

### Data Import and Storage
- `src/dataimporter.h` — DataImporter interface
- `src/dataimporter.cpp` — CSV parsing, measurement storage (current non-SI units)
- `src/sessiondata.h` — SessionData container with getAttribute/getMeasurement
- `src/sessiondata.cpp` — Measurement storage implementation
- `src/sessionmodel.h` — Session model for UI binding
- `src/sessionmodel.cpp` — Model implementation with signals

### Display Layer
- `src/legendpresenter.h` — Legend presenter interface
- `src/legendpresenter.cpp` — Value formatting with formatValue(), precision logic (lines 73-88)
- `src/legendwidget.h` — Legend widget UI
- `src/legendwidget.cpp` — Widget implementation, preference connections
- `src/legendtablemodel.h` — Table model for legend display
- `src/plotwidget.h` — Plot display widget interface
- `src/plotwidget.cpp` — Plot rendering, Y-axis labels from PlotValue.plotUnits

### Calculated Values System
- `src/calculatedvalue.h` — Template for cached calculated values
- `src/dependencymanager.h` — Dependency tracking
- `src/dependencykey.h` — Dependency key types

### Plugin System
- `src/pluginhost.h` — Plugin host interface
- `src/pluginhost.cpp` — Python plugin loading, plot/marker registration from plugins
- `src/bridgeimpl.h` — pybind11 bridge for Python plugins

### MainWindow Coordination
- `src/mainwindow.h` — Main window interface
- `src/mainwindow.cpp` — Built-in plot registration (lines 682-755), preference initialization (lines 795-872), calculated value registration (lines 874-998), keyboard shortcuts

### Signal/Slot Patterns for Reactive Updates
- `src/crosshairmanager.cpp` — Connects to preferenceChanged for style updates
- `src/legendwidget.cpp` — Connects to preferenceChanged for text size
- `src/plotwidget.cpp` — Connects to preferenceChanged for plot appearance

## Decisions & Constraints

### Architectural Decisions
1. **New `src/units/` directory**: Create dedicated directory for unit system files to maintain clean organization
2. **UnitConverter as QObject singleton**: Enables signal/slot for reactive updates across the application
3. **measurementType as QString**: String-based type identification for flexibility and plugin compatibility
4. **Conversion at display layer only**: Raw SI data flows through the system unchanged until final display

### Constraints
1. **Backward compatibility**: Existing data files use mixed units; import must handle both old and new formats
2. **Plugin API stability**: Plugins using PlotRegistry must continue to work; measurementType should be optional with empty string default
3. **Performance**: Conversion happens frequently (every cursor move updates legend); must be fast O(1) lookups
4. **Coordinate precision**: Latitude/longitude must remain at 6 decimal places regardless of unit system

### Open Items
- None currently identified; all major decisions resolved in the feature specification

---

## Planning Complete

### Structure
- Phases: 6
- Total tasks: 31 across all phases
  - Phase 1: 6 tasks (Core Unit System)
  - Phase 2: 4 tasks (Data Import Normalization)
  - Phase 3: 4 tasks (PlotRegistry Extension)
  - Phase 4: 8 tasks (Display Integration)
  - Phase 5: 5 tasks (UI and Preferences)
  - Phase 6: 4 tasks (Plugin Support)
- Estimated complexity: 38 points (S=1, M=2, L=3)

### Parallel Execution Opportunities
- **Wave 1**: Phases 1 and 2 can start immediately (no dependencies)
- **Wave 2**: Phase 3 starts after Phase 1 completes; Phase 6 can start after Phase 3
- **Wave 3**: Phase 4 starts after Phases 1, 2, and 3 complete
- **Wave 4**: Phase 5 starts after Phases 1 and 4 complete

### Coverage
All requirements from the feature specification are addressed:
- Unit definitions registry with 14 measurement types
- UnitConverter singleton with conversion, formatting, and signals
- SI normalization of sensor data during import
- PlotValue extended with measurementType field
- Display layer integration (PlotWidget, LegendPresenter)
- Preferences UI wired to UnitConverter
- "U" keyboard shortcut for quick toggle
- Plugin support for measurementType
- Persistence of unit preference

### Phase Dependencies Verified
- Phase references are consistent across all documents
- No circular dependencies
- Acceptance criteria are specific and testable
- No gaps between phase boundaries

### Ready for Implementation
The plan is ready for handoff to the implementation orchestrator.
