# Phase 1: Infrastructure - Implementation Document

## Overview

This document details the infrastructure changes required to support the expanded preferences system in FlySight Viewer 2. Phase 1 establishes the foundation for new preference pages by:

1. Extending `PreferencesManager` with default value retrieval
2. Registering new preferences for plots, markers, legend, and map
3. Creating utility functions for generating preference keys
4. Ensuring proper initialization order for registries and preferences

## Files to Modify

### 1. src/preferences/preferencesmanager.h

**Purpose:** Add `getDefaultValue()` method to support "Reset to Defaults" functionality.

**Current Code (lines 32-37):**
```cpp
    QVariant getValue(const QString &key) const {
        if (!m_preferences.contains(key)) {
            Q_ASSERT("Requested value for an unregistered preference!");
        }
        return m_settings.value(key, m_preferences.value(key).defaultValue);
    }
```

**Add after `getValue()` method (after line 37, before `setValue()`):**
```cpp
    QVariant getDefaultValue(const QString &key) const {
        if (!m_preferences.contains(key)) {
            qWarning() << "Requested default value for an unregistered preference:" << key;
            return QVariant();
        }
        return m_preferences.value(key).defaultValue;
    }
```

**Complete modified preferencesmanager.h:**
```cpp
#ifndef PREFERENCESMANAGER_H
#define PREFERENCESMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariant>

namespace FlySight {

struct Preference {
    QVariant defaultValue;
};

class PreferencesManager : public QObject {
    Q_OBJECT

public:
    static PreferencesManager& instance() {
        static PreferencesManager instance;
        return instance;
    }

    void registerPreference(const QString &key, const QVariant &defaultValue) {
        m_preferences[key] = Preference{defaultValue};

        // Set the default value in QSettings if the key doesn't exist yet
        if (!m_settings.contains(key)) {
            m_settings.setValue(key, defaultValue);
        }
    }

    QVariant getValue(const QString &key) const {
        if (!m_preferences.contains(key)) {
            Q_ASSERT("Requested value for an unregistered preference!");
        }
        return m_settings.value(key, m_preferences.value(key).defaultValue);
    }

    QVariant getDefaultValue(const QString &key) const {
        if (!m_preferences.contains(key)) {
            qWarning() << "Requested default value for an unregistered preference:" << key;
            return QVariant();
        }
        return m_preferences.value(key).defaultValue;
    }

    void setValue(const QString &key, const QVariant &value) {
        if (m_settings.value(key) != value) {
            m_settings.setValue(key, value);
            emit preferenceChanged(key, value);
        }
    }

signals:
    void preferenceChanged(const QString &key, const QVariant &value);

private:
    PreferencesManager() : m_settings("FlySight", "Viewer 2") {}

    QSettings m_settings;
    QMap<QString, Preference> m_preferences;

    Q_DISABLE_COPY(PreferencesManager)
};

} // namespace FlySight

#endif // PREFERENCESMANAGER_H
```

---

### 2. src/preferences/preferencekeys.h (NEW FILE)

**Purpose:** Centralize preference key generation functions for consistency across the codebase.

**Complete file:**
```cpp
#ifndef PREFERENCEKEYS_H
#define PREFERENCEKEYS_H

#include <QString>

namespace FlySight {
namespace PreferenceKeys {

// ============================================================================
// General Preferences
// ============================================================================
inline const QString GeneralUnits = QStringLiteral("general/units");
inline const QString GeneralLogbookFolder = QStringLiteral("general/logbookFolder");

// ============================================================================
// Import Preferences
// ============================================================================
inline const QString ImportGroundReferenceMode = QStringLiteral("import/groundReferenceMode");
inline const QString ImportFixedElevation = QStringLiteral("import/fixedElevation");

// ============================================================================
// Plots Preferences (global)
// ============================================================================
inline const QString PlotsLineThickness = QStringLiteral("plots/lineThickness");
inline const QString PlotsTextSize = QStringLiteral("plots/textSize");
inline const QString PlotsCrosshairColor = QStringLiteral("plots/crosshairColor");
inline const QString PlotsCrosshairThickness = QStringLiteral("plots/crosshairThickness");
inline const QString PlotsYAxisPadding = QStringLiteral("plots/yAxisPadding");

// ============================================================================
// Legend Preferences
// ============================================================================
inline const QString LegendTextSize = QStringLiteral("legend/textSize");

// ============================================================================
// Map Preferences
// ============================================================================
inline const QString MapLineThickness = QStringLiteral("map/lineThickness");
inline const QString MapMarkerSize = QStringLiteral("map/markerSize");
inline const QString MapTrackOpacity = QStringLiteral("map/trackOpacity");

// ============================================================================
// Per-Plot Preference Key Generators
// ============================================================================

/**
 * @brief Generate the preference key for a plot's color setting.
 * @param sensorID The sensor identifier (e.g., "GNSS", "IMU")
 * @param measurementID The measurement identifier (e.g., "velH", "ax")
 * @return The preference key in format "plots/{sensorID}/{measurementID}/color"
 */
inline QString plotColorKey(const QString &sensorID, const QString &measurementID) {
    return QStringLiteral("plots/%1/%2/color").arg(sensorID, measurementID);
}

/**
 * @brief Generate the preference key for a plot's Y-axis mode setting.
 * @param sensorID The sensor identifier
 * @param measurementID The measurement identifier
 * @return The preference key in format "plots/{sensorID}/{measurementID}/yAxisMode"
 */
inline QString plotYAxisModeKey(const QString &sensorID, const QString &measurementID) {
    return QStringLiteral("plots/%1/%2/yAxisMode").arg(sensorID, measurementID);
}

/**
 * @brief Generate the preference key for a plot's Y-axis minimum value.
 * @param sensorID The sensor identifier
 * @param measurementID The measurement identifier
 * @return The preference key in format "plots/{sensorID}/{measurementID}/yAxisMin"
 */
inline QString plotYAxisMinKey(const QString &sensorID, const QString &measurementID) {
    return QStringLiteral("plots/%1/%2/yAxisMin").arg(sensorID, measurementID);
}

/**
 * @brief Generate the preference key for a plot's Y-axis maximum value.
 * @param sensorID The sensor identifier
 * @param measurementID The measurement identifier
 * @return The preference key in format "plots/{sensorID}/{measurementID}/yAxisMax"
 */
inline QString plotYAxisMaxKey(const QString &sensorID, const QString &measurementID) {
    return QStringLiteral("plots/%1/%2/yAxisMax").arg(sensorID, measurementID);
}

// ============================================================================
// Per-Marker Preference Key Generators
// ============================================================================

/**
 * @brief Generate the preference key for a marker's color setting.
 * @param attributeKey The marker's unique attribute key (e.g., "exitTime", "startTime")
 * @return The preference key in format "markers/{attributeKey}/color"
 */
inline QString markerColorKey(const QString &attributeKey) {
    return QStringLiteral("markers/%1/color").arg(attributeKey);
}

} // namespace PreferenceKeys
} // namespace FlySight

#endif // PREFERENCEKEYS_H
```

---

### 3. src/mainwindow.cpp

**Purpose:** Register all new preferences and modify initialization order.

#### 3.1 Add Include Statement

**After line 34 (after `#include "preferences/preferencesmanager.h"`):**
```cpp
#include "preferences/preferencekeys.h"
```

#### 3.2 Modify Constructor Initialization Order

The current order in `MainWindow::MainWindow()` (lines 69-82) is:
1. `initializePreferences()` (line 70)
2. `initializeCalculatedAttributes()` (line 73)
3. `initializeCalculatedMeasurements()` (line 74)
4. `registerBuiltInPlots()` (line 80)
5. `registerBuiltInMarkers()` (line 81)
6. `PluginHost::instance().initialise()` (line 82)

**Change to:**
1. `registerBuiltInPlots()` - Register plots FIRST
2. `registerBuiltInMarkers()` - Register markers
3. `PluginHost::instance().initialise()` - Register plugin plots/markers
4. `initializePreferences()` - Now can iterate over all registered plots/markers
5. `initializeCalculatedAttributes()`
6. `initializeCalculatedMeasurements()`

**Replace lines 69-82 with:**
```cpp
    // Register built-in plots and markers BEFORE initializing preferences
    // so that we can dynamically register per-plot and per-marker preferences
    registerBuiltInPlots();
    registerBuiltInMarkers();

    // Initialize plugins (may register additional plots/markers)
    QString defaultDir = QCoreApplication::applicationDirPath() + "/plugins";
    QString pluginDir = qEnvironmentVariable("FLYSIGHT_PLUGINS", defaultDir);
    PluginHost::instance().initialise(pluginDir);

    // Initialize preferences (must come AFTER plots and markers are registered)
    initializePreferences();

    // Initialize calculated values
    initializeCalculatedAttributes();
    initializeCalculatedMeasurements();
```

#### 3.3 Replace initializePreferences() Implementation

**Replace the entire `initializePreferences()` function (lines 792-801) with:**
```cpp
void MainWindow::initializePreferences()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // ========================================================================
    // General Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::GeneralUnits, QStringLiteral("Metric"));
    prefs.registerPreference(PreferenceKeys::GeneralLogbookFolder,
                             QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));

    // ========================================================================
    // Import Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::ImportGroundReferenceMode, QStringLiteral("Automatic"));
    prefs.registerPreference(PreferenceKeys::ImportFixedElevation, 0.0);

    // ========================================================================
    // Global Plot Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::PlotsLineThickness, 1.0);
    prefs.registerPreference(PreferenceKeys::PlotsTextSize, 9);
    prefs.registerPreference(PreferenceKeys::PlotsCrosshairColor, QVariant::fromValue(QColor(Qt::gray)));
    prefs.registerPreference(PreferenceKeys::PlotsCrosshairThickness, 1.0);
    prefs.registerPreference(PreferenceKeys::PlotsYAxisPadding, 0.05);

    // ========================================================================
    // Per-Plot Preferences (dynamically registered from PlotRegistry)
    // ========================================================================
    const QVector<PlotValue> allPlots = PlotRegistry::instance().allPlots();
    for (const PlotValue &pv : allPlots) {
        // Color preference (default from PlotValue.defaultColor)
        prefs.registerPreference(
            PreferenceKeys::plotColorKey(pv.sensorID, pv.measurementID),
            QVariant::fromValue(pv.defaultColor)
        );

        // Y-axis mode preference (auto, fixed)
        prefs.registerPreference(
            PreferenceKeys::plotYAxisModeKey(pv.sensorID, pv.measurementID),
            QStringLiteral("auto")
        );

        // Y-axis min/max values (used when mode is "fixed")
        prefs.registerPreference(
            PreferenceKeys::plotYAxisMinKey(pv.sensorID, pv.measurementID),
            0.0
        );
        prefs.registerPreference(
            PreferenceKeys::plotYAxisMaxKey(pv.sensorID, pv.measurementID),
            100.0
        );
    }

    // ========================================================================
    // Per-Marker Preferences (dynamically registered from MarkerRegistry)
    // ========================================================================
    const QVector<MarkerDefinition> allMarkers = MarkerRegistry::instance().allMarkers();
    for (const MarkerDefinition &md : allMarkers) {
        // Color preference (default from MarkerDefinition.color)
        prefs.registerPreference(
            PreferenceKeys::markerColorKey(md.attributeKey),
            QVariant::fromValue(md.color)
        );
    }

    // ========================================================================
    // Legend Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::LegendTextSize, 9);

    // ========================================================================
    // Map Preferences
    // ========================================================================
    prefs.registerPreference(PreferenceKeys::MapLineThickness, 3.0);
    prefs.registerPreference(PreferenceKeys::MapMarkerSize, 10);
    prefs.registerPreference(PreferenceKeys::MapTrackOpacity, 0.85);
}
```

---

### 4. src/CMakeLists.txt

**Purpose:** Add the new header file to the project.

**In the `PROJECT_SOURCES` list (around line 188), after `preferences/preferencesmanager.h`, add:**
```cmake
  preferences/preferencekeys.h
```

**Updated section (lines 188-191):**
```cmake
  preferences/preferencesmanager.h
  preferences/preferencekeys.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
```

---

## Summary of Changes

| File | Change Type | Description |
|------|-------------|-------------|
| `src/preferences/preferencesmanager.h` | Modify | Add `getDefaultValue()` method |
| `src/preferences/preferencekeys.h` | **New** | Centralized preference key constants and generators |
| `src/mainwindow.cpp` | Modify | Add include, reorder initialization, expand `initializePreferences()` |
| `src/CMakeLists.txt` | Modify | Add `preferencekeys.h` to sources |

---

## Dependencies and Implementation Order

Execute changes in this order:

1. **Create `src/preferences/preferencekeys.h`**
   - No dependencies on other changes
   - Can be created immediately

2. **Modify `src/preferences/preferencesmanager.h`**
   - No dependencies on other changes
   - Can be done in parallel with step 1

3. **Modify `src/CMakeLists.txt`**
   - Depends on: Step 1 (file must exist)
   - Add the new header file to build

4. **Modify `src/mainwindow.cpp`**
   - Depends on: Steps 1-3 (header must exist and be buildable)
   - Add include statement
   - Reorder constructor calls
   - Replace `initializePreferences()` implementation

---

## Registry Keys Reference

After implementation, the following preference keys will be registered:

### Static Keys

| Key | Type | Default Value |
|-----|------|---------------|
| `general/units` | QString | "Metric" |
| `general/logbookFolder` | QString | (Documents folder) |
| `import/groundReferenceMode` | QString | "Automatic" |
| `import/fixedElevation` | double | 0.0 |
| `plots/lineThickness` | double | 1.0 |
| `plots/textSize` | int | 9 |
| `plots/crosshairColor` | QColor | Qt::gray |
| `plots/crosshairThickness` | double | 1.0 |
| `plots/yAxisPadding` | double | 0.05 |
| `legend/textSize` | int | 9 |
| `map/lineThickness` | double | 3.0 |
| `map/markerSize` | int | 10 |
| `map/trackOpacity` | double | 0.85 |

### Dynamic Keys (Per-Plot)

For each registered plot (sensor/measurement pair):

| Key Pattern | Type | Default Value |
|-------------|------|---------------|
| `plots/{sensorID}/{measurementID}/color` | QColor | (from PlotValue.defaultColor) |
| `plots/{sensorID}/{measurementID}/yAxisMode` | QString | "auto" |
| `plots/{sensorID}/{measurementID}/yAxisMin` | double | 0.0 |
| `plots/{sensorID}/{measurementID}/yAxisMax` | double | 100.0 |

**Example keys for built-in plots:**
- `plots/GNSS/velH/color`
- `plots/GNSS/velH/yAxisMode`
- `plots/IMU/ax/color`
- `plots/BARO/pressure/yAxisMin`

### Dynamic Keys (Per-Marker)

For each registered marker:

| Key Pattern | Type | Default Value |
|-------------|------|---------------|
| `markers/{attributeKey}/color` | QColor | (from MarkerDefinition.color) |

**Example keys for built-in markers:**
- `markers/exitTime/color`
- `markers/startTime/color`

---

## Testing Checklist

### Build Verification

- [ ] Project compiles without errors
- [ ] No new warnings introduced
- [ ] All existing tests pass

### PreferencesManager Tests

- [ ] `getDefaultValue()` returns correct default for registered preferences
- [ ] `getDefaultValue()` returns empty QVariant for unregistered keys
- [ ] `getDefaultValue()` logs warning for unregistered keys
- [ ] Existing `getValue()` and `setValue()` behavior unchanged

### Preference Registration Tests

- [ ] All static preferences are registered at startup
- [ ] Per-plot preferences are registered for each PlotRegistry entry
- [ ] Per-marker preferences are registered for each MarkerRegistry entry
- [ ] Default values match specification

### PreferenceKeys Utility Tests

- [ ] `plotColorKey("GNSS", "velH")` returns `"plots/GNSS/velH/color"`
- [ ] `plotYAxisModeKey("IMU", "ax")` returns `"plots/IMU/ax/yAxisMode"`
- [ ] `plotYAxisMinKey("BARO", "pressure")` returns `"plots/BARO/pressure/yAxisMin"`
- [ ] `plotYAxisMaxKey("MAG", "x")` returns `"plots/MAG/x/yAxisMax"`
- [ ] `markerColorKey("exitTime")` returns `"markers/exitTime/color"`

### Initialization Order Tests

- [ ] `registerBuiltInPlots()` called before `initializePreferences()`
- [ ] `registerBuiltInMarkers()` called before `initializePreferences()`
- [ ] `PluginHost::initialise()` called before `initializePreferences()`
- [ ] `PlotRegistry::allPlots()` returns all plots when `initializePreferences()` runs
- [ ] `MarkerRegistry::allMarkers()` returns all markers when `initializePreferences()` runs

### Regression Tests

- [ ] Application launches without crashes
- [ ] Existing preference values are preserved (not reset)
- [ ] Plots display correctly
- [ ] Markers display correctly
- [ ] Import functionality works
- [ ] General settings (units, logbook folder) work

### Registry Persistence Tests

- [ ] New preferences are persisted to Windows Registry
- [ ] Values survive application restart
- [ ] Default values are written on first launch
- [ ] Registry path is `HKEY_CURRENT_USER\Software\FlySight\Viewer 2`

---

## Verification Steps

After implementation, verify the changes work correctly:

1. **Build the project:**
   ```
   cmake --build . --config Release
   ```

2. **Run the application** and check that it starts without errors.

3. **Open Windows Registry Editor** (regedit) and navigate to:
   ```
   HKEY_CURRENT_USER\Software\FlySight\Viewer 2
   ```

4. **Verify new keys exist:**
   - `plots/lineThickness`
   - `plots/textSize`
   - `plots/crosshairColor`
   - `plots/GNSS/velH/color`
   - `markers/exitTime/color`
   - `legend/textSize`
   - `map/lineThickness`

5. **Test getDefaultValue()** by adding temporary debug code:
   ```cpp
   qDebug() << "Default line thickness:"
            << PreferencesManager::instance().getDefaultValue(PreferenceKeys::PlotsLineThickness);
   ```

---

## Notes

### QColor Storage in QSettings

Qt's `QSettings` can store `QColor` values directly when using `QVariant::fromValue()`. The color is serialized as a string in the format `@Variant(...)`. When reading back, use:

```cpp
QColor color = prefs.getValue(key).value<QColor>();
```

### Plugin-Registered Plots

Plugins may register additional plots via `PlotRegistry::registerPlot()`. The current implementation ensures these are registered before `initializePreferences()` runs, so their preferences will also be registered.

### Backward Compatibility

Existing preference values are preserved. The `registerPreference()` method only sets the default value if the key doesn't already exist in QSettings:

```cpp
if (!m_settings.contains(key)) {
    m_settings.setValue(key, defaultValue);
}
```

This means users who have already customized their preferences will retain their settings.
