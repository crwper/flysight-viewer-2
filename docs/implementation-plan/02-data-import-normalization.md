# Phase 2: Data Import Normalization

## Overview

This phase normalizes sensor data to SI units during the import process using **unit text from file headers** to drive conversions. Rather than hardcoding sensor/field names, the importer uses a lookup table that maps unit text strings (e.g., "g", "gauss") to SI conversion factors. This approach is more maintainable and easily extensible for future unit types.

## Dependencies

- **Depends on:** None - can begin immediately
- **Blocks:** Phase 4 (Display Integration) - display layer needs to know data is in SI to apply correct conversion factors
- **Assumptions:**
  - FS1 files have unit text in row 2 (e.g., `,(deg),(deg),(m),(m/s),...`)
  - FS2 files have `$UNIT` lines (e.g., `$UNIT,IMU,s,deg/s,deg/s,deg/s,g,g,g,deg C`)
  - Unit text is currently parsed but discarded in both formats

## Tasks

### Task 2.1: Create UnitConversion Header

**Purpose:** Define a lookup table mapping unit text strings to SI conversion factors, enabling data-driven normalization during import.

**Files to create:**
- `src/units/unitconversion.h` - Header-only lookup table and conversion API

**Technical Approach:**

Create a header-only file with a static lookup table and conversion functions:

1. **ConversionSpec struct** - Holds conversion parameters:
   ```cpp
   struct ConversionSpec {
       double scale;      // raw * scale + offset = SI
       double offset;     // for affine transforms (unused for current units)
       QString siUnit;    // resulting SI unit (for debugging)
   };
   ```

2. **Lookup table** - Map unit text to conversion specs:
   ```cpp
   static const QHash<QString, ConversionSpec>& lookup() {
       static QHash<QString, ConversionSpec> table = []() {
           QHash<QString, ConversionSpec> t;

           // === Units already in SI (scale=1, offset=0) ===
           t["m"]      = {1.0, 0.0, "m"};
           t["m/s"]    = {1.0, 0.0, "m/s"};
           t["Pa"]     = {1.0, 0.0, "Pa"};
           t["s"]      = {1.0, 0.0, "s"};
           t["deg"]    = {1.0, 0.0, "deg"};
           t["deg/s"]  = {1.0, 0.0, "deg/s"};
           t["V"]      = {1.0, 0.0, "V"};
           t["%"]      = {1.0, 0.0, "%"};
           t[""]       = {1.0, 0.0, ""};      // dimensionless (time, numSV, week)
           t["deg C"]  = {1.0, 0.0, "degC"};  // keep Celsius, don't convert to Kelvin

           // === Units requiring conversion ===
           t["g"]      = {9.80665, 0.0, "m/s^2"};   // acceleration
           t["gauss"]  = {0.0001, 0.0, "T"};        // magnetic field -> Tesla

           // === Aliases for FS1 parenthesized format ===
           t["(m)"]    = t["m"];
           t["(m/s)"]  = t["m/s"];
           t["(deg)"]  = t["deg"];

           // === Aliases for common variations ===
           t["volt"]    = t["V"];
           t["percent"] = t["%"];

           return t;
       }();
       return table;
   }
   ```

3. **Public API**:
   ```cpp
   // Get conversion spec for a unit string (returns identity if unknown)
   static ConversionSpec getConversion(const QString& unitText);

   // Check if unit requires conversion (scale != 1 or offset != 0)
   static bool requiresConversion(const QString& unitText);

   // Convert entire vector in-place (optimized batch operation)
   static void toSI(QVector<double>& values, const QString& unitText);
   ```

4. **Unknown unit handling**: Return identity conversion (scale=1, offset=0) with a `qWarning()` log message.

**Known Unit Text Values:**

| Unit Text | Scale | Offset | SI Unit | Source |
|-----------|-------|--------|---------|--------|
| `m` | 1.0 | 0 | m | FS1/FS2 GNSS |
| `m/s` | 1.0 | 0 | m/s | FS1/FS2 GNSS |
| `deg` | 1.0 | 0 | deg | FS1/FS2 GNSS |
| `Pa` | 1.0 | 0 | Pa | FS2 BARO |
| `s` | 1.0 | 0 | s | FS2 all sensors |
| `g` | 9.80665 | 0 | m/s^2 | FS2 IMU |
| `deg/s` | 1.0 | 0 | deg/s | FS2 IMU |
| `gauss` | 0.0001 | 0 | T | FS2 MAG |
| `deg C` | 1.0 | 0 | degC | FS2 temperature (keep as Celsius) |
| `percent` | 1.0 | 0 | % | FS2 HUM |
| `volt` | 1.0 | 0 | V | FS2 VBAT |
| `(m)`, `(m/s)`, `(deg)` | (aliases) | | | FS1 format |
| `` (empty) | 1.0 | 0 | (none) | FS1/FS2 dimensionless |

**Acceptance Criteria:**
- [ ] `ConversionSpec` struct defined with scale, offset, and siUnit fields
- [ ] All known unit text values from FS1 and FS2 have entries
- [ ] FS1 parenthesized aliases are included
- [ ] `getConversion()` returns correct spec for each unit
- [ ] `requiresConversion()` returns false for already-SI units (fast path)
- [ ] Unknown units default to identity conversion with warning
- [ ] Header compiles without errors when included

**Complexity:** M

---

### Task 2.2: Capture and Apply FS1 Unit Text

**Purpose:** Parse unit text from FS1 row 2 and use it to drive SI conversion during import.

**Files to modify:**
- `src/dataimporter.cpp` - Modify `importSimple()` function

**Technical Approach:**

Currently, `importSimple()` (lines 96-121) reads row 2 but discards it:
```cpp
// Read the second line (units), IGNORE
in.readLine();  // line 114
```

Change to capture and use the unit text:

1. **Parse unit text from row 2**:
   ```cpp
   // Read the second line (units)
   QString unitLine = in.readLine();
   QVector<QString> units;
   for (const auto& part : unitLine.split(',')) {
       units.append(part.trimmed());
   }
   ```

2. **Store column-to-unit mapping** for later use:
   ```cpp
   // Map column index to unit text
   QVector<QString> columnUnits = units;
   ```

3. **After all data rows are imported**, apply SI conversions:
   ```cpp
   // Apply SI normalization based on unit text
   const QVector<QString>& cols = columnOrder[sensorName];
   for (int i = 0; i < cols.size() && i < columnUnits.size(); ++i) {
       const QString& unitText = columnUnits[i];
       if (UnitConversion::requiresConversion(unitText)) {
           QVector<double>& data = sessionData.getMeasurementRef(sensorName, cols[i]);
           UnitConversion::toSI(data, unitText);
       }
   }
   ```

**Note:** FS1 format is GNSS-only, and all GNSS units are already SI (m, m/s, deg), so no actual conversion will occur. However, implementing this ensures consistency and handles any future FS1 variants.

**Acceptance Criteria:**
- [ ] Unit text from row 2 is captured into a vector
- [ ] `UnitConversion::requiresConversion()` is called for each column
- [ ] `UnitConversion::toSI()` is called for columns that need conversion
- [ ] Columns with SI units (m, m/s, deg) are not converted (fast path)
- [ ] Empty unit text (time, numSV) is handled gracefully

**Complexity:** S

---

### Task 2.3: Capture and Apply FS2 Unit Text

**Purpose:** Parse `$UNIT` lines in FS2 format and use them to drive SI conversion during import.

**Files to modify:**
- `src/dataimporter.cpp` - Modify `importHeaderRow()` and `importFS2()` functions

**Technical Approach:**

Currently, `importHeaderRow()` (lines 183-185) ignores `$UNIT` lines:
```cpp
} else if (token0 == u"$UNIT") {
    // Ignore $UNIT lines
}
```

1. **Add member variable** to store unit text per sensor:
   ```cpp
   // In DataImporter class or as local variable in importFS2()
   QMap<QString, QVector<QString>> m_columnUnits;  // sensor -> unit list
   ```

2. **Parse `$UNIT` lines** in `importHeaderRow()`:
   ```cpp
   } else if (token0 == u"$UNIT") {
       if (it != tokenizer.end()) {
           QString sensorName = (*it++).toString();
           QVector<QString> units;
           for (; it != tokenizer.end(); ++it) {
               units.append(it->toString());
           }
           m_columnUnits[sensorName] = units;
       }
   }
   ```

3. **After all data is imported** in `importFS2()`, apply SI conversions:
   ```cpp
   // Apply SI normalization based on unit text
   for (auto sensorIt = m_columnUnits.constBegin();
        sensorIt != m_columnUnits.constEnd(); ++sensorIt) {
       const QString& sensor = sensorIt.key();
       const QVector<QString>& units = sensorIt.value();
       const QVector<QString>& cols = columnOrder.value(sensor);

       for (int i = 0; i < cols.size() && i < units.size(); ++i) {
           const QString& unitText = units[i];
           if (UnitConversion::requiresConversion(unitText)) {
               QVector<double>& data = sessionData.getMeasurementRef(sensor, cols[i]);
               UnitConversion::toSI(data, unitText);
           }
       }
   }
   ```

**Acceptance Criteria:**
- [ ] `$UNIT` lines are parsed and stored in `m_columnUnits`
- [ ] Unit text is correctly associated with columns per sensor
- [ ] IMU acceleration columns (ax, ay, az with unit "g") are converted to m/s^2
- [ ] MAG columns (x, y, z with unit "gauss") are converted to Tesla
- [ ] Other units (Pa, deg/s, deg C, s, V, %) pass through unchanged
- [ ] Missing `$UNIT` line for a sensor is handled gracefully (no conversion)

**Complexity:** M

---

### Task 2.4: Update IMU-GNSS EKF Fusion to Expect SI Units

**Purpose:** The sensor fusion code currently converts IMU acceleration from g's to m/s² internally. After Tasks 2.2/2.3, the data will already be in SI, so this internal conversion must be removed.

**Files to modify:**
- `src/imugnssekf.cpp` - Remove redundant g-to-m/s² conversions

**Technical Approach:**

In `imugnssekf.cpp`, the `runFusion()` function and related code currently multiply IMU acceleration values by `kG2ms2` because it assumes input is in g's. After this phase, input will already be in m/s².

Locations to modify:

1. **Line 144**: `avgAcc *= kG2ms2;` - REMOVE this line
   - Context: `calculateInitialOrientation()` averages accelerometer readings and converts to m/s²
   - After change: Input is already m/s², no conversion needed

2. **Line 334**: `Vector3 acc(imuAx[currentImuIdx - 1] * kG2ms2, imuAy[currentImuIdx - 1] * kG2ms2, imuAz[currentImuIdx - 1] * kG2ms2);`
   - Change to: `Vector3 acc(imuAx[currentImuIdx - 1], imuAy[currentImuIdx - 1], imuAz[currentImuIdx - 1]);`
   - Context: IMU integration step in the main fusion loop

3. **Lines 423-425**: Output acceleration conversion to g's
   ```cpp
   out.accN.append(acc.x()*kms22G);
   out.accE.append(acc.y()*kms22G);
   out.accD.append(acc.z()*kms22G);
   ```
   - Change to:
   ```cpp
   out.accN.append(acc.x());
   out.accE.append(acc.y());
   out.accD.append(acc.z());
   ```
   - After change: Output remains in m/s² (SI); display layer converts to g's

4. **Remove unused constant** `kms22G` (line 44) if no longer used after the above changes

**Acceptance Criteria:**
- [ ] `calculateInitialOrientation()` no longer multiplies averaged acceleration by `kG2ms2`
- [ ] `runFusion()` no longer multiplies IMU acceleration by `kG2ms2` when creating the `acc` vector
- [ ] Fusion output acceleration values (`accN`, `accE`, `accD`) are in m/s² (not g's)
- [ ] Code compiles without errors
- [ ] Sensor fusion produces valid orientation and position estimates (no NaN values in output)

**Complexity:** M

---

### Task 2.5: Review Calculated Measurements Using IMU/MAG Data

**Purpose:** Ensure calculated values that use IMU acceleration or MAG data work correctly with SI input data.

**Files to review:**
- `src/mainwindow.cpp` - Calculated measurements using IMU/MAG data

**Technical Approach:**

The `initializeCalculatedMeasurements()` function in `mainwindow.cpp` registers calculated values. Review these:

1. **`"IMU", "aTotal"`** (lines 1087-1115) - Total acceleration magnitude:
   ```cpp
   aTotal.append(std::sqrt(ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]));
   ```
   - After Phase 2: Input `ax`, `ay`, `az` will be in m/s² (not g's)
   - Output `aTotal` will therefore be in m/s² (not g's)
   - **No code changes needed** - formula is unit-agnostic (magnitude calculation)

2. **`"MAG", "total"`** (lines 1147-1175) - Total magnetic field magnitude:
   ```cpp
   total.append(std::sqrt(x[i]*x[i] + y[i]*y[i] + z[i]*z[i]));
   ```
   - After Phase 2: Input will be in Tesla (not gauss)
   - Output will be in Tesla
   - **No code changes needed** - formula is unit-agnostic

3. **`"IMU", "wTotal"`** (lines 1117-1145) - Total rotation rate:
   - Uses rotation data (deg/s), not acceleration
   - **No changes needed**

**Note:** The display layer (Phase 4) will convert these SI values back to user-friendly units (g's, gauss) using the `measurementType` field.

**Acceptance Criteria:**
- [ ] Confirm `"IMU", "aTotal"` calculation uses no hardcoded unit conversion factors
- [ ] Confirm `"MAG", "total"` calculation uses no hardcoded unit conversion factors
- [ ] Document that display units ("g", "gauss") will be handled by Phase 4's UnitConverter

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No formal unit test framework is in use. Manual verification is the primary testing approach.

### Integration Tests

- Import a FlySight 2 data file with known IMU and MAG values
- Verify that stored values in SessionData are in SI units:
  - IMU acceleration: Original value × 9.80665
  - MAG field: Original value × 0.0001

### Manual Verification

1. **Import FS2 file with IMU data:**
   - Add temporary debug output in dataimporter.cpp after conversion:
     ```cpp
     qDebug() << "IMU ax first value:" << sessionData.getMeasurement("IMU", "ax").first();
     ```
   - If original file has `ax = 0.1` (in g), stored value should be ~0.981 m/s²

2. **Import FS2 file with MAG data:**
   - If original file has `x = 0.5` (in gauss), stored value should be 0.00005 T

3. **Import FS1 file:**
   - Verify GNSS values are unchanged (already SI)
   - No conversion should occur for m, m/s, deg columns

4. **Sensor fusion test:**
   - After loading a file with IMU data, enable the "Sensor fusion" plots
   - Verify the fusion algorithm produces sensible position and orientation estimates
   - Verify no NaN values appear in the fusion output

5. **Unknown unit test:**
   - Temporarily add an unknown unit to a test file
   - Verify warning is logged and identity conversion is applied

## Notes for Implementer

### Gotchas

1. **Unit text parsing:** FS1 units are parenthesized (e.g., `(m/s)`), FS2 units are not. The lookup table includes both forms.

2. **Empty unit text:** Some columns (time, numSV, week) have empty unit strings. These map to identity conversion.

3. **Column/unit count mismatch:** If the number of columns doesn't match the number of units, only convert columns that have corresponding unit entries.

4. **Conversion timing:** Apply conversions AFTER all data rows are imported, not during row-by-row import. This is more efficient (batch conversion) and simpler to implement.

5. **Sensor fusion output units:** The `FusionOutput` struct stores acceleration outputs. After this phase, those outputs will be in m/s². The display layer will convert back to g's.

6. **Temperature decision:** Temperature is kept in Celsius, not converted to Kelvin. This simplifies display layer logic.

### Decisions Made

1. **Unit-text-driven conversion:** Using unit text from file headers rather than hardcoding sensor/field names. This is more maintainable and extensible.

2. **Lookup table location:** Created as a separate header `src/units/unitconversion.h` for clean separation from display-time conversions in `unitdefinitions.h`.

3. **No metadata storage:** Unit text is used only for conversion lookup during import, then discarded. SessionData does not store original units.

4. **Temperature in Celsius:** Kept as Celsius rather than converting to Kelvin to simplify display layer.

5. **Batch conversion:** Convert entire vectors after import rather than value-by-value during import for better performance.

### Future Extensibility

Adding support for a new unit requires only one line in the lookup table:

```cpp
// Example: Adding support for knots
t["kn"] = {0.514444, 0.0, "m/s"};  // 1 knot = 0.514444 m/s
t["kt"] = t["kn"];  // alias
```

### Open Questions

None. All design decisions were addressed in the feature specification and refinement discussion.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. `UnitConversion` lookup table contains all known unit text values
3. FS1 row 2 unit text is captured and used for conversion
4. FS2 `$UNIT` lines are parsed and used for conversion
5. IMU acceleration data is stored in m/s² after import
6. MAG magnetic field data is stored in Tesla after import
7. Sensor fusion algorithm works correctly with SI input data
8. Calculated measurements (aTotal, MAG total) produce correct magnitudes
9. No TODOs or placeholder code remains
10. Code compiles and runs without errors or warnings related to these changes

---

Phase 2 documentation complete.
- Tasks: 5
- Estimated complexity: 7 (M=2 + S=1 + M=2 + M=2 + S=1 = 8, adjusted to 7)
- Ready for implementation: Yes
