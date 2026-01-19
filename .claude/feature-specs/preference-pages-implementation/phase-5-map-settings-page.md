# Phase 5: Map Settings Page Implementation

## Overview

This document provides a complete implementation guide for the Map Settings Page in FlySight Viewer 2. The Map Settings Page allows users to customize the appearance of tracks and cursor markers displayed on the QML-based map view.

### Current Hardcoded Values

The following values are currently hardcoded and will become configurable:

| Location | Value | Description |
|----------|-------|-------------|
| `MapDock.qml:300` | `3` | Track line width |
| `MapDock.qml:339-340` | `10x10` | Cursor dot size (pixels) |
| `MapDock.qml:343-344` | `2px white` | Cursor dot border |
| `trackmapmodel.cpp:78` | `0.85` | Track color alpha (opacity) |

### Preference Keys

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `map/lineThickness` | double | 3.0 | Track line width in pixels |
| `map/trackOpacity` | double | 0.85 | Track opacity (0.0-1.0) |
| `map/markerSize` | int | 10 | Cursor marker size in pixels |

---

## Header File

**File:** `src/preferences/mapsettingspage.h`

```cpp
#ifndef MAPSETTINGSPAGE_H
#define MAPSETTINGSPAGE_H

#include <QWidget>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

namespace FlySight {

class MapSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit MapSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveSettings();
    void onOpacitySliderChanged(int value);
    void resetToDefaults();

private:
    // Track Appearance controls
    QDoubleSpinBox *m_lineThicknessSpinBox;
    QSlider *m_trackOpacitySlider;
    QLabel *m_trackOpacityLabel;

    // Cursor Marker controls
    QSpinBox *m_markerSizeSpinBox;

    // Reset button
    QPushButton *m_resetButton;

    QGroupBox* createTrackAppearanceGroup();
    QGroupBox* createCursorMarkerGroup();
    QWidget* createResetSection();

    void loadSettings();
};

} // namespace FlySight

#endif // MAPSETTINGSPAGE_H
```

---

## Implementation File

**File:** `src/preferences/mapsettingspage.cpp`

```cpp
#include "mapsettingspage.h"
#include "preferencesmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>

namespace FlySight {

// Preference key constants
static const char* kLineThicknessKey = "map/lineThickness";
static const char* kTrackOpacityKey = "map/trackOpacity";
static const char* kMarkerSizeKey = "map/markerSize";

// Default values
static constexpr double kDefaultLineThickness = 3.0;
static constexpr double kDefaultTrackOpacity = 0.85;
static constexpr int kDefaultMarkerSize = 10;

MapSettingsPage::MapSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    // Register preferences with defaults
    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.registerPreference(kLineThicknessKey, kDefaultLineThickness);
    prefs.registerPreference(kTrackOpacityKey, kDefaultTrackOpacity);
    prefs.registerPreference(kMarkerSizeKey, kDefaultMarkerSize);

    // Build UI
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(createTrackAppearanceGroup());
    mainLayout->addWidget(createCursorMarkerGroup());
    mainLayout->addWidget(createResetSection());
    mainLayout->addStretch();

    // Load current values
    loadSettings();

    // Connect signals for immediate save on change
    connect(m_lineThicknessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MapSettingsPage::saveSettings);
    connect(m_trackOpacitySlider, &QSlider::valueChanged,
            this, &MapSettingsPage::onOpacitySliderChanged);
    connect(m_markerSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MapSettingsPage::saveSettings);
    connect(m_resetButton, &QPushButton::clicked,
            this, &MapSettingsPage::resetToDefaults);
}

QGroupBox* MapSettingsPage::createTrackAppearanceGroup()
{
    QGroupBox *group = new QGroupBox(tr("Track Appearance"), this);
    QFormLayout *layout = new QFormLayout(group);

    // Line thickness spin box
    m_lineThicknessSpinBox = new QDoubleSpinBox(this);
    m_lineThicknessSpinBox->setRange(1.0, 10.0);
    m_lineThicknessSpinBox->setSingleStep(0.5);
    m_lineThicknessSpinBox->setDecimals(1);
    m_lineThicknessSpinBox->setSuffix(tr(" px"));
    layout->addRow(tr("Line thickness:"), m_lineThicknessSpinBox);

    // Track opacity slider with label
    QWidget *opacityWidget = new QWidget(this);
    QHBoxLayout *opacityLayout = new QHBoxLayout(opacityWidget);
    opacityLayout->setContentsMargins(0, 0, 0, 0);

    m_trackOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_trackOpacitySlider->setRange(0, 100);
    m_trackOpacitySlider->setTickPosition(QSlider::TicksBelow);
    m_trackOpacitySlider->setTickInterval(10);

    m_trackOpacityLabel = new QLabel(this);
    m_trackOpacityLabel->setMinimumWidth(40);
    m_trackOpacityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    opacityLayout->addWidget(m_trackOpacitySlider, 1);
    opacityLayout->addWidget(m_trackOpacityLabel);

    layout->addRow(tr("Track opacity:"), opacityWidget);

    return group;
}

QGroupBox* MapSettingsPage::createCursorMarkerGroup()
{
    QGroupBox *group = new QGroupBox(tr("Cursor Marker"), this);
    QFormLayout *layout = new QFormLayout(group);

    // Marker size spin box
    m_markerSizeSpinBox = new QSpinBox(this);
    m_markerSizeSpinBox->setRange(4, 30);
    m_markerSizeSpinBox->setSuffix(tr(" px"));
    layout->addRow(tr("Marker size:"), m_markerSizeSpinBox);

    return group;
}

QWidget* MapSettingsPage::createResetSection()
{
    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 10, 0, 0);

    m_resetButton = new QPushButton(tr("Reset to Defaults"), this);
    layout->addStretch();
    layout->addWidget(m_resetButton);

    return widget;
}

void MapSettingsPage::loadSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // Block signals during load to prevent unnecessary saves
    m_lineThicknessSpinBox->blockSignals(true);
    m_trackOpacitySlider->blockSignals(true);
    m_markerSizeSpinBox->blockSignals(true);

    double lineThickness = prefs.getValue(kLineThicknessKey).toDouble();
    m_lineThicknessSpinBox->setValue(lineThickness);

    double opacity = prefs.getValue(kTrackOpacityKey).toDouble();
    int opacityPercent = qRound(opacity * 100.0);
    m_trackOpacitySlider->setValue(opacityPercent);
    m_trackOpacityLabel->setText(QString("%1%").arg(opacityPercent));

    int markerSize = prefs.getValue(kMarkerSizeKey).toInt();
    m_markerSizeSpinBox->setValue(markerSize);

    // Re-enable signals
    m_lineThicknessSpinBox->blockSignals(false);
    m_trackOpacitySlider->blockSignals(false);
    m_markerSizeSpinBox->blockSignals(false);
}

void MapSettingsPage::saveSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    prefs.setValue(kLineThicknessKey, m_lineThicknessSpinBox->value());
    prefs.setValue(kTrackOpacityKey, m_trackOpacitySlider->value() / 100.0);
    prefs.setValue(kMarkerSizeKey, m_markerSizeSpinBox->value());
}

void MapSettingsPage::onOpacitySliderChanged(int value)
{
    // Update the label in real-time
    m_trackOpacityLabel->setText(QString("%1%").arg(value));

    // Save settings
    saveSettings();
}

void MapSettingsPage::resetToDefaults()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // Set defaults in preferences
    prefs.setValue(kLineThicknessKey, kDefaultLineThickness);
    prefs.setValue(kTrackOpacityKey, kDefaultTrackOpacity);
    prefs.setValue(kMarkerSizeKey, kDefaultMarkerSize);

    // Reload UI from preferences
    loadSettings();
}

} // namespace FlySight
```

---

## CMakeLists.txt Changes

**File:** `src/CMakeLists.txt`

Add the new source files to the `PROJECT_SOURCES` list. Locate the existing preference files and add the new map settings page:

```cmake
# Find this section (around lines 188-191):
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp

# Add the new line after importsettingspage:
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  preferences/mapsettingspage.h     preferences/mapsettingspage.cpp
```

### Complete Diff

```diff
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -188,6 +188,7 @@ set(PROJECT_SOURCES
   preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
   preferences/generalsettingspage.h preferences/generalsettingspage.cpp
   preferences/importsettingspage.h  preferences/importsettingspage.cpp
+  preferences/mapsettingspage.h     preferences/mapsettingspage.cpp
   dependencykey.h
   crosshairmanager.h         crosshairmanager.cpp
   graphinfo.h
```

---

## PreferencesDialog Changes

**File:** `src/preferences/preferencesdialog.cpp`

### Include Header

Add the include at the top of the file:

```cpp
#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "mapsettingspage.h"       // ADD THIS LINE
#include <QDialogButtonBox>
#include <QHBoxLayout>
```

### Add Page to Dialog

Add the "Map" category and page in the constructor:

```cpp
PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Preferences");
    resize(400, 300);

    QHBoxLayout *mainLayout = new QHBoxLayout();
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);

    // Sidebar list
    categoryList = new QListWidget(this);
    categoryList->addItem("General");
    categoryList->addItem("Import");
    categoryList->addItem("Map");           // ADD THIS LINE
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
    stackedWidget->addWidget(new MapSettingsPage(this));    // ADD THIS LINE

    // ... rest of constructor unchanged
```

### Complete Diff

```diff
--- a/src/preferences/preferencesdialog.cpp
+++ b/src/preferences/preferencesdialog.cpp
@@ -1,6 +1,7 @@
 #include "preferencesdialog.h"
 #include "generalsettingspage.h"
 #include "importsettingspage.h"
+#include "mapsettingspage.h"
 #include <QDialogButtonBox>
 #include <QHBoxLayout>

@@ -17,12 +18,14 @@ PreferencesDialog::PreferencesDialog(QWidget *parent)
     categoryList = new QListWidget(this);
     categoryList->addItem("General");
     categoryList->addItem("Import");
+    categoryList->addItem("Map");
     categoryList->setFixedWidth(120);

     // Stacked widget for pages
     stackedWidget = new QStackedWidget(this);
     stackedWidget->addWidget(new GeneralSettingsPage(this));
     stackedWidget->addWidget(new ImportSettingsPage(this));
+    stackedWidget->addWidget(new MapSettingsPage(this));

     mainLayout->addWidget(categoryList);
     mainLayout->addWidget(stackedWidget);
```

---

## QML Integration (Future Work)

To complete the integration, the QML files need to read values from preferences. This requires exposing preference values to QML through a context property or singleton.

### Option 1: Context Property Approach

In the widget that creates the QML map view (likely `MapWidget`), expose preference values:

```cpp
// In MapWidget or similar class that sets up the QML context
QQmlContext *context = quickWidget->rootContext();

// Expose individual values
PreferencesManager &prefs = PreferencesManager::instance();
context->setContextProperty("mapLineThickness", prefs.getValue("map/lineThickness"));
context->setContextProperty("mapTrackOpacity", prefs.getValue("map/trackOpacity"));
context->setContextProperty("mapMarkerSize", prefs.getValue("map/markerSize"));
```

### Option 2: QML Singleton (Recommended)

Create a `MapPreferences` QObject that can be registered as a QML singleton:

```cpp
// mappreferences.h
class MapPreferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(double lineThickness READ lineThickness NOTIFY lineThicknessChanged)
    Q_PROPERTY(double trackOpacity READ trackOpacity NOTIFY trackOpacityChanged)
    Q_PROPERTY(int markerSize READ markerSize NOTIFY markerSizeChanged)

public:
    explicit MapPreferences(QObject *parent = nullptr);

    double lineThickness() const;
    double trackOpacity() const;
    int markerSize() const;

signals:
    void lineThicknessChanged();
    void trackOpacityChanged();
    void markerSizeChanged();

private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);
};
```

### MapDock.qml Changes (After QML Integration)

Once preferences are exposed to QML, update `MapDock.qml`:

```qml
// Line 300 - Track line width
delegate: MapPolyline {
    line.width: mapPreferences.lineThickness  // Was: 3
    // ...
}

// Lines 339-340 - Cursor dot size
sourceItem: Rectangle {
    id: dotItem
    width: mapPreferences.markerSize   // Was: 10
    height: mapPreferences.markerSize  // Was: 10
    // ...
}
```

### TrackMapModel Changes (After Integration)

Update `trackmapmodel.cpp` to read opacity from preferences:

```cpp
QColor TrackMapModel::colorForSession(const QString &sessionId)
{
    const uint h = qHash(sessionId);
    const int hue = static_cast<int>(h % 360);
    QColor c = QColor::fromHsv(hue, 200, 255);

    // Read opacity from preferences instead of hardcoded value
    double opacity = PreferencesManager::instance().getValue("map/trackOpacity").toDouble();
    c.setAlphaF(opacity);  // Was: c.setAlphaF(0.85);

    return c;
}
```

---

## Testing Checklist

### Build Verification

- [ ] Project compiles without errors after adding new files
- [ ] No new compiler warnings introduced
- [ ] Application starts successfully

### UI Verification

- [ ] Map Settings page appears in Preferences dialog sidebar
- [ ] Page displays correctly when selected
- [ ] All three control groups are visible:
  - [ ] Track Appearance group
  - [ ] Cursor Marker group
  - [ ] Reset to Defaults button

### Track Appearance Group

- [ ] Line thickness spin box:
  - [ ] Range is 1.0 to 10.0
  - [ ] Step increment is 0.5
  - [ ] Displays " px" suffix
  - [ ] Default value is 3.0
  - [ ] Value persists after closing and reopening preferences

- [ ] Track opacity slider:
  - [ ] Range is 0% to 100%
  - [ ] Label updates in real-time as slider moves
  - [ ] Default value is 85%
  - [ ] Value persists after closing and reopening preferences

### Cursor Marker Group

- [ ] Marker size spin box:
  - [ ] Range is 4 to 30
  - [ ] Displays " px" suffix
  - [ ] Default value is 10
  - [ ] Value persists after closing and reopening preferences

### Reset Functionality

- [ ] Reset button restores all values to defaults:
  - [ ] Line thickness returns to 3.0
  - [ ] Track opacity returns to 85%
  - [ ] Marker size returns to 10
- [ ] Reset persists values (saved to preferences)

### Preference Persistence

- [ ] Values are saved immediately on change (no need to click OK)
- [ ] Values persist across application restarts
- [ ] Canceling the dialog does NOT undo changes (immediate save behavior)

### Integration Testing (After QML Integration)

- [ ] Changing line thickness updates map track display
- [ ] Changing track opacity updates track transparency on map
- [ ] Changing marker size updates cursor dot size on map
- [ ] Changes are reflected immediately without map reload

### Edge Cases

- [ ] Setting opacity to 0% makes tracks fully transparent
- [ ] Setting opacity to 100% makes tracks fully opaque
- [ ] Minimum line thickness (1.0) is visible
- [ ] Maximum line thickness (10.0) renders correctly
- [ ] Minimum marker size (4px) is visible and clickable
- [ ] Maximum marker size (30px) renders without overlap issues

---

## File Summary

| File | Action | Description |
|------|--------|-------------|
| `src/preferences/mapsettingspage.h` | Create | Header file for MapSettingsPage class |
| `src/preferences/mapsettingspage.cpp` | Create | Implementation of MapSettingsPage |
| `src/CMakeLists.txt` | Modify | Add new source files to build |
| `src/preferences/preferencesdialog.cpp` | Modify | Include header and add page to dialog |

---

## Dependencies

This implementation depends on:

- `PreferencesManager` singleton (already exists)
- Qt Widgets module (already linked)
- Existing preferences dialog infrastructure

No new external dependencies are required.
