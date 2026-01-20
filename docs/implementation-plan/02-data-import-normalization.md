# Phase 2: Data Import Normalization

## Overview

This phase normalizes sensor data to SI units during the import process. Specifically, IMU acceleration values (currently stored in g's) will be converted to m/s², and magnetometer values (currently stored in gauss) will be converted to Tesla. This establishes the foundation for a consistent SI-based data layer that calculations and the display system can rely upon.

## Dependencies

- **Depends on:** None - can begin immediately
- **Blocks:** Phase 4 (Display Integration) - display layer needs to know data is in SI to apply correct conversion factors
- **Assumptions:**
  - The FlySight 2 sensor outputs acceleration in g's (gravitational units)
  - The FlySight 2 sensor outputs magnetic field in gauss
  - All other measurements (position, velocity, temperature, pressure) are already in SI-compatible units

## Tasks

### Task 2.1: Add SI Conversion Constants to DataImporter

**Purpose:** Define the conversion factors needed to normalize sensor data from native units to SI.

**Files to modify:**
- `src/dataimporter.cpp` - Add conversion constants at the top of the file

**Technical Approach:**

Add conversion constants in the `FlySight` namespace at the top of `dataimporter.cpp`, following the pattern used in `imugnssekf.cpp` (lines 41-45):

```cpp
// SI conversion constants
static constexpr double kG2ms2 = 9.80665;      // g to m/s²
static constexpr double kGauss2Tesla = 0.0001; // gauss to Tesla
```

These should be placed after the includes and before the `DataImporter::importFile()` function definition.

**Acceptance Criteria:**
- [ ] `kG2ms2` constant is defined with value `9.80665`
- [ ] `kGauss2Tesla` constant is defined with value `0.0001`
- [ ] Constants are `static constexpr double` for compile-time evaluation
- [ ] Constants are in the `FlySight` namespace

**Complexity:** S

---

### Task 2.2: Apply SI Conversion During Data Row Import

**Purpose:** Convert IMU acceleration and MAG magnetic field values to SI units as they are parsed from the data file.

**Files to modify:**
- `src/dataimporter.cpp` - Modify `importDataRow()` function

**Technical Approach:**

In the `importDataRow()` function (lines 192-264), after parsing all values and before appending them to `SessionData`, apply unit conversions based on the sensor type and column name.

The conversion should happen in the loop at lines 259-263 where values are appended to the sensor data. Modify this section to check the sensor key and column name, applying the appropriate conversion factor:

1. For sensor key `"IMU"` with columns `"ax"`, `"ay"`, `"az"`: multiply by `kG2ms2`
2. For sensor key `"MAG"` with columns `"x"`, `"y"`, `"z"`: multiply by `kGauss2Tesla`

Implementation approach (insert before line 259):

```cpp
// Apply SI normalization for specific sensors/measurements
for (int i = 0; i < cols.size(); ++i) {
    const QString& colName = cols[i];

    // IMU acceleration: g -> m/s²
    if (key == "IMU" && (colName == "ax" || colName == "ay" || colName == "az")) {
        tempValues[i] *= kG2ms2;
    }
    // Magnetometer: gauss -> Tesla
    else if (key == "MAG" && (colName == "x" || colName == "y" || colName == "z")) {
        tempValues[i] *= kGauss2Tesla;
    }
}
```

This approach:
- Applies conversions after all parsing is complete but before storage
- Only affects the specific columns that need conversion
- Does not affect the "time" or "temperature" columns for these sensors
- Keeps the conversion logic localized in one place

**Acceptance Criteria:**
- [ ] IMU `ax`, `ay`, `az` values are multiplied by `9.80665` during import
- [ ] MAG `x`, `y`, `z` values are multiplied by `0.0001` during import
- [ ] IMU temperature and time columns are NOT converted
- [ ] MAG temperature and time columns are NOT converted
- [ ] Other sensors (GNSS, BARO, HUM, TIME, VBAT) are NOT affected
- [ ] Conversion applies to both FS1 and FS2 file formats (both use `importDataRow`)

**Complexity:** M

---

### Task 2.3: Update IMU-GNSS EKF Fusion to Expect SI Units

**Purpose:** The sensor fusion code currently converts IMU acceleration from g's to m/s² internally. After Task 2.2, the data will already be in SI, so this internal conversion must be removed.

**Files to modify:**
- `src/imugnssekf.cpp` - Remove redundant g-to-m/s² conversions

**Technical Approach:**

In `imugnssekf.cpp`, the `runFusion()` function and related code currently multiply IMU acceleration values by `kG2ms2` because it assumes input is in g's. After Phase 2, input will already be in m/s².

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
   - These lines convert the OUTPUT acceleration back to g's for display
   - After Phase 2, output should remain in m/s² (SI) for consistency with the architecture
   - Change to:
   ```cpp
   out.accN.append(acc.x());
   out.accE.append(acc.y());
   out.accD.append(acc.z());
   ```

4. **Remove unused constant** `kms22G` (line 44) if no longer used after the above changes

**Acceptance Criteria:**
- [ ] `calculateInitialOrientation()` no longer multiplies averaged acceleration by `kG2ms2`
- [ ] `runFusion()` no longer multiplies IMU acceleration by `kG2ms2` when creating the `acc` vector
- [ ] Fusion output acceleration values (`accN`, `accE`, `accD`) are in m/s² (not g's)
- [ ] Code compiles without errors
- [ ] Sensor fusion produces valid orientation and position estimates (no NaN values in output)

**Complexity:** M

---

### Task 2.4: Update Calculated Measurements Using IMU Data

**Purpose:** Ensure calculated values that use IMU acceleration work correctly with SI input data.

**Files to modify:**
- `src/mainwindow.cpp` - Review calculated measurements using IMU data

**Technical Approach:**

The `initializeCalculatedMeasurements()` function in `mainwindow.cpp` registers calculated values. The `"IMU", "aTotal"` calculated measurement (lines 1087-1115) computes the total acceleration magnitude:

```cpp
aTotal.append(std::sqrt(ax[i]*ax[i] + ay[i]*ay[i] + az[i]*az[i]));
```

After Phase 2:
- Input `ax`, `ay`, `az` will be in m/s² (not g's)
- Output `aTotal` will therefore be in m/s² (not g's)
- This is the correct behavior for SI normalization

**No code changes are required** for this calculated value - the formula is unit-agnostic (it computes magnitude from components). The output unit will naturally be in m/s² once inputs are in m/s².

However, the plot registration (line 704) shows `aTotal` with units "g":
```cpp
{"IMU", "Total acceleration", "g", QColor::fromHsl(0, 255, 128), "IMU", "aTotal"},
```

This display unit will be handled in Phase 4 (Display Integration) when the UnitConverter applies the `acceleration` measurement type conversion (m/s² to g's for display).

**Review the following calculated measurements to confirm they don't need changes:**
1. `"IMU", "aTotal"` (lines 1087-1115) - Magnitude calculation, no explicit unit assumptions
2. `"IMU", "wTotal"` (lines 1117-1145) - Uses rotation data, not acceleration, no changes needed
3. `"MAG", "total"` (lines 1147-1175) - Magnitude calculation, no explicit unit assumptions

**Acceptance Criteria:**
- [ ] Confirm `"IMU", "aTotal"` calculation uses no hardcoded unit conversion factors
- [ ] Confirm `"MAG", "total"` calculation uses no hardcoded unit conversion factors
- [ ] Document that display units ("g", "gauss") will be handled by Phase 4's UnitConverter

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No new unit tests are strictly required, but the following would be valuable if a test framework is established:

- Test that IMU acceleration values are multiplied by 9.80665 during import
- Test that MAG values are multiplied by 0.0001 during import
- Test that other sensor values are unchanged
- Test that calculated measurements (aTotal, MAG total) produce correct magnitudes

### Integration Tests

- Import a FlySight 2 data file with known IMU and MAG values
- Verify that stored values in SessionData are in SI units:
  - IMU acceleration: Original value * 9.80665
  - MAG field: Original value * 0.0001

### Manual Verification

1. **Import Test File:**
   - Open a FlySight 2 log file containing IMU and MAG data
   - Use Qt Creator debugger or add temporary qDebug statements to verify values

2. **Value Verification:**
   - If original IMU ax = 1.0 g, stored value should be approximately 9.80665 m/s²
   - If original MAG x = 0.5 gauss, stored value should be 0.00005 T

3. **Sensor Fusion Test:**
   - After loading a file with IMU data, enable the "Sensor fusion" plots
   - Verify the fusion algorithm produces sensible position and orientation estimates
   - Verify no NaN values appear in the fusion output

4. **Calculated Value Test:**
   - Enable "Total acceleration" plot
   - At rest (1g), the value should be approximately 9.80665 m/s² (will display as ~1g once Phase 4 is complete)

## Notes for Implementer

### Gotchas

1. **FS1 vs FS2 Format:** Both file formats use the same `importDataRow()` function, but FS1 files may not have IMU or MAG data. The conversion code should still work because it only converts columns that exist.

2. **Empty Data Handling:** If a sensor has no data, the loops won't execute and no conversion is needed. The existing empty-check logic handles this.

3. **Order of Operations:** The conversion must happen AFTER parsing but BEFORE storage. Don't convert the raw string during parsing - convert the double value after successful parsing.

4. **Sensor Fusion Output Units:** The `FusionOutput` struct stores acceleration outputs. After this phase, those outputs will be in m/s². The plot registry currently shows "g" units - this is intentional and will be resolved in Phase 4 when the display layer converts back to g's.

5. **Backward Compatibility:** Existing saved sessions (if any) would have data in the old units. This phase assumes we only care about newly imported data. If cached/saved session data exists, it would need migration.

### Decisions Made

1. **Conversion Location:** Chose to convert in `importDataRow()` rather than creating a separate post-processing step. This keeps all import logic together and ensures conversion happens exactly once.

2. **Sensor Fusion Output:** Decided to change fusion output from g's to m/s² for consistency with the SI normalization goal. The display layer will handle the g's conversion.

3. **No Conversion for Angular Rate:** IMU rotation (wx, wy, wz) in deg/s is kept as-is because deg/s is commonly used and the overview doesn't specify converting to rad/s.

### Open Questions

None. All design decisions were addressed in the feature specification.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. IMU acceleration data is stored in m/s² after import
3. MAG magnetic field data is stored in Tesla after import
4. Sensor fusion algorithm works correctly with SI input data
5. Calculated measurements (aTotal, MAG total) produce correct magnitudes
6. No TODOs or placeholder code remains
7. Code compiles and runs without errors or warnings related to these changes
