# Phase 6: Apply Preferences to Components

## Overview

Phase 6 connects the preference system (established in Phases 1-5) to the actual rendering components. Each component must:

1. Read preferences at initialization
2. Connect to `PreferencesManager::preferenceChanged` signal
3. Update rendering when relevant preferences change
4. Apply changes immediately without requiring application restart

This document provides detailed implementation instructions for each component that needs to respond to preference changes.

---

## Prerequisites

Before implementing Phase 6, ensure the following are complete:

- **Phase 1:** PreferencesManager with `getValue()`, `getDefaultValue()`, and `preferenceChanged` signal
- **Phase 2:** Plots preferences registered (`plots/lineThickness`, `plots/textSize`, etc.)
- **Phase 3:** Marker preferences registered (`markers/{attributeKey}/color`)
- **Phase 4:** Legend preferences registered (`legend/textSize`)
- **Phase 5:** Map preferences registered (`map/lineThickness`, `map/trackOpacity`, `map/markerSize`)

---

## General Implementation Pattern

All components follow the same pattern for consuming preferences:

```cpp
// In header file, add:
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

// In class declaration:
private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    void applyPreferences();

// In constructor or init method:
connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
        this, &ClassName::onPreferenceChanged);
applyPreferences();

// Implement the slot:
void ClassName::onPreferenceChanged(const QString &key, const QVariant &value)
{
    // Filter for relevant keys
    if (key == PreferenceKeys::SomeKey) {
        // Apply the specific change
        m_someValue = value.toDouble();
        update(); // or replot(), etc.
    }
}

// Implement initial application:
void ClassName::applyPreferences()
{
    auto &prefs = PreferencesManager::instance();
    m_someValue = prefs.getValue(PreferenceKeys::SomeKey).toDouble();
    // ... apply all relevant preferences
}
```

---

## Component 1: PlotWidget

**File:** `src/plotwidget.h`

### Header Changes

Add the following to the `PlotWidget` class declaration:

```cpp
// After existing private slots:
private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

// Add to private section:
private:
    void applyPlotPreferences();
    void applyAxisPreferences(const QString &sensorID, const QString &measurementID);

    // Cached preference values
    double m_lineThickness = 1.0;
    int m_textSize = 9;
    double m_yAxisPadding = 0.05;
```

### Implementation Changes

**File:** `src/plotwidget.cpp`

#### Add Includes

At the top of the file, add:

```cpp
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
```

#### Modify Constructor

Add preference connection and initialization at the end of the constructor (before `updatePlot()`):

```cpp
PlotWidget::PlotWidget(SessionModel *model,
                       PlotModel *plotModel,
                       MarkerModel *markerModel,
                       PlotViewSettingsModel *viewSettingsModel,
                       CursorModel *cursorModel,
                       PlotRangeModel *rangeModel,
                       QWidget *parent)
    : QWidget(parent)
    // ... existing initialization ...
{
    // ... existing constructor code ...

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &PlotWidget::onPreferenceChanged);

    // Apply initial preferences
    applyPlotPreferences();

    // update the plot with initial data (existing call)
    updatePlot();

    // ... rest of constructor ...
}
```

#### Add applyPlotPreferences Method

```cpp
void PlotWidget::applyPlotPreferences()
{
    auto &prefs = PreferencesManager::instance();

    // Cache global settings
    m_lineThickness = prefs.getValue(PreferenceKeys::PlotsLineThickness).toDouble();
    m_textSize = prefs.getValue(PreferenceKeys::PlotsTextSize).toInt();
    m_yAxisPadding = prefs.getValue(PreferenceKeys::PlotsYAxisPadding).toDouble();

    // Apply text size to axis labels
    QFont axisFont = customPlot->xAxis->labelFont();
    axisFont.setPointSize(m_textSize);
    customPlot->xAxis->setLabelFont(axisFont);
    customPlot->xAxis->setTickLabelFont(axisFont);

    // Apply to all Y axes
    for (auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it) {
        QCPAxis *yAxis = it.value();
        yAxis->setLabelFont(axisFont);
        yAxis->setTickLabelFont(axisFont);
    }
}
```

#### Add onPreferenceChanged Slot

```cpp
void PlotWidget::onPreferenceChanged(const QString &key, const QVariant &value)
{
    // Global plot settings
    if (key == PreferenceKeys::PlotsLineThickness) {
        m_lineThickness = value.toDouble();
        updatePlot(); // Rebuild graphs with new line thickness
        return;
    }

    if (key == PreferenceKeys::PlotsTextSize) {
        m_textSize = value.toInt();
        applyPlotPreferences();
        customPlot->replot();
        return;
    }

    if (key == PreferenceKeys::PlotsYAxisPadding) {
        m_yAxisPadding = value.toDouble();
        onXAxisRangeChanged(customPlot->xAxis->range()); // Recalculate Y ranges
        return;
    }

    // Per-plot color changes
    if (key.startsWith("plots/") && key.endsWith("/color")) {
        updatePlot(); // Rebuild to apply new color
        return;
    }

    // Per-plot Y-axis mode changes
    if (key.startsWith("plots/") && (key.endsWith("/yAxisMode") ||
                                      key.endsWith("/yAxisMin") ||
                                      key.endsWith("/yAxisMax"))) {
        onXAxisRangeChanged(customPlot->xAxis->range()); // Recalculate Y ranges
        return;
    }
}
```

#### Modify updatePlot Method - Line Thickness

Replace line 263:

```cpp
// BEFORE (line 263):
info.defaultPen = QPen(QColor(color));

// AFTER:
info.defaultPen = QPen(QColor(color), m_lineThickness);
```

#### Modify updatePlot Method - Per-Plot Color

In the loop where graphs are created, modify the color retrieval to check preferences:

```cpp
// In updatePlot(), inside the loop over plots, after line 210:
// Replace:
QColor color = pv.defaultColor;

// With:
QColor color = pv.defaultColor;
{
    // Check for user-configured color preference
    QString colorKey = PreferenceKeys::plotColorKey(pv.sensorID, pv.measurementID);
    QVariant colorPref = PreferencesManager::instance().getValue(colorKey);
    if (colorPref.isValid()) {
        QColor prefColor = colorPref.value<QColor>();
        if (prefColor.isValid()) {
            color = prefColor;
        }
    }
}
```

#### Modify onXAxisRangeChanged Method - Y-Axis Padding and Manual Mode

Replace lines 331-335:

```cpp
// BEFORE (lines 331-335):
if (yMin < yMax) {
    double padding = (yMax - yMin) * 0.05;
    padding = (padding == 0) ? 1.0 : padding;
    yAxis->setRange(yMin - padding, yMax + padding);
}

// AFTER:
if (yMin < yMax) {
    // Extract sensorID/measurementID from axis key
    QString axisKey = it.key(); // Format: "sensorID/measurementID"
    QStringList parts = axisKey.split('/');

    bool useManualRange = false;
    double manualMin = 0.0;
    double manualMax = 100.0;

    if (parts.size() == 2) {
        QString sensorID = parts[0];
        QString measurementID = parts[1];

        // Check Y-axis mode preference
        QString modeKey = PreferenceKeys::plotYAxisModeKey(sensorID, measurementID);
        QString mode = PreferencesManager::instance().getValue(modeKey).toString();

        if (mode.toLower() == "manual") {
            useManualRange = true;
            QString minKey = PreferenceKeys::plotYAxisMinKey(sensorID, measurementID);
            QString maxKey = PreferenceKeys::plotYAxisMaxKey(sensorID, measurementID);
            manualMin = PreferencesManager::instance().getValue(minKey).toDouble();
            manualMax = PreferencesManager::instance().getValue(maxKey).toDouble();
        }
    }

    if (useManualRange && manualMax > manualMin) {
        yAxis->setRange(manualMin, manualMax);
    } else {
        // Auto mode: use data range with padding
        double padding = (yMax - yMin) * m_yAxisPadding;
        padding = (padding == 0) ? 1.0 : padding;
        yAxis->setRange(yMin - padding, yMax + padding);
    }
}
```

---

## Component 2: CrosshairManager

**File:** `src/crosshairmanager.h`

### Header Changes

Add to the class declaration:

```cpp
private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    void applyCrosshairPreferences();

    // Cached preference values
    QColor m_crosshairColor = Qt::gray;
    double m_crosshairThickness = 1.0;
```

### Implementation Changes

**File:** `src/crosshairmanager.cpp`

#### Add Includes

```cpp
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
```

#### Modify Constructor

Add at the end of the constructor:

```cpp
CrosshairManager::CrosshairManager(QCustomPlot *plot,
                                   SessionModel *model,
                                   QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                                   QObject *parent)
    : QObject(parent)
    // ... existing initialization ...
{
    // ... existing constructor code ...

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &CrosshairManager::onPreferenceChanged);

    // Apply initial preferences
    applyCrosshairPreferences();
}
```

#### Add applyCrosshairPreferences Method

```cpp
void CrosshairManager::applyCrosshairPreferences()
{
    auto &prefs = PreferencesManager::instance();

    m_crosshairColor = prefs.getValue(PreferenceKeys::PlotsCrosshairColor).value<QColor>();
    if (!m_crosshairColor.isValid()) {
        m_crosshairColor = Qt::gray;
    }

    m_crosshairThickness = prefs.getValue(PreferenceKeys::PlotsCrosshairThickness).toDouble();
    if (m_crosshairThickness <= 0) {
        m_crosshairThickness = 1.0;
    }

    // Apply to existing crosshair lines if they exist
    if (m_crosshairH) {
        m_crosshairH->setPen(QPen(m_crosshairColor, m_crosshairThickness));
    }
    if (m_crosshairV) {
        m_crosshairV->setPen(QPen(m_crosshairColor, m_crosshairThickness));
    }

    if (m_plot) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}
```

#### Add onPreferenceChanged Slot

```cpp
void CrosshairManager::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::PlotsCrosshairColor ||
        key == PreferenceKeys::PlotsCrosshairThickness) {
        applyCrosshairPreferences();
    }
}
```

#### Modify ensureCrosshairCreated Method

Replace lines 291-300:

```cpp
// BEFORE (lines 291-300):
void CrosshairManager::ensureCrosshairCreated()
{
    if (!m_plot)
        return;

    if (!m_crosshairH) {
        m_crosshairH = new QCPItemLine(m_plot);
        m_crosshairH->setPen(QPen(Qt::gray, 1));
        m_crosshairH->setVisible(false);
    }
    if (!m_crosshairV) {
        m_crosshairV = new QCPItemLine(m_plot);
        m_crosshairV->setPen(QPen(Qt::gray, 1));
        m_crosshairV->setVisible(false);
    }
}

// AFTER:
void CrosshairManager::ensureCrosshairCreated()
{
    if (!m_plot)
        return;

    if (!m_crosshairH) {
        m_crosshairH = new QCPItemLine(m_plot);
        m_crosshairH->setPen(QPen(m_crosshairColor, m_crosshairThickness));
        m_crosshairH->setVisible(false);
    }
    if (!m_crosshairV) {
        m_crosshairV = new QCPItemLine(m_plot);
        m_crosshairV->setPen(QPen(m_crosshairColor, m_crosshairThickness));
        m_crosshairV->setVisible(false);
    }
}
```

---

## Component 3: LegendWidget

**File:** `src/legendwidget.h`

### Header Changes

Add to the class declaration:

```cpp
private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    void applyLegendPreferences();

    int m_textSize = 9;
```

### Implementation Changes

**File:** `src/legendwidget.cpp`

#### Add Includes

```cpp
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
```

#### Modify Constructor

Add at the end of the constructor (before `configureTableForMode` and `clear()`):

```cpp
LegendWidget::LegendWidget(QWidget *parent)
    : QWidget(parent)
{
    // ... existing UI setup code ...

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &LegendWidget::onPreferenceChanged);

    // Apply initial preferences
    applyLegendPreferences();

    configureTableForMode(m_mode);
    clear();
}
```

#### Add applyLegendPreferences Method

```cpp
void LegendWidget::applyLegendPreferences()
{
    auto &prefs = PreferencesManager::instance();

    m_textSize = prefs.getValue(PreferenceKeys::LegendTextSize).toInt();
    if (m_textSize < 6) {
        m_textSize = 9; // Fallback to default
    }

    // Apply to table view
    if (m_table) {
        QFont tableFont = m_table->font();
        tableFont.setPointSize(m_textSize);
        m_table->setFont(tableFont);
    }

    // Apply to header labels
    if (m_sessionLabel) {
        QFont labelFont = m_sessionLabel->font();
        labelFont.setPointSize(m_textSize);
        labelFont.setItalic(true);
        m_sessionLabel->setFont(labelFont);
    }

    if (m_utcLabel) {
        QFont labelFont = m_utcLabel->font();
        labelFont.setPointSize(m_textSize);
        m_utcLabel->setFont(labelFont);
    }

    if (m_coordsLabel) {
        QFont labelFont = m_coordsLabel->font();
        labelFont.setPointSize(m_textSize);
        m_coordsLabel->setFont(labelFont);
    }
}
```

#### Add onPreferenceChanged Slot

```cpp
void LegendWidget::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::LegendTextSize) {
        applyLegendPreferences();
    }
}
```

---

## Component 4: TrackMapModel

**File:** `src/trackmapmodel.h`

### Header Changes

Add to the class declaration:

```cpp
private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    double m_trackOpacity = 0.85;
```

### Implementation Changes

**File:** `src/trackmapmodel.cpp`

#### Add Includes

```cpp
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
```

#### Modify Constructor

Add preference connection:

```cpp
TrackMapModel::TrackMapModel(SessionModel *sessionModel,
                             PlotRangeModel *rangeModel,
                             QObject *parent)
    : QAbstractListModel(parent)
    , m_sessionModel(sessionModel)
    , m_rangeModel(rangeModel)
{
    // ... existing connection code ...

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &TrackMapModel::onPreferenceChanged);

    // Load initial preference value
    m_trackOpacity = PreferencesManager::instance().getValue(
        PreferenceKeys::MapTrackOpacity).toDouble();

    rebuild();
}
```

#### Add onPreferenceChanged Slot

```cpp
void TrackMapModel::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::MapTrackOpacity) {
        m_trackOpacity = value.toDouble();
        rebuild(); // Rebuild tracks with new opacity
    }
}
```

#### Modify colorForSession Method

Replace line 78:

```cpp
// BEFORE (line 78):
QColor TrackMapModel::colorForSession(const QString &sessionId)
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);
    c.setAlphaF(0.85);
    return c;
}

// AFTER:
QColor TrackMapModel::colorForSession(const QString &sessionId)
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);
    c.setAlphaF(m_trackOpacity);
    return c;
}
```

**Note:** Since `colorForSession` is a static method, we need to change its signature or read the preference directly:

```cpp
// Alternative: Read preference directly in static method
QColor TrackMapModel::colorForSession(const QString &sessionId)
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);

    double opacity = PreferencesManager::instance().getValue(
        PreferenceKeys::MapTrackOpacity).toDouble();
    c.setAlphaF(opacity);

    return c;
}
```

---

## Component 5: MapCursorDotModel

**File:** `src/mapcursordotmodel.h`

### Header Changes

Add to the class declaration:

```cpp
private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);
```

### Implementation Changes

**File:** `src/mapcursordotmodel.cpp`

#### Add Includes

```cpp
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
```

#### Modify Constructor

Add preference connection:

```cpp
MapCursorDotModel::MapCursorDotModel(SessionModel *sessionModel,
                                     CursorModel *cursorModel,
                                     QObject *parent)
    : QAbstractListModel(parent)
    , m_sessionModel(sessionModel)
    , m_cursorModel(cursorModel)
{
    // ... existing connection code ...

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &MapCursorDotModel::onPreferenceChanged);

    rebuild();
}
```

#### Add onPreferenceChanged Slot

```cpp
void MapCursorDotModel::onPreferenceChanged(const QString &key, const QVariant &value)
{
    Q_UNUSED(value)

    if (key == PreferenceKeys::MapTrackOpacity) {
        rebuild(); // Rebuild dots with new opacity (affects color)
    }
}
```

#### Modify colorForSession Method

Similar to TrackMapModel:

```cpp
QColor MapCursorDotModel::colorForSession(const QString &sessionId)
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);

    double opacity = PreferencesManager::instance().getValue(
        PreferenceKeys::MapTrackOpacity).toDouble();
    c.setAlphaF(opacity);

    return c;
}
```

---

## Component 6: QML Map Integration (MapDock.qml)

The QML integration requires exposing preferences to QML through a bridge object. This is more involved than the C++ components.

### Create MapPreferencesBridge Class

**File:** `src/mappreferencesbridge.h`

```cpp
#ifndef MAPPREFERENCESBRIDGE_H
#define MAPPREFERENCESBRIDGE_H

#include <QObject>
#include <QColor>

namespace FlySight {

/**
 * @brief Bridge class to expose map preferences to QML.
 *
 * This class listens to PreferencesManager changes and notifies QML
 * when map-related preferences change.
 */
class MapPreferencesBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double lineThickness READ lineThickness NOTIFY lineThicknessChanged)
    Q_PROPERTY(int markerSize READ markerSize NOTIFY markerSizeChanged)
    Q_PROPERTY(double trackOpacity READ trackOpacity NOTIFY trackOpacityChanged)

public:
    explicit MapPreferencesBridge(QObject *parent = nullptr);

    double lineThickness() const { return m_lineThickness; }
    int markerSize() const { return m_markerSize; }
    double trackOpacity() const { return m_trackOpacity; }

signals:
    void lineThicknessChanged();
    void markerSizeChanged();
    void trackOpacityChanged();

private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    void loadAllPreferences();

    double m_lineThickness = 3.0;
    int m_markerSize = 10;
    double m_trackOpacity = 0.85;
};

} // namespace FlySight

#endif // MAPPREFERENCESBRIDGE_H
```

**File:** `src/mappreferencesbridge.cpp`

```cpp
#include "mappreferencesbridge.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

MapPreferencesBridge::MapPreferencesBridge(QObject *parent)
    : QObject(parent)
{
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &MapPreferencesBridge::onPreferenceChanged);

    loadAllPreferences();
}

void MapPreferencesBridge::loadAllPreferences()
{
    auto &prefs = PreferencesManager::instance();

    m_lineThickness = prefs.getValue(PreferenceKeys::MapLineThickness).toDouble();
    m_markerSize = prefs.getValue(PreferenceKeys::MapMarkerSize).toInt();
    m_trackOpacity = prefs.getValue(PreferenceKeys::MapTrackOpacity).toDouble();
}

void MapPreferencesBridge::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::MapLineThickness) {
        m_lineThickness = value.toDouble();
        emit lineThicknessChanged();
    }
    else if (key == PreferenceKeys::MapMarkerSize) {
        m_markerSize = value.toInt();
        emit markerSizeChanged();
    }
    else if (key == PreferenceKeys::MapTrackOpacity) {
        m_trackOpacity = value.toDouble();
        emit trackOpacityChanged();
    }
}

} // namespace FlySight
```

### Modify MapWidget

**File:** `src/mapwidget.h`

Add to private members:

```cpp
class MapPreferencesBridge;

// In private section:
MapPreferencesBridge *m_preferencesBridge = nullptr;
```

**File:** `src/mapwidget.cpp`

Add include and expose bridge to QML:

```cpp
#include "mappreferencesbridge.h"

MapWidget::MapWidget(SessionModel *sessionModel,
                     CursorModel *cursorModel,
                     PlotRangeModel *rangeModel,
                     QWidget *parent)
    : QWidget(parent)
    , m_cursorModel(cursorModel)
{
    // ... existing code ...

    // Create preferences bridge for QML
    m_preferencesBridge = new MapPreferencesBridge(this);

    // Expose the bridge to QML as a context property
    view->engine()->rootContext()->setContextProperty(
        QStringLiteral("mapPreferences"), m_preferencesBridge);

    // ... rest of constructor ...
}
```

### Modify MapDock.qml

**File:** `src/qml/MapDock.qml`

#### Modify Track Line Width (line 300)

```qml
// BEFORE (line 300):
delegate: MapPolyline {
    id: poly
    z: 10
    line.width: 3

// AFTER:
delegate: MapPolyline {
    id: poly
    z: 10
    line.width: mapPreferences ? mapPreferences.lineThickness : 3
```

#### Modify Cursor Dot Size (lines 337-345)

```qml
// BEFORE (lines 337-345):
sourceItem: Rectangle {
    id: dotItem
    width: 10
    height: 10
    radius: width / 2
    color: dot.dotColor
    border.width: 2
    border.color: "white"
}

// AFTER:
sourceItem: Rectangle {
    id: dotItem
    width: mapPreferences ? mapPreferences.markerSize : 10
    height: mapPreferences ? mapPreferences.markerSize : 10
    radius: width / 2
    color: dot.dotColor
    border.width: 2
    border.color: "white"
}
```

---

## CMakeLists.txt Changes

**File:** `src/CMakeLists.txt`

Add the new bridge file to the build:

```cmake
# Find the existing map-related files section and add:
  mappreferencesbridge.h         mappreferencesbridge.cpp
```

Complete addition:

```cmake
# Around the map widget files, add:
  mapwidget.h                    mapwidget.cpp
  mappreferencesbridge.h         mappreferencesbridge.cpp
  mapcursorproxy.h               mapcursorproxy.cpp
```

---

## Summary of Files Modified

| File | Type | Changes |
|------|------|---------|
| `src/plotwidget.h` | Modify | Add preference slots, cached values |
| `src/plotwidget.cpp` | Modify | Apply preferences for line thickness, text size, colors, Y-axis |
| `src/crosshairmanager.h` | Modify | Add preference slots, cached values |
| `src/crosshairmanager.cpp` | Modify | Apply crosshair color and thickness |
| `src/legendwidget.h` | Modify | Add preference slot, cached text size |
| `src/legendwidget.cpp` | Modify | Apply legend text size |
| `src/trackmapmodel.h` | Modify | Add preference slot, cached opacity |
| `src/trackmapmodel.cpp` | Modify | Apply track opacity |
| `src/mapcursordotmodel.h` | Modify | Add preference slot |
| `src/mapcursordotmodel.cpp` | Modify | Apply color opacity |
| `src/mappreferencesbridge.h` | **New** | Bridge class for QML preferences |
| `src/mappreferencesbridge.cpp` | **New** | Bridge implementation |
| `src/mapwidget.h` | Modify | Add bridge member |
| `src/mapwidget.cpp` | Modify | Create and expose bridge to QML |
| `src/qml/MapDock.qml` | Modify | Bind to preferences bridge |
| `src/CMakeLists.txt` | Modify | Add new source files |

---

## Testing Checklist

### Build Verification

- [ ] Project compiles without errors after all changes
- [ ] No new compiler warnings introduced
- [ ] Application starts successfully

### Plot Widget Tests

- [ ] **Line Thickness:**
  - [ ] Changing `plots/lineThickness` preference updates graph line width
  - [ ] Default value (1.0) renders correctly
  - [ ] Maximum value (10.0) renders correctly
  - [ ] Changes apply without reopening plot

- [ ] **Text Size:**
  - [ ] Changing `plots/textSize` preference updates axis labels
  - [ ] Tick labels update with new font size
  - [ ] Axis title labels update with new font size

- [ ] **Y-Axis Padding:**
  - [ ] Changing `plots/yAxisPadding` affects Y-axis range calculation
  - [ ] 5% padding (0.05) provides visible margin above/below data
  - [ ] 0% padding shows data at exact limits

- [ ] **Per-Plot Colors:**
  - [ ] Changing `plots/{sensor}/{measurement}/color` updates that plot's color
  - [ ] Color persists after zoom/pan operations
  - [ ] Multiple plots can have different custom colors

- [ ] **Y-Axis Mode:**
  - [ ] Setting mode to "Manual" disables auto-scaling for that Y-axis
  - [ ] Manual min/max values are respected when mode is "Manual"
  - [ ] Switching back to "Auto" restores automatic scaling

### Crosshair Manager Tests

- [ ] **Crosshair Color:**
  - [ ] Changing `plots/crosshairColor` updates crosshair line color
  - [ ] Both horizontal and vertical lines update
  - [ ] Change applies immediately while hovering

- [ ] **Crosshair Thickness:**
  - [ ] Changing `plots/crosshairThickness` updates line width
  - [ ] Minimum thickness (0.5) is visible
  - [ ] Maximum thickness (5.0) renders correctly

### Legend Widget Tests

- [ ] **Text Size:**
  - [ ] Changing `legend/textSize` updates table font
  - [ ] Header labels update with new font size
  - [ ] All rows display with consistent new size

### Map Tests

- [ ] **Track Line Thickness:**
  - [ ] Changing `map/lineThickness` updates track polyline width
  - [ ] Change applies without reloading map
  - [ ] All visible tracks update together

- [ ] **Track Opacity:**
  - [ ] Changing `map/trackOpacity` updates track transparency
  - [ ] 0% opacity makes tracks invisible
  - [ ] 100% opacity makes tracks fully opaque
  - [ ] New tracks created with current opacity setting

- [ ] **Marker Size:**
  - [ ] Changing `map/markerSize` updates cursor dot diameter
  - [ ] Small sizes (4px) are visible
  - [ ] Large sizes (30px) render without overlap issues

### Integration Tests

- [ ] **Preference Persistence:**
  - [ ] Changes survive application restart
  - [ ] Values stored in Windows Registry at correct path

- [ ] **Signal Flow:**
  - [ ] PreferencesManager emits `preferenceChanged` signal
  - [ ] All connected components receive the signal
  - [ ] Components only respond to their relevant keys

- [ ] **Performance:**
  - [ ] Rapid preference changes don't cause UI freeze
  - [ ] No memory leaks with repeated changes
  - [ ] Replot operations are efficient

### Edge Cases

- [ ] **Missing Preferences:**
  - [ ] Components use sensible defaults if preference not found
  - [ ] No crashes when reading unregistered keys

- [ ] **Invalid Values:**
  - [ ] Zero or negative line thickness handled gracefully
  - [ ] Invalid colors fall back to defaults
  - [ ] Out-of-range opacity clamped to 0.0-1.0

- [ ] **Concurrent Changes:**
  - [ ] Multiple preferences changed rapidly don't cause race conditions
  - [ ] UI remains responsive during batch updates

---

## Implementation Notes

### Performance Considerations

1. **Batch Updates:** When multiple preferences change at once (e.g., "Reset to Defaults"), consider using a debounce mechanism to avoid multiple replot operations.

2. **Selective Updates:** The `onPreferenceChanged` slot should filter keys efficiently. Use `QString::startsWith()` for prefix matching rather than parsing every key.

3. **QML Binding Efficiency:** The QML property bindings in MapDock.qml automatically update only when the bound property changes. No manual refresh needed.

### Thread Safety

1. **Main Thread Only:** All preference access and UI updates must occur on the main thread.

2. **Signal/Slot Connections:** Qt's queued connections ensure thread safety when signals are emitted from worker threads (though PreferencesManager should only be used from the main thread).

### Debugging Tips

1. **Verify Signal Connections:** Add temporary debug output to confirm signals are received:
   ```cpp
   void Component::onPreferenceChanged(const QString &key, const QVariant &value)
   {
       qDebug() << "Preference changed:" << key << "=" << value;
       // ... rest of implementation
   }
   ```

2. **Check Registry Values:** Use Windows Registry Editor (regedit) to verify values at:
   ```
   HKEY_CURRENT_USER\Software\FlySight\Viewer 2
   ```

3. **QML Debugging:** Enable QML debugging in Qt Creator to inspect `mapPreferences` property values at runtime.

---

## Future Enhancements

After Phase 6 is complete, consider these improvements:

1. **Undo/Redo Support:** Track preference changes and allow reverting recent modifications.

2. **Import/Export Preferences:** Allow users to save and load preference profiles as files.

3. **Preview Mode:** Show live preview in the settings dialog before committing changes.

4. **Category-Specific Defaults:** Allow resetting only a specific category (plots, map, etc.) to defaults.

5. **Keyboard Shortcuts:** Add shortcuts for common preference adjustments (e.g., increase/decrease line thickness).
