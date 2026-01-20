# Phase 3: PlotRegistry Extension

## Overview

This phase extends the PlotRegistry system to support unit conversion by adding a `measurementType` field to the `PlotValue` struct. This field associates each plot with a measurement category (e.g., "speed", "altitude", "temperature") that determines how the UnitConverter will transform values for display. Additionally, all ~50 built-in plot registrations in MainWindow will be updated with appropriate measurement types.

## Dependencies

- **Depends on:** Phase 1 (Core Unit System) - requires UnitDefinitions with measurement type constants and UnitConverter for validation
- **Blocks:** Phase 4 (Display Integration), Phase 6 (Plugin Support)
- **Assumptions:**
  - UnitConverter singleton exists at `src/units/unitconverter.h/.cpp`
  - UnitDefinitions header exists at `src/units/unitdefinitions.h` with all 14 measurement type string constants
  - The measurement type strings match exactly those defined in UnitDefinitions (e.g., "speed", "altitude", "temperature")

## Tasks

### Task 3.1: Extend PlotValue Struct

**Purpose:** Add the `measurementType` field to PlotValue so plots can be associated with unit conversion categories.

**Files to modify:**
- `src/plotregistry.h` - Add measurementType field to PlotValue struct

**Technical Approach:**

Modify the `PlotValue` struct (lines 11-18 in `src/plotregistry.h`) to add a new QString field:

```cpp
struct PlotValue {
    QString category;          // Category name
    QString plotName;          // Display name of the plot
    QString plotUnits;         // Units for the y-axis (display string)
    QColor defaultColor;       // Default color for the plot
    QString sensorID;          // Sensor name (e.g., "GNSS")
    QString measurementID;     // Measurement name (e.g., "hMSL")
    QString measurementType;   // Unit conversion category (e.g., "speed", "altitude")
};
```

Key considerations:
1. Place `measurementType` as the last field to minimize impact on existing aggregate initialization
2. The field defaults to empty QString when not explicitly set (backward compatible)
3. Empty or missing measurementType means no unit conversion will be applied
4. Do NOT include `unitdefinitions.h` in this header - keep PlotRegistry independent of the unit system

**Acceptance Criteria:**
- [ ] `measurementType` field added to PlotValue struct as QString
- [ ] Field is positioned as the last member of the struct
- [ ] Existing code that creates PlotValue without measurementType still compiles (empty default)
- [ ] `src/plotregistry.h` does not include any unit system headers

**Complexity:** S

---

### Task 3.2: Add MeasurementType Role to PlotModel

**Purpose:** Expose the measurementType through the Qt model/view data role system so other components can query it.

**Files to modify:**
- `src/plotmodel.h` - Add MeasurementTypeRole enum value
- `src/plotmodel.cpp` - Return measurementType in data() method

**Technical Approach:**

1. In `src/plotmodel.h`, add a new role to the `PlotRoles` enum (after line 28):
   ```cpp
   enum PlotRoles {
       DefaultColorRole = Qt::UserRole + 1,
       SensorIDRole,
       MeasurementIDRole,
       PlotUnitsRole,
       PlotValueIdRole,
       CategoryRole,
       MeasurementTypeRole  // NEW: Returns measurementType for unit conversion
   };
   ```

2. In `src/plotmodel.cpp`, extend the `data()` method's switch statement (around line 253-272) to handle the new role:
   ```cpp
   case MeasurementTypeRole:
       return plot->value.measurementType;
   ```

3. In `src/plotmodel.cpp`, update `roleNames()` method (around line 334-345) to include the new role:
   ```cpp
   roles[MeasurementTypeRole] = "measurementType";
   ```

Follow the existing pattern established for PlotUnitsRole (lines 264-265 in data(), line 343 in roleNames()).

**Acceptance Criteria:**
- [ ] `MeasurementTypeRole` added to PlotRoles enum
- [ ] `data()` method returns measurementType for MeasurementTypeRole
- [ ] `roleNames()` includes mapping for MeasurementTypeRole
- [ ] Model API is consistent with existing role patterns

**Complexity:** S

---

### Task 3.3: Update Built-in Plot Registrations in MainWindow

**Purpose:** Add measurementType to all built-in plot registrations so they can participate in unit conversion.

**Files to modify:**
- `src/mainwindow.cpp` - Update registerBuiltInPlots() function (lines 682-755)

**Technical Approach:**

Update the `defaults` vector in `registerBuiltInPlots()` to include measurementType as the 7th element of each PlotValue initializer. The current format is:
```cpp
{"Category", "Name", "units", color, "sensorID", "measurementID"}
```

Change to:
```cpp
{"Category", "Name", "units", color, "sensorID", "measurementID", "measurementType"}
```

Map each plot to its appropriate measurement type based on the overview specification:

**GNSS Category (lines 689-698):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Elevation | m | `altitude` |
| Horizontal speed | m/s | `speed` |
| Vertical speed | m/s | `vertical_speed` |
| Total speed | m/s | `speed` |
| Vertical acceleration | m/s^2 | `acceleration` |
| Horizontal accuracy | m | `distance` |
| Vertical accuracy | m | `distance` |
| Speed accuracy | m/s | `speed` |
| Number of satellites | (empty) | `count` |

**IMU Category (lines 700-711):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Acceleration X/Y/Z | g | `acceleration` |
| Total acceleration | g | `acceleration` |
| Rotation X/Y/Z | deg/s | `rotation` |
| Total rotation | deg/s | `rotation` |
| Temperature | degC | `temperature` |

**Magnetometer Category (lines 713-719):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Magnetic field X/Y/Z | gauss | `magnetic_field` |
| Total magnetic field | gauss | `magnetic_field` |
| Temperature | degC | `temperature` |

**Barometer Category (lines 721-723):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Air pressure | Pa | `pressure` |
| Temperature | degC | `temperature` |

**Humidity Category (lines 725-727):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Humidity | % | `percentage` |
| Temperature | degC | `temperature` |

**Battery Category (line 729-730):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Battery voltage | V | `voltage` |

**GNSS time Category (lines 732-734):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| Time of week | s | `time` |
| Week number | (empty) | `count` |

**Sensor fusion Category (lines 736-750):**
| Plot | Current Units | Measurement Type |
|------|---------------|------------------|
| North/East/Down position | m | `distance` |
| North/East/Down velocity | m/s | `speed` |
| Horizontal acceleration | g | `acceleration` |
| Vertical acceleration | g | `acceleration` |
| X/Y/Z rotation (roll/pitch/yaw) | deg | `angle` |

**Acceptance Criteria:**
- [ ] All ~45 built-in plot registrations include measurementType as 7th field
- [ ] Each plot has the correct measurementType matching UnitDefinitions categories
- [ ] Plots with no unit conversion use empty string `""`
- [ ] Code compiles and application starts without errors
- [ ] Plot registrations maintain their existing color, sensor, and measurement assignments

**Complexity:** M

---

### Task 3.4: Update Plugin Plot Registration

**Purpose:** Extend the plugin system to support measurementType in plot registrations while maintaining backward compatibility.

**Files to modify:**
- `src/pluginhost.cpp` - Update simple plot registration (lines 316-331)

**Technical Approach:**

Modify section 6 ("Register simple plot definitions") in `pluginhost.cpp` to handle an optional `measurementType` attribute from Python plugins:

Current code (lines 318-331):
```cpp
for (py::handle h : sdk.attr("_simple_plots")) {
    py::object plt = h.cast<py::object>();
    PlotRegistry::instance().registerPlot({
        QString::fromStdString(plt.attr("category").cast<std::string>()),
        QString::fromStdString(plt.attr("name").cast<std::string>()),
        plt.attr("units").is_none() ? QString{} :
            QString::fromStdString(plt.attr("units").cast<std::string>()),
        QColor(QString::fromStdString(
            plt.attr("color").cast<std::string>())),
        QString::fromStdString(plt.attr("sensor").cast<std::string>()),
        QString::fromStdString(
            plt.attr("measurement").cast<std::string>())
    });
}
```

Update to include measurementType with backward compatibility:
```cpp
for (py::handle h : sdk.attr("_simple_plots")) {
    py::object plt = h.cast<py::object>();
    PlotRegistry::instance().registerPlot({
        QString::fromStdString(plt.attr("category").cast<std::string>()),
        QString::fromStdString(plt.attr("name").cast<std::string>()),
        plt.attr("units").is_none() ? QString{} :
            QString::fromStdString(plt.attr("units").cast<std::string>()),
        QColor(QString::fromStdString(
            plt.attr("color").cast<std::string>())),
        QString::fromStdString(plt.attr("sensor").cast<std::string>()),
        QString::fromStdString(
            plt.attr("measurement").cast<std::string>()),
        // measurementType: optional, defaults to empty string (no conversion)
        py::hasattr(plt, "measurement_type") && !plt.attr("measurement_type").is_none()
            ? QString::fromStdString(plt.attr("measurement_type").cast<std::string>())
            : QString{}
    });
}
```

Key considerations:
1. Use `py::hasattr()` to check if the attribute exists on the plugin object
2. Also check for None value (Python plugins might set it to None explicitly)
3. Default to empty QString if not provided (backward compatible - no unit conversion)
4. The Python attribute name uses snake_case (`measurement_type`) per Python conventions

**Acceptance Criteria:**
- [ ] Plugin plots with `measurement_type` attribute get it passed to PlotValue
- [ ] Plugin plots without `measurement_type` attribute default to empty string
- [ ] Plugin plots with `measurement_type = None` default to empty string
- [ ] Existing plugins continue to work without modification
- [ ] No runtime errors when loading plugins without measurementType

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No formal unit test framework is in use. Manual verification is the primary testing approach.

### Integration Tests

1. **Build verification**: Project compiles without errors after all modifications
2. **Plugin compatibility**: Existing plugins continue to load and function
3. **Model role access**: Verify MeasurementTypeRole returns correct values through Qt model interface

### Manual Verification

1. **Struct verification**:
   - Create a temporary PlotValue with all 7 fields and verify it compiles
   - Create a PlotValue with only 6 fields (no measurementType) and verify it compiles with empty default

2. **Registration verification** (add temporary debug output):
   ```cpp
   // In registerBuiltInPlots(), after the loop:
   for (const auto& pv : PlotRegistry::instance().allPlots()) {
       qDebug() << "Plot:" << pv.plotName
                << "measurementType:" << pv.measurementType;
   }
   ```
   Verify all plots show expected measurementType values.

3. **Model verification**:
   ```cpp
   // After setupPlotSelectionDock():
   QModelIndex idx = plotModel->index(0, 0);  // First category
   idx = plotModel->index(0, 0, idx);          // First plot in category
   qDebug() << "measurementType via model:"
            << idx.data(PlotModel::MeasurementTypeRole);
   ```

4. **Plugin verification**:
   - Start application with plugins enabled
   - Verify plugins load without errors in console
   - Verify plugin-registered plots appear in plot selection

## Notes for Implementer

### Gotchas

1. **Aggregate initialization order**: The 7th field (measurementType) must be added last to maintain compatibility with existing 6-element initializer lists. C++ allows omitting trailing fields in aggregate initialization.

2. **String matching**: The measurementType strings must exactly match those defined in UnitDefinitions. Common types:
   - `"distance"`, `"altitude"` (both use meters but may have different precision)
   - `"speed"`, `"vertical_speed"` (both use m/s)
   - `"acceleration"` (m/s^2, displayed as g)
   - `"temperature"` (degC)
   - `"pressure"` (Pa)
   - `"rotation"` (deg/s)
   - `"angle"` (deg)
   - `"magnetic_field"` (Tesla, displayed as gauss)
   - `"voltage"` (V)
   - `"percentage"` (%)
   - `"time"` (s)
   - `"count"` (unitless)

3. **Empty vs missing measurementType**: Both empty string `""` and missing field mean "no conversion". Code consuming measurementType should treat empty string as "pass through raw value".

4. **Python attribute naming**: Use snake_case `measurement_type` in Python (per PEP 8) but the C++ field is `measurementType` (camelCase per Qt/FlySight convention).

5. **Magnetic field units**: The current display shows "gauss" but SI stores Tesla. The measurementType "magnetic_field" will handle this conversion in Phase 4.

6. **Acceleration units**: Current display shows "g" and data is stored in g's. Per the overview, Phase 2 normalizes to SI (m/s^2), so measurementType "acceleration" is appropriate.

### Decisions Made

1. **Field position**: Adding measurementType as the last field enables backward compatibility with existing 6-element aggregate initializers. This is the least disruptive approach.

2. **No header dependency**: PlotRegistry should not include unitdefinitions.h. The measurementType is just a QString key; validation against UnitDefinitions happens in consuming code (Phase 4).

3. **Plugin attribute name**: Using `measurement_type` (snake_case) for the Python attribute follows Python naming conventions while the C++ struct uses `measurementType` (camelCase).

4. **Empty string default**: Using empty string (not a special constant) for "no conversion" keeps the API simple and matches the existing pattern for optional string fields like plotUnits.

### Open Questions

None - all design decisions are resolved in the overview document.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. All tests pass (manual verification steps succeed)
3. Code follows patterns established in reference files (PlotValue struct, PlotModel roles, plugin registration)
4. No TODOs or placeholder code remains
5. Project builds and runs without errors
6. All ~45 built-in plots have correct measurementType assigned
7. Plugin system supports optional measurementType with backward compatibility
8. PlotModel exposes measurementType through MeasurementTypeRole

---

Phase 3 documentation complete.
- Tasks: 4
- Estimated complexity: 5 (S=1 + S=1 + M=2 + S=1)
- Ready for implementation: Yes
