# Phase 4: Display Integration

## Overview

This phase integrates the UnitConverter into the display layer to convert SI-stored values to user-preferred units (Metric or Imperial) when rendering. The primary integration points are PlotWidget (for Y-axis labels, tick values, and data display) and LegendPresenter (for formatted values shown when the cursor hovers over the plot). This phase also establishes reactive update patterns so that changing the unit system immediately refreshes all visible converted values.

## Dependencies

- **Depends on:** Phase 1 (Core Unit System) - requires UnitConverter singleton with convert/format/getUnitLabel methods; Phase 2 (Data Import Normalization) - all SessionData values are in SI units; Phase 3 (PlotRegistry Extension) - PlotValue struct has measurementType field
- **Blocks:** Phase 5 (UI and Preferences)
- **Assumptions:**
  - UnitConverter singleton exists at `src/units/unitconverter.h` with methods: `convert()`, `getUnitLabel()`, `getPrecision()`, `format()`, and signal `systemChanged(QString)`
  - All data in SessionData is normalized to SI units (m/s, m, m/s^2, Tesla, Celsius, etc.)
  - PlotValue struct includes `measurementType` field (QString) that maps to UnitDefinitions categories
  - PlotModel exposes `MeasurementTypeRole` for accessing measurementType through Qt's model interface

## Tasks

### Task 4.1: Include UnitConverter in PlotWidget

**Purpose:** Add the necessary include and establish connection to the UnitConverter's systemChanged signal for reactive updates.

**Files to modify:**
- `src/plotwidget.h` - Add forward declaration
- `src/plotwidget.cpp` - Add include and signal connection

**Technical Approach:**

1. In `src/plotwidget.h`, add forward declaration in the FlySight namespace (after line 24):
   ```cpp
   class UnitConverter;
   ```
   Note: Forward declaration is sufficient since we only need to connect to the singleton.

2. In `src/plotwidget.cpp`, add include at the top (after line 7):
   ```cpp
   #include "units/unitconverter.h"
   ```

3. In the PlotWidget constructor (around line 120-125, after connecting to PreferencesManager), add connection to UnitConverter:
   ```cpp
   // Connect to unit system changes for reactive updates
   connect(&UnitConverter::instance(), &UnitConverter::systemChanged,
           this, &PlotWidget::updatePlot);
   ```

   This follows the pattern established in `crosshairmanager.cpp` lines 36-37 for connecting to PreferencesManager::preferenceChanged.

**Acceptance Criteria:**
- [ ] `units/unitconverter.h` is included in plotwidget.cpp
- [ ] PlotWidget constructor connects to UnitConverter::systemChanged signal
- [ ] Signal connection triggers updatePlot() when unit system changes
- [ ] Code compiles without errors

**Complexity:** S

---

### Task 4.2: Convert Y-Axis Labels in PlotWidget

**Purpose:** Display Y-axis labels with the user's preferred unit system (e.g., "Elevation (ft)" instead of "Elevation (m)" when Imperial is selected).

**Files to modify:**
- `src/plotwidget.cpp` - Modify updatePlot() method

**Technical Approach:**

In the `updatePlot()` method (lines 263-320), modify the Y-axis label creation section. Currently (around line 267-269):

```cpp
if (!plotUnits.isEmpty()) {
    newYAxis->setLabel(plotName + " (" + plotUnits + ")");
} else {
    newYAxis->setLabel(plotName);
}
```

Change to:

```cpp
// Determine display unit label based on measurementType
QString displayUnits = plotUnits; // Fallback to static plotUnits
if (!pv.measurementType.isEmpty()) {
    QString convertedLabel = UnitConverter::instance().getUnitLabel(pv.measurementType);
    if (!convertedLabel.isEmpty()) {
        displayUnits = convertedLabel;
    }
}

if (!displayUnits.isEmpty()) {
    newYAxis->setLabel(plotName + " (" + displayUnits + ")");
} else {
    newYAxis->setLabel(plotName);
}
```

Key considerations:
1. If measurementType is empty, use the original plotUnits (backward compatible)
2. If measurementType is set but UnitConverter returns empty label, fall back to plotUnits
3. The actual data values are NOT converted here - only the label. Data conversion happens during display in the legend and potentially in tick label formatting (Task 4.3)

**Acceptance Criteria:**
- [ ] Y-axis labels show unit from UnitConverter when measurementType is set
- [ ] Y-axis labels fall back to plotUnits when measurementType is empty
- [ ] Switching unit system updates Y-axis labels on next plot update
- [ ] Plots without measurementType continue to show their original plotUnits

**Complexity:** S

---

### Task 4.3: Convert Y-Axis Tick Values in PlotWidget

**Purpose:** Display Y-axis tick values in the user's preferred unit system so that the numeric scale matches the axis label.

**Files to modify:**
- `src/plotwidget.cpp` - Create custom QCPAxisTicker subclass and apply in updatePlot()

**Technical Approach:**

QCustomPlot uses QCPAxisTicker to generate tick labels. To convert tick values, we need a custom ticker that applies unit conversion. There are two approaches:

**Approach A: QCPAxisTickerText with manual conversion**
This is simpler but requires recalculating ticks when range changes:
- Use QCPAxisTickerText
- When Y-axis range changes, calculate tick positions in SI, convert to display units, and set tick labels

**Approach B: Custom QCPAxisTicker subclass (Recommended)**
More robust and integrates naturally with QCustomPlot's tick system:

1. Create a local helper class in plotwidget.cpp (before the FlySight namespace, around line 23):

```cpp
namespace {

class UnitConvertingTicker : public QCPAxisTicker
{
public:
    void setMeasurementType(const QString &type) { m_measurementType = type; }
    void setScale(double scale) { m_scale = scale; }
    void setOffset(double offset) { m_offset = offset; }
    void setPrecision(int precision) { m_precision = precision; }

protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
    {
        Q_UNUSED(locale)
        Q_UNUSED(formatChar)

        // Convert SI value to display units
        double displayValue = (tick * m_scale) + m_offset;

        // Use our precision, not the default
        int usePrecision = (m_precision >= 0) ? m_precision : precision;
        return QString::number(displayValue, 'f', usePrecision);
    }

private:
    QString m_measurementType;
    double m_scale = 1.0;
    double m_offset = 0.0;
    int m_precision = -1;
};

} // anonymous namespace
```

2. In updatePlot(), when creating the Y-axis (after line 278), configure the ticker:

```cpp
// Configure unit-converting ticker for this axis
if (!pv.measurementType.isEmpty()) {
    auto ticker = QSharedPointer<UnitConvertingTicker>::create();
    ticker->setMeasurementType(pv.measurementType);

    // Get conversion parameters from UnitConverter
    // Note: We need scale/offset from UnitDefinitions - UnitConverter may need to expose these
    // For now, we can compute scale by converting 1.0 and offset by converting 0.0
    double scale = UnitConverter::instance().convert(1.0, pv.measurementType);
    double offset = UnitConverter::instance().convert(0.0, pv.measurementType);
    // Actual scale = (convert(1) - convert(0)), offset = convert(0)
    ticker->setScale(scale - offset);
    ticker->setOffset(offset);
    ticker->setPrecision(UnitConverter::instance().getPrecision(pv.measurementType));

    newYAxis->setTicker(ticker);
}
```

Note on scale/offset derivation:
- For linear conversion `y = x * scale + offset`:
  - `convert(0) = offset`
  - `convert(1) = scale + offset`
  - Therefore: `scale = convert(1) - convert(0)`, `offset = convert(0)`

**Acceptance Criteria:**
- [ ] Y-axis tick values are displayed in user's preferred units
- [ ] Tick values match the unit shown in the axis label
- [ ] Precision matches what UnitConverter specifies for the measurement type
- [ ] Plots without measurementType use default QCPAxisTicker behavior
- [ ] Temperature conversion (with offset) displays correctly

**Complexity:** M

---

### Task 4.4: Include UnitConverter in LegendPresenter

**Purpose:** Add the necessary include and establish connection to the UnitConverter's systemChanged signal for reactive legend updates.

**Files to modify:**
- `src/legendpresenter.h` - No changes needed (uses signals from existing models)
- `src/legendpresenter.cpp` - Add include and signal connection

**Technical Approach:**

1. In `src/legendpresenter.cpp`, add include at the top (after line 9):
   ```cpp
   #include "units/unitconverter.h"
   ```

2. In the LegendPresenter constructor (around line 257), add connection to UnitConverter's systemChanged signal:
   ```cpp
   // Connect to unit system changes for reactive updates
   connect(&UnitConverter::instance(), &UnitConverter::systemChanged,
           this, &LegendPresenter::scheduleUpdate);
   ```

   This follows the existing pattern in the constructor (lines 237-257) where connections to various models trigger scheduleUpdate().

**Acceptance Criteria:**
- [ ] `units/unitconverter.h` is included in legendpresenter.cpp
- [ ] LegendPresenter constructor connects to UnitConverter::systemChanged signal
- [ ] Signal connection triggers scheduleUpdate() when unit system changes
- [ ] Legend values update when unit system is toggled

**Complexity:** S

---

### Task 4.5: Update seriesDisplayName for Unit Labels

**Purpose:** Display the correct unit label in the legend series name based on the user's unit system preference.

**Files to modify:**
- `src/legendpresenter.cpp` - Modify seriesDisplayName() function

**Technical Approach:**

The current `seriesDisplayName()` function (lines 25-31) constructs the display name using plotUnits:

```cpp
QString seriesDisplayName(const PlotValue &pv)
{
    if (!pv.plotUnits.isEmpty()) {
        return QString("%1 (%2)").arg(pv.plotName).arg(pv.plotUnits);
    }
    return pv.plotName;
}
```

Modify to use UnitConverter when measurementType is available:

```cpp
QString seriesDisplayName(const PlotValue &pv)
{
    QString displayUnits = pv.plotUnits; // Fallback

    if (!pv.measurementType.isEmpty()) {
        QString convertedLabel = UnitConverter::instance().getUnitLabel(pv.measurementType);
        if (!convertedLabel.isEmpty()) {
            displayUnits = convertedLabel;
        }
    }

    if (!displayUnits.isEmpty()) {
        return QString("%1 (%2)").arg(pv.plotName).arg(displayUnits);
    }
    return pv.plotName;
}
```

This mirrors the logic in Task 4.2 for consistency.

**Acceptance Criteria:**
- [ ] Legend row names show units from UnitConverter when measurementType is set
- [ ] Legend row names fall back to plotUnits when measurementType is empty
- [ ] Switching unit system updates legend row names
- [ ] Format is consistent: "PlotName (units)"

**Complexity:** S

---

### Task 4.6: Convert Values in formatValue Function

**Purpose:** Display converted values in the legend when hovering over the plot, using the user's preferred unit system.

**Files to modify:**
- `src/legendpresenter.cpp` - Modify formatValue() function and its callers

**Technical Approach:**

The current `formatValue()` function (lines 73-88) formats raw values with measurement-specific precision:

```cpp
QString formatValue(double value, const QString &measurementId)
{
    if (std::isnan(value))
        return QStringLiteral("--");

    int precision = 1;

    const QString m = measurementId.toLower();
    if (m.contains(QStringLiteral("lat")) || m.contains(QStringLiteral("lon"))) {
        precision = 6;
    } else if (m.contains(QStringLiteral("time"))) {
        precision = 3;
    }

    return QString::number(value, 'f', precision);
}
```

There are two approaches:

**Approach A: Modify formatValue signature to include measurementType**

Change the function signature and implementation:

```cpp
QString formatValue(double value, const QString &measurementId, const QString &measurementType)
{
    if (std::isnan(value))
        return QStringLiteral("--");

    // Preserve coordinate precision regardless of unit system (per constraints)
    const QString m = measurementId.toLower();
    if (m.contains(QStringLiteral("lat")) || m.contains(QStringLiteral("lon"))) {
        return QString::number(value, 'f', 6);
    }

    // Apply unit conversion if measurementType is specified
    if (!measurementType.isEmpty()) {
        double displayValue = UnitConverter::instance().convert(value, measurementType);
        int precision = UnitConverter::instance().getPrecision(measurementType);
        if (precision < 0) precision = 1; // Fallback
        return QString::number(displayValue, 'f', precision);
    }

    // Legacy fallback: use measurement-based heuristics
    int precision = 1;
    if (m.contains(QStringLiteral("time"))) {
        precision = 3;
    }

    return QString::number(value, 'f', precision);
}
```

Update all call sites to pass measurementType:
- Line 389: `row.value = formatValue(v, pv.measurementID, pv.measurementType);`
- Lines 466-468: `formatValue(minVal, pv.measurementID, pv.measurementType)` etc.

**Approach B: Create separate convertAndFormat helper**

Add a new helper function for converted values, keeping formatValue for legacy paths:

```cpp
QString convertAndFormatValue(double siValue, const QString &measurementId, const QString &measurementType)
{
    if (std::isnan(siValue))
        return QStringLiteral("--");

    // Preserve coordinate precision (constraint from overview)
    const QString m = measurementId.toLower();
    if (m.contains(QStringLiteral("lat")) || m.contains(QStringLiteral("lon"))) {
        return QString::number(siValue, 'f', 6);
    }

    // Convert and format using UnitConverter
    if (!measurementType.isEmpty()) {
        return UnitConverter::instance().format(siValue, measurementType);
    }

    // Fallback for plots without measurementType
    return formatValue(siValue, measurementId);
}
```

**Recommended: Approach A** - modifying the existing function maintains a single code path and is easier to reason about.

**Acceptance Criteria:**
- [ ] Legend values are converted from SI to display units
- [ ] Precision matches UnitConverter's specification for each measurement type
- [ ] Latitude/longitude values retain 6 decimal places (per constraint)
- [ ] NaN values continue to display as "--"
- [ ] Values update when unit system changes

**Complexity:** M

---

### Task 4.7: Update Header Legend Information

**Purpose:** Ensure the coordinates displayed in the legend header maintain appropriate precision and units.

**Files to modify:**
- `src/legendpresenter.cpp` - Review and potentially modify coordinate display in recompute()

**Technical Approach:**

The legend header (lines 416-425 in recompute()) displays coordinates:

```cpp
if (!std::isnan(lat) && !std::isnan(lon) && !std::isnan(alt)) {
    coordsText = QStringLiteral("(%1 deg, %2 deg, %3 m)")
                     .arg(lat, 0, 'f', 7)
                     .arg(lon, 0, 'f', 7)
                     .arg(alt, 0, 'f', 3);
}
```

Per the constraint "Latitude/longitude must remain at 6 decimal places regardless of unit system", coordinates should not be converted. However, altitude should be converted:

```cpp
if (!std::isnan(lat) && !std::isnan(lon) && !std::isnan(alt)) {
    // Convert altitude to display units
    double displayAlt = UnitConverter::instance().convert(alt, QStringLiteral("altitude"));
    QString altUnit = UnitConverter::instance().getUnitLabel(QStringLiteral("altitude"));
    int altPrecision = UnitConverter::instance().getPrecision(QStringLiteral("altitude"));
    if (altPrecision < 0) altPrecision = 1; // Fallback

    coordsText = QStringLiteral("(%1 deg, %2 deg, %3 %4)")
                     .arg(lat, 0, 'f', 7)
                     .arg(lon, 0, 'f', 7)
                     .arg(displayAlt, 0, 'f', altPrecision)
                     .arg(altUnit);
}
```

Note: The overview constraint says "6 decimal places" for coordinates but the current code uses 7. Maintain the existing 7 for backward compatibility unless explicitly asked to change.

**Acceptance Criteria:**
- [ ] Altitude in legend header is converted to display units
- [ ] Altitude unit label matches current system (m or ft)
- [ ] Latitude and longitude remain in degrees with existing precision
- [ ] Coordinate format remains consistent and readable

**Complexity:** S

---

### Task 4.8: Update GraphInfo Display Name Construction

**Purpose:** Ensure the graph display names (used for tooltips and internal tracking) reflect the current unit system.

**Files to modify:**
- `src/plotwidget.cpp` - Modify display name construction in updatePlot()

**Technical Approach:**

In updatePlot() (around lines 304-308), the GraphInfo display name is constructed:

```cpp
if (!plotUnits.isEmpty()) {
    info.displayName = QString("%1 (%2)").arg(plotName).arg(plotUnits);
} else {
    info.displayName = plotName;
}
```

This should use the converted unit label for consistency:

```cpp
// Determine display unit label based on measurementType
QString displayUnits = plotUnits; // Fallback
if (!pv.measurementType.isEmpty()) {
    QString convertedLabel = UnitConverter::instance().getUnitLabel(pv.measurementType);
    if (!convertedLabel.isEmpty()) {
        displayUnits = convertedLabel;
    }
}

if (!displayUnits.isEmpty()) {
    info.displayName = QString("%1 (%2)").arg(plotName).arg(displayUnits);
} else {
    info.displayName = plotName;
}
```

This mirrors the logic in Tasks 4.2 and 4.5 for consistency. Consider extracting this into a helper function to avoid code duplication.

**Acceptance Criteria:**
- [ ] GraphInfo displayName uses unit from UnitConverter when available
- [ ] Display name falls back to plotUnits when measurementType is empty
- [ ] Display name updates when plot is rebuilt after unit system change

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No formal unit test framework is in use. Manual verification is the primary testing approach.

### Integration Tests

1. **Build verification**: Project compiles without errors after all modifications
2. **Signal connectivity**: Verify UnitConverter::systemChanged triggers updates in PlotWidget and LegendPresenter
3. **Reactive updates**: Changing unit system immediately updates all visible displays

### Manual Verification

1. **Y-axis label verification**:
   - Load a session with GNSS data
   - Enable "Elevation" plot
   - Verify Y-axis label shows "Elevation (m)" in Metric mode
   - Switch to Imperial (via preferences or "U" key after Phase 5)
   - Verify Y-axis label shows "Elevation (ft)"

2. **Y-axis tick verification**:
   - In Metric mode, note tick values (e.g., 0, 500, 1000 m)
   - Switch to Imperial
   - Verify tick values show converted values (e.g., 0, 1640, 3281 ft)
   - Verify tick values are reasonable (not truncated or overflowing)

3. **Legend value verification**:
   - Hover cursor over plot to show legend
   - Note displayed values in Metric (e.g., "120.5 m/s")
   - Switch to Imperial
   - Verify values show converted amounts (e.g., "269.6 mph")
   - Verify precision matches UnitConverter specification

4. **Temperature verification** (includes offset):
   - Enable a temperature plot (IMU or Barometer)
   - In Metric, note a value (e.g., "25.0 degC")
   - Switch to Imperial
   - Verify conversion includes offset (25C = 77F, not 45F)

5. **Coordinate preservation**:
   - Hover over plot to show legend header with coordinates
   - Verify lat/lon remain in degrees with 7 decimal places
   - Verify altitude changes with unit system

6. **Edge cases**:
   - Verify NaN values still display as "--"
   - Verify plots without measurementType show original plotUnits
   - Verify empty data doesn't cause crashes

## Notes for Implementer

### Gotchas

1. **Include order**: `units/unitconverter.h` must be included after Qt headers but before FlySight headers that might use it.

2. **Scale/offset derivation**: The UnitConverter provides convert() but not direct access to scale/offset. The derivation `scale = convert(1) - convert(0), offset = convert(0)` works for affine transforms but assumes the conversion is linear. This is true for all defined measurement types.

3. **Precision fallback**: Always check if getPrecision() returns -1 (unknown type) and provide a sensible default (1 decimal place).

4. **Signal connection order**: Connect to UnitConverter::systemChanged before the initial updatePlot() call in the constructor. Otherwise, the initial plot won't use the correct units if preferences were changed while the app was closed.

5. **Coordinate precision constraint**: The overview specifies 6 decimal places for coordinates, but existing code uses 7. Maintain backward compatibility unless explicitly asked to change.

6. **QSharedPointer for tickers**: QCustomPlot expects QSharedPointer<QCPAxisTicker> for custom tickers. Don't use raw pointers or std::shared_ptr.

7. **Reentrant signal handling**: If setSystem() is called during a slot connected to systemChanged, it could cause issues. The pattern of checking for actual changes before emitting (in UnitConverter) prevents infinite loops.

8. **Performance**: The formatValue function is called frequently (every cursor move). The UnitConverter lookup should be O(1)/O(log n) as specified in constraints. Don't add expensive operations.

### Decisions Made

1. **Custom ticker approach**: Using a QCPAxisTicker subclass rather than QCPAxisTickerText because it integrates more naturally with QCustomPlot's tick generation and doesn't require manual tick position calculation.

2. **Scale/offset derivation**: Deriving scale and offset from convert(0) and convert(1) rather than adding new getScale()/getOffset() methods to UnitConverter. This keeps the UnitConverter API simpler.

3. **Single formatValue function**: Modifying the existing formatValue() signature rather than creating a parallel function. This avoids code duplication and ensures all value formatting goes through one path.

4. **Coordinate altitude conversion**: Converting altitude in the legend header coordinates while preserving lat/lon. This matches user expectations (altitude is a measured distance that changes with unit system).

5. **Helper function extraction**: The unit label determination logic (check measurementType, fallback to plotUnits) appears in multiple places. Consider extracting to a helper, but this is left to implementer discretion to avoid over-engineering.

### Open Questions

None - all design decisions are resolved based on the overview document and Phase 1/3 specifications.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. All tests pass (manual verification steps succeed)
3. Code follows patterns established in reference files (signal connections, preference handling)
4. No TODOs or placeholder code remains
5. Project builds and runs without errors
6. Y-axis labels display correct unit for current system
7. Y-axis tick values are converted to display units
8. Legend values are converted to display units with correct precision
9. Changing unit system immediately updates all displays
10. Plots without measurementType continue to work with original behavior

---

Phase 4 documentation complete.
- Tasks: 8
- Estimated complexity: 10 (S=1 + S=1 + M=2 + S=1 + S=1 + M=2 + S=1 + S=1)
- Ready for implementation: Yes
