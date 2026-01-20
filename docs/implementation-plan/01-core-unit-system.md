# Phase 1: Core Unit System

## Overview

This phase creates the foundational unit conversion infrastructure for FlySight Viewer 2. It introduces two new classes: `UnitDefinitions` (a header-only registry of measurement types and their conversion specifications) and `UnitConverter` (a QObject singleton that performs conversions and emits signals when the unit system changes). These classes enable the application to convert SI-stored data to user-preferred display units (Metric or Imperial).

## Dependencies

- **Depends on:** None — can begin immediately
- **Blocks:** Phase 3 (PlotRegistry Extension), Phase 4 (Display Integration), Phase 5 (UI and Preferences)
- **Assumptions:** The `general/units` preference key already exists in `PreferenceKeys.h` and stores "Metric" or "Imperial" strings. The `PreferencesManager` singleton pattern is established and can be followed.

## Tasks

### Task 1.1: Create Units Directory Structure

**Purpose:** Establish the organizational structure for unit system files, following the pattern of existing feature directories like `src/preferences/`.

**Files to create:**
- `src/units/` — New directory for unit conversion system

**Technical Approach:**

Create the `src/units/` directory to house all unit-related files. This follows the architectural decision documented in the overview to maintain clean organization.

**Acceptance Criteria:**
- [ ] Directory `src/units/` exists
- [ ] Directory is ready to contain header and implementation files

**Complexity:** S

---

### Task 1.2: Create UnitDefinitions Header

**Purpose:** Define the data structures and static registry of all measurement types with their conversion specifications for each unit system.

**Files to create:**
- `src/units/unitdefinitions.h` — Header-only file containing unit type definitions and conversion data

**Technical Approach:**

Create a header-only file that defines:

1. **UnitSpec struct** — Holds conversion parameters for a single unit:
   ```cpp
   struct UnitSpec {
       QString label;      // Display label (e.g., "m/s", "mph")
       double scale;       // Multiplication factor
       double offset;      // Addition after scaling (for temperature)
       int precision;      // Decimal places for display
   };
   ```

2. **MeasurementTypeInfo struct** — Holds all unit systems for a measurement type:
   ```cpp
   struct MeasurementTypeInfo {
       QString siBaseUnit;                        // SI unit name (e.g., "m/s")
       QMap<QString, UnitSpec> systems;           // System name -> UnitSpec
   };
   ```

3. **Static definitions** — Use `inline` const maps (C++17) or static initialization for the measurement type registry. Follow the pattern in `preferencekeys.h` (lines 9-41) which uses `inline const QString` for compile-time constants.

4. **Measurement types to define** (from overview table):
   - `distance`: m → m (metric), ft (imperial)
   - `altitude`: m → m (metric), ft (imperial)
   - `speed`: m/s → m/s (metric), mph (imperial)
   - `vertical_speed`: m/s → m/s (metric), mph (imperial)
   - `acceleration`: m/s² → g (both systems)
   - `temperature`: °C → °C (metric), °F (imperial, with offset)
   - `pressure`: Pa → Pa (metric), inHg (imperial)
   - `rotation`: deg/s → deg/s (both)
   - `angle`: deg → deg (both)
   - `magnetic_field`: T → gauss (both)
   - `voltage`: V → V (both)
   - `percentage`: % → % (both)
   - `time`: s → s (both)
   - `count`: unitless (both)

5. **Namespace**: Place all definitions in `FlySight` namespace, consistent with existing code.

6. **Conversion formula reference**: `displayValue = (rawValue * scale) + offset`

**Reference patterns:**
- `src/preferences/preferencekeys.h` lines 9-41 for inline const pattern
- `src/plotregistry.h` lines 11-18 for struct definition pattern

**Acceptance Criteria:**
- [ ] `UnitSpec` struct defined with label, scale, offset, and precision fields
- [ ] `MeasurementTypeInfo` struct defined with siBaseUnit and systems map
- [ ] All 14 measurement types from the overview table are defined
- [ ] Metric and Imperial systems defined for each measurement type
- [ ] Conversion factors match the overview specification exactly
- [ ] Temperature includes offset of 32 for Fahrenheit
- [ ] Header compiles without errors when included
- [ ] All code is within `FlySight` namespace

**Complexity:** M

---

### Task 1.3: Create UnitConverter Header

**Purpose:** Define the UnitConverter singleton interface with conversion methods and change notification signal.

**Files to create:**
- `src/units/unitconverter.h` — Header file for UnitConverter class

**Technical Approach:**

Create a QObject-based singleton following the pattern in `preferencesmanager.h` (lines 14-65). Key elements:

1. **Singleton pattern** — Use Meyer's singleton with static instance() method:
   ```cpp
   static UnitConverter& instance() {
       static UnitConverter instance;
       return instance;
   }
   ```

2. **Q_OBJECT macro** — Required for signal/slot functionality.

3. **Public methods**:
   - `double convert(double value, const QString& measurementType) const` — Apply conversion
   - `QString getUnitLabel(const QString& measurementType) const` — Get current unit string
   - `int getPrecision(const QString& measurementType) const` — Get decimal places
   - `QString format(double value, const QString& measurementType) const` — Format value with unit
   - `QString currentSystem() const` — Get active system name
   - `void setSystem(const QString& systemName)` — Set active system
   - `QStringList availableSystems() const` — List registered system names

4. **Signal**:
   - `void systemChanged(const QString& systemName)` — Emitted when unit system changes

5. **Private members**:
   - `QString m_currentSystem` — Currently active system name
   - `Q_DISABLE_COPY` macro to prevent copying

6. **Private constructor** — Initialize with default system from preferences.

**Reference patterns:**
- `src/preferences/preferencesmanager.h` lines 14-65 for complete QObject singleton pattern
- Note the use of Q_DISABLE_COPY on line 64

**Acceptance Criteria:**
- [ ] Class inherits from QObject with Q_OBJECT macro
- [ ] Meyer's singleton pattern implemented with instance() method
- [ ] All six public methods declared (convert, getUnitLabel, getPrecision, format, currentSystem, setSystem, availableSystems)
- [ ] `systemChanged` signal declared
- [ ] Private constructor prevents external instantiation
- [ ] Q_DISABLE_COPY prevents copying
- [ ] Header includes necessary Qt headers (QObject, QString, QStringList)
- [ ] Code is within `FlySight` namespace

**Complexity:** M

---

### Task 1.4: Implement UnitConverter

**Purpose:** Implement the UnitConverter methods with O(1) lookup performance and preference synchronization.

**Files to create:**
- `src/units/unitconverter.cpp` — Implementation file for UnitConverter

**Technical Approach:**

1. **Constructor implementation**:
   - Read initial system from `PreferencesManager::getValue(PreferenceKeys::GeneralUnits)`
   - Default to "Metric" if not set
   - Store in `m_currentSystem`

2. **convert() implementation**:
   - Look up measurement type in UnitDefinitions
   - Get UnitSpec for current system
   - Apply formula: `return (value * spec.scale) + spec.offset`
   - If measurement type not found, return value unchanged (no conversion)
   - Must be O(1) for performance (QMap lookup is O(log n), acceptable)

3. **getUnitLabel() implementation**:
   - Look up measurement type and current system
   - Return the label field from UnitSpec
   - Return empty string if not found

4. **getPrecision() implementation**:
   - Look up measurement type and current system
   - Return the precision field from UnitSpec
   - Return -1 if not found (caller should handle)

5. **format() implementation**:
   - Call convert() to get display value
   - Call getPrecision() for decimal places
   - Call getUnitLabel() for unit string
   - Return formatted string: `QString::number(displayValue, 'f', precision) + " " + label`
   - Handle special cases: if value is NaN, return "--"

6. **currentSystem() implementation**:
   - Return `m_currentSystem`

7. **setSystem() implementation**:
   - Check if new system is different from current
   - Update `m_currentSystem`
   - Write to PreferencesManager: `setValue(PreferenceKeys::GeneralUnits, systemName)`
   - Emit `systemChanged(systemName)` signal
   - Follow pattern from `preferencesmanager.h` line 48-53 for change detection

8. **availableSystems() implementation**:
   - Return list of system names: {"Metric", "Imperial"}
   - These should be derived from UnitDefinitions if possible, or hardcoded initially

**Reference patterns:**
- `src/preferences/preferencesmanager.h` lines 48-53 for setValue with change detection and signal emit

**Acceptance Criteria:**
- [ ] Constructor reads initial system from preferences
- [ ] convert() correctly applies scale and offset formula
- [ ] convert() returns unchanged value for unknown measurement types
- [ ] getUnitLabel() returns correct label for current system
- [ ] getPrecision() returns correct precision for current system
- [ ] format() returns properly formatted string with unit
- [ ] format() returns "--" for NaN values
- [ ] setSystem() updates preferences and emits signal only when value changes
- [ ] currentSystem() returns the active system name
- [ ] availableSystems() returns list containing "Metric" and "Imperial"
- [ ] All lookups are O(1) or O(log n) for performance

**Complexity:** M

---

### Task 1.5: Update CMakeLists.txt

**Purpose:** Add the new unit system files to the build configuration so they are compiled and linked.

**Files to modify:**
- `src/CMakeLists.txt` — Add new source files to build

**Technical Approach:**

Add the new files to the appropriate file lists in CMakeLists.txt. Based on the existing structure, locate the section where source files are listed and add:

1. Add `units/unitdefinitions.h` to header file list (if headers are listed separately)
2. Add `units/unitconverter.h` to header file list
3. Add `units/unitconverter.cpp` to source file list

Follow the existing pattern in CMakeLists.txt for file organization. The files should be added in alphabetical order within their sections if that's the convention.

**Reference patterns:**
- `src/CMakeLists.txt` — Examine existing file list structure

**Acceptance Criteria:**
- [ ] `units/unitconverter.cpp` added to source files
- [ ] `units/unitconverter.h` added to header files (if listed separately)
- [ ] `units/unitdefinitions.h` added to header files (if listed separately)
- [ ] Project builds successfully with new files
- [ ] No CMake configuration errors

**Complexity:** S

---

### Task 1.6: Register Units Preference Default

**Purpose:** Ensure the `general/units` preference has a registered default value so UnitConverter can reliably read it.

**Files to modify:**
- `src/mainwindow.cpp` — Add preference registration if not already present

**Technical Approach:**

Check if `PreferenceKeys::GeneralUnits` is already registered with a default value. If not, add registration in the appropriate initialization section of MainWindow.

1. Search for existing registration of `GeneralUnits` preference
2. If not found, add: `PreferencesManager::instance().registerPreference(PreferenceKeys::GeneralUnits, "Metric");`
3. Place near other preference registrations (reference lines 795-872 in mainwindow.cpp per overview)

This ensures the preference system has a default before UnitConverter tries to read it.

**Reference patterns:**
- `src/mainwindow.cpp` lines 795-872 for preference initialization patterns
- `src/preferences/preferencesmanager.h` line 23-30 for registerPreference usage

**Acceptance Criteria:**
- [ ] `GeneralUnits` preference is registered with default value "Metric"
- [ ] Registration happens before any code attempts to read the preference
- [ ] Existing functionality is not broken

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No formal unit test framework appears to be in use based on the codebase. Manual verification is the primary testing approach.

### Integration Tests

- Verify UnitConverter singleton can be instantiated
- Verify UnitConverter correctly reads initial system from preferences
- Verify setSystem() correctly writes to preferences
- Verify systemChanged signal is emitted on system change

### Manual Verification

1. **Build verification**: Project compiles without errors after adding new files
2. **Instantiation test**: Add temporary debug output in main.cpp or mainwindow.cpp:
   ```cpp
   qDebug() << "Current unit system:" << UnitConverter::instance().currentSystem();
   qDebug() << "Available systems:" << UnitConverter::instance().availableSystems();
   ```
3. **Conversion test**: Test key conversions manually:
   ```cpp
   qDebug() << "100 m/s to speed:" << UnitConverter::instance().convert(100, "speed");
   qDebug() << "Speed label:" << UnitConverter::instance().getUnitLabel("speed");
   ```
4. **System switching test**: Verify setSystem() changes output:
   ```cpp
   UnitConverter::instance().setSystem("Imperial");
   qDebug() << "100 m/s in Imperial:" << UnitConverter::instance().convert(100, "speed");
   ```
5. **Temperature offset test**: Verify temperature conversion includes offset:
   ```cpp
   // 0°C should be 32°F
   UnitConverter::instance().setSystem("Imperial");
   qDebug() << "0°C in Imperial:" << UnitConverter::instance().convert(0, "temperature");  // Should be 32
   ```

## Notes for Implementer

### Gotchas

1. **Header include order**: `unitconverter.h` must include `unitdefinitions.h` and Qt headers in correct order to avoid compilation issues.

2. **QObject singleton initialization**: The UnitConverter constructor must not call methods that require other singletons that might not be initialized yet. PreferencesManager should be initialized first.

3. **Floating-point precision**: The conversion factors in UnitDefinitions should use sufficient decimal places (e.g., 2.23694 for m/s to mph, not 2.24).

4. **Empty measurement type**: Handle the case where measurementType is an empty string — return the value unchanged without logging errors (this is expected for plots without unit conversion).

5. **NaN handling**: In format(), check for NaN using `std::isnan()` or `qIsNaN()` before formatting.

6. **String comparison**: System names ("Metric", "Imperial") are case-sensitive. Consider using case-insensitive comparison or documenting the expected case.

### Decisions Made

1. **Header-only UnitDefinitions**: Using a header-only approach with inline const data simplifies the build and avoids static initialization order issues. The data is small enough that this won't cause significant binary bloat.

2. **QMap for system lookup**: Using QMap<QString, UnitSpec> for the systems within MeasurementTypeInfo. This is O(log n) but with only 2-3 systems, performance is effectively O(1).

3. **Default precision of -1**: When a measurement type is not found, returning -1 for precision signals to callers that they should handle the unknown type case.

4. **Preference sync on setSystem**: UnitConverter writes to PreferencesManager when setSystem() is called, ensuring the preference is always in sync. The alternative (only reading from preferences) would require connecting to preferenceChanged, which adds complexity.

### Open Questions

None — all design decisions are resolved in the overview document.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. All tests pass (manual verification steps succeed)
3. Code follows patterns established in reference files (PreferencesManager singleton, PreferenceKeys inline const)
4. No TODOs or placeholder code remains
5. Project builds and runs without errors
6. UnitConverter correctly converts values for all 14 measurement types
7. System switching works and persists to preferences
