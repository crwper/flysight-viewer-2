# Phase 6: Plugin Support

## Overview

This phase enables Python plugins to specify `measurement_type` for registered plots, allowing plugin-defined plots to participate in the unit conversion system. The core implementation (reading `measurement_type` from Python objects and passing to PlotValue) was already completed in Phase 3, Task 3.4. This phase focuses on documenting the complete plugin API and providing examples and guidance for plugin authors.

## Dependencies

- **Depends on:** Phase 3 (PlotRegistry Extension) - requires PlotValue.measurementType field and the C++ code that reads `measurement_type` from Python plugin objects
- **Blocks:** None
- **Assumptions:**
  - Phase 3 Task 3.4 is complete: `pluginhost.cpp` reads `measurement_type` from Python plot objects using `py::hasattr()`
  - PlotValue struct has `measurementType` as its 7th field
  - UnitDefinitions from Phase 1 defines all valid measurement type strings
  - The plugin SDK file `plugins/flysight_plugin_sdk.py` exists and defines the `SimplePlot` dataclass

## Tasks

### Task 6.1: Update SimplePlot Dataclass in Python SDK

**Purpose:** Add the `measurement_type` field to the `SimplePlot` dataclass so plugin authors can specify unit conversion categories for their plots.

**Files to modify:**
- `plugins/flysight_plugin_sdk.py` - Add `measurement_type` field to `SimplePlot` dataclass

**Technical Approach:**

Update the `SimplePlot` dataclass (lines 61-68 in `flysight_plugin_sdk.py`) to include an optional `measurement_type` field:

Current definition:
```python
@dataclass(frozen=True)
class SimplePlot:
    category:    str
    name:        str
    units:       Optional[str]
    color:       str     # CSS color string, e.g. "#1E88E5"
    sensor:      str
    measurement: str
```

Updated definition:
```python
@dataclass(frozen=True)
class SimplePlot:
    category:    str
    name:        str
    units:       Optional[str]
    color:       str     # CSS color string, e.g. "#1E88E5"
    sensor:      str
    measurement: str
    measurement_type: Optional[str] = None  # Unit conversion category (e.g., "speed", "altitude")
```

Key considerations:
1. Use snake_case `measurement_type` per Python naming conventions (PEP 8)
2. Default to `None` for backward compatibility with existing plugins
3. The field must be optional with a default value since it comes after required fields in a frozen dataclass
4. Type hint as `Optional[str]` to match the existing `units` field pattern

**Acceptance Criteria:**
- [ ] `measurement_type` field added to SimplePlot dataclass with type `Optional[str]`
- [ ] Field defaults to `None` for backward compatibility
- [ ] Existing plugins that create SimplePlot without `measurement_type` continue to work
- [ ] New plugins can specify `measurement_type` when creating SimplePlot instances

**Complexity:** S

---

### Task 6.2: Add Docstring Documentation to SimplePlot

**Purpose:** Provide inline documentation for plugin authors explaining the measurement_type field and available values.

**Files to modify:**
- `plugins/flysight_plugin_sdk.py` - Add docstring to SimplePlot class

**Technical Approach:**

Add a class-level docstring to SimplePlot that explains:
1. What the dataclass is for
2. Each field's purpose
3. Available measurement_type values and when to use them

Example docstring to add after the class declaration:

```python
@dataclass(frozen=True)
class SimplePlot:
    """
    Defines a simple plot that displays a measurement from SessionData.

    Attributes:
        category: Category name for grouping in the plot selection UI (e.g., "GNSS", "IMU")
        name: Display name of the plot (e.g., "Ground Speed", "Elevation")
        units: Display units string for the y-axis (e.g., "m/s"). Set to None if unitless.
               Note: This is the display string only; actual conversion uses measurement_type.
        color: CSS color string for the plot line (e.g., "#1E88E5", "blue", "rgb(30,136,229)")
        sensor: Sensor ID in SessionData (e.g., "GNSS", "IMU", "BARO")
        measurement: Measurement ID within the sensor (e.g., "hMSL", "velN", "temperature")
        measurement_type: Optional unit conversion category. When set, the UnitConverter
            automatically converts values between metric and imperial systems.

            Available measurement types:
            - "distance": meters <-> feet (for horizontal distances, accuracy values)
            - "altitude": meters <-> feet (for elevation, vertical position)
            - "speed": m/s <-> mph (for horizontal speeds)
            - "vertical_speed": m/s <-> mph (for vertical speeds)
            - "acceleration": m/s^2 <-> g's (displayed as g in both systems)
            - "temperature": Celsius <-> Fahrenheit
            - "pressure": Pascals <-> inHg
            - "rotation": deg/s (same in both systems)
            - "angle": degrees (same in both systems)
            - "magnetic_field": Tesla <-> gauss
            - "voltage": Volts (same in both systems)
            - "percentage": % (same in both systems)
            - "time": seconds (same in both systems)
            - "count": unitless integers (same in both systems)

            Leave as None (default) for plots that should not be unit-converted.

    Example:
        # A speed plot that converts between m/s and mph:
        register_plot(SimplePlot(
            category="My Plugin",
            name="Custom Speed",
            units="m/s",  # Base metric units
            color="#FF5722",
            sensor="GNSS",
            measurement="customSpeed",
            measurement_type="speed"
        ))
    """
    category:    str
    name:        str
    units:       Optional[str]
    color:       str
    sensor:      str
    measurement: str
    measurement_type: Optional[str] = None
```

**Acceptance Criteria:**
- [ ] SimplePlot has a comprehensive docstring explaining all fields
- [ ] All 14 measurement_type values are listed with brief descriptions
- [ ] Example usage is included showing how to register a plot with measurement_type
- [ ] Docstring is accessible via Python's help() function

**Complexity:** S

---

### Task 6.3: Add Module-Level Documentation

**Purpose:** Provide overview documentation at the module level explaining the plugin SDK and unit conversion integration.

**Files to modify:**
- `plugins/flysight_plugin_sdk.py` - Add/update module-level docstring

**Technical Approach:**

Add or expand the module-level docstring at the top of `flysight_plugin_sdk.py` (after the `from __future__ import annotations` import) to document:
1. Overview of the plugin SDK
2. How to create plugins
3. Unit conversion integration

Example module docstring:

```python
"""
FlySight Plugin SDK

This module provides the API for creating FlySight Viewer 2 plugins that can:
- Register calculated attributes (single values derived from session data)
- Register calculated measurements (arrays of values, one per sample)
- Register simple plots (display existing or calculated measurements)

Unit Conversion Support
-----------------------
Plugins can specify a `measurement_type` when registering plots to enable automatic
unit conversion between metric and imperial systems. The user's unit preference is
set in Preferences > General or toggled with the "U" keyboard shortcut.

When a plot has a measurement_type, the display layer automatically:
- Converts y-axis values to the user's preferred unit system
- Updates the y-axis label with the correct unit
- Formats values with appropriate precision

Example Plugin
--------------
```python
from flysight_plugin_sdk import (
    MeasurementPlugin, SimplePlot,
    register_measurement, register_plot, meas
)
import numpy as np

class GroundSpeed(MeasurementPlugin):
    name = "groundSpeed"
    sensor = "GNSS"
    units = "m/s"

    def inputs(self):
        return [meas("GNSS", "velN"), meas("GNSS", "velE")]

    def compute(self, session):
        velN = np.array(session.getMeasurement("GNSS", "velN"))
        velE = np.array(session.getMeasurement("GNSS", "velE"))
        return np.sqrt(velN**2 + velE**2)

register_measurement(GroundSpeed())

register_plot(SimplePlot(
    category="GNSS",
    name="Ground Speed",
    units="m/s",
    color="#1E88E5",
    sensor="GNSS",
    measurement="groundSpeed",
    measurement_type="speed"  # Enables m/s <-> mph conversion
))
```

Available Measurement Types
---------------------------
See SimplePlot docstring for the complete list of measurement types and their
conversion behaviors.
"""
```

**Acceptance Criteria:**
- [ ] Module has a comprehensive docstring at the top
- [ ] Unit conversion feature is clearly explained
- [ ] Example plugin code demonstrates measurement_type usage
- [ ] Documentation references SimplePlot for measurement type details

**Complexity:** S

---

### Task 6.4: Verify C++ Plugin Integration

**Purpose:** Verify that the Phase 3 implementation correctly reads measurement_type from Python plugins and confirm the integration works end-to-end.

**Files to review (no modification needed if Phase 3 complete):**
- `src/pluginhost.cpp` - Lines 316-331 (simple plot registration)

**Technical Approach:**

This is a verification task to ensure Phase 3, Task 3.4 was implemented correctly. Review the code in `pluginhost.cpp` section 6 ("Register simple plot definitions") and verify:

1. The code uses `py::hasattr(plt, "measurement_type")` to check for the optional attribute
2. The code handles the case where `measurement_type` is `None`
3. The code defaults to empty QString when the attribute is missing or None
4. The measurementType is passed as the 7th argument to PlotRegistry::registerPlot()

Expected code pattern (from Phase 3 documentation):
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

If Phase 3 was not fully implemented, this task becomes implementing the above pattern.

**Acceptance Criteria:**
- [ ] `pluginhost.cpp` reads `measurement_type` attribute from Python plugin objects
- [ ] Missing `measurement_type` attribute defaults to empty QString
- [ ] `None` value for `measurement_type` defaults to empty QString
- [ ] measurementType is passed as 7th field to PlotValue initializer
- [ ] Code follows existing null-check pattern from `units` field (lines 323-324)

**Complexity:** S (verification only) or M (if implementation needed)

---

## Testing Requirements

### Unit Tests

No formal unit test framework is in use. Manual verification is the primary testing approach.

### Integration Tests

1. **Build verification**: Project compiles without errors after SDK modifications
2. **Plugin loading**: Existing plugins load without errors
3. **New plugin support**: New plugins with measurement_type load and register correctly

### Manual Verification

1. **SDK field verification**:
   - Start Python interpreter in the plugins directory
   - Import flysight_plugin_sdk
   - Verify SimplePlot accepts measurement_type parameter:
     ```python
     from flysight_plugin_sdk import SimplePlot
     plot = SimplePlot("Cat", "Name", "m/s", "#FF0000", "GNSS", "velN", "speed")
     print(plot.measurement_type)  # Should print "speed"
     ```

2. **Backward compatibility verification**:
   - Verify existing plugins load without modification:
     ```python
     # Existing plugin syntax still works
     plot = SimplePlot("Cat", "Name", "m/s", "#FF0000", "GNSS", "velN")
     print(plot.measurement_type)  # Should print None
     ```

3. **Documentation verification**:
   - Run `python -c "from flysight_plugin_sdk import SimplePlot; help(SimplePlot)"`
   - Verify docstring displays with all measurement types listed

4. **End-to-end plugin verification**:
   Create a test plugin file `plugins/test_units_plugin.py`:
   ```python
   from flysight_plugin_sdk import SimplePlot, register_plot

   # Test plot with measurement_type
   register_plot(SimplePlot(
       category="Test",
       name="Speed Test",
       units="m/s",
       color="#FF5722",
       sensor="GNSS",
       measurement="velN",
       measurement_type="speed"
   ))

   # Test plot without measurement_type (backward compat)
   register_plot(SimplePlot(
       category="Test",
       name="Raw Value",
       units="custom",
       color="#4CAF50",
       sensor="GNSS",
       measurement="velE"
   ))
   ```

   - Start application
   - Verify both plots appear in plot selection under "Test" category
   - Verify no errors in console during plugin loading
   - (After Phase 4/5) Toggle units and verify "Speed Test" converts while "Raw Value" does not

## Notes for Implementer

### Gotchas

1. **Dataclass field ordering**: In Python dataclasses, fields with default values must come after fields without defaults. Since `measurement_type` has a default of `None`, it must be the last field in SimplePlot.

2. **Frozen dataclass**: SimplePlot is `frozen=True`, meaning instances are immutable. This is intentional and should not be changed.

3. **String matching with C++**: The `measurement_type` string must exactly match the C++ attribute name `measurement_type` (snake_case). The C++ code uses `py::hasattr(plt, "measurement_type")`.

4. **None vs empty string**: In Python, `None` indicates "not specified". The C++ code converts both `None` and missing attributes to empty QString. Plugins can use either `None` or omit the field entirely.

5. **No custom measurement types**: Per the overview document, "Plugins will use built-in measurement types only (no custom definitions)". Plugins must use one of the 14 predefined types or leave measurement_type as None.

### Decisions Made

1. **Documentation scope**: Comprehensive docstrings are added to SimplePlot and the module level. This serves as the primary plugin author documentation since there is no separate developer guide.

2. **Example code style**: Examples use explicit keyword arguments for clarity, even though positional arguments would work.

3. **Verification task**: Task 6.4 is primarily a verification task since Phase 3, Task 3.4 should have implemented the C++ side. If that implementation is incomplete, the complexity increases.

### Open Questions

None - all design decisions were resolved in the overview document and Phase 3 documentation. Plugin API uses built-in measurement types only.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. All tests pass (manual verification steps succeed)
3. SimplePlot dataclass includes measurement_type field with default None
4. Comprehensive docstrings are added to SimplePlot and module level
5. C++ plugin registration (from Phase 3) correctly reads measurement_type
6. Existing plugins continue to work without modification
7. New plugins can specify measurement_type and have it passed to PlotRegistry
8. Plugin author documentation is accessible via Python help() system

---

Phase 6 documentation complete.
- Tasks: 4
- Estimated complexity: 4 (S=1 + S=1 + S=1 + S=1, or 5 if Task 6.4 requires implementation)
- Ready for implementation: Yes
