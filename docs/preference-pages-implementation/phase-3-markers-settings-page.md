# Phase 3: Markers Settings Page Implementation

## Overview

This document provides a detailed implementation plan for the Markers Settings Page in FlySight Viewer 2. This page allows users to customize marker colors for all registered markers in the application. The implementation follows the existing preference page pattern established by `GeneralSettingsPage` and `ImportSettingsPage`.

### Current Markers

The application currently has two built-in markers registered via `MarkerRegistry`:

| Category  | Label | Default Color      | Attribute Key |
|-----------|-------|-------------------|---------------|
| Reference | Exit  | RGB(0, 122, 204)  | ExitTime      |
| Reference | Start | RGB(0, 153, 51)   | StartTime     |

### Preference Keys

Marker colors are stored using the following key pattern:
- `markers/{attributeKey}/color`

For the current markers:
- `markers/ExitTime/color`
- `markers/StartTime/color`

---

## File Structure

```
src/preferences/
    markerssettingspage.h      (new)
    markerssettingspage.cpp    (new)
    preferencesdialog.cpp      (modified)
    preferencesdialog.h        (no changes needed)
```

---

## 1. Header File

**File:** `src/preferences/markerssettingspage.h`

```cpp
#ifndef MARKERSSETTINGSPAGE_H
#define MARKERSSETTINGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QMap>
#include <QColor>

namespace FlySight {

class MarkersSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit MarkersSettingsPage(QWidget *parent = nullptr);

private slots:
    void onColorButtonClicked();
    void resetAllToDefaults();

private:
    QTreeWidget *m_markerTree;
    QPushButton *m_resetButton;

    // Maps attributeKey -> color button for quick access
    QMap<QString, QPushButton*> m_colorButtons;

    // Maps attributeKey -> default color for reset functionality
    QMap<QString, QColor> m_defaultColors;

    QGroupBox* createMarkerColorsGroup();
    QWidget* createResetSection();

    void populateMarkerTree();
    void updateColorButtonStyle(QPushButton *button, const QColor &color);
    void saveMarkerColor(const QString &attributeKey, const QColor &color);
    QColor loadMarkerColor(const QString &attributeKey, const QColor &defaultColor);

    static QString markerColorKey(const QString &attributeKey);
};

} // namespace FlySight

#endif // MARKERSSETTINGSPAGE_H
```

---

## 2. Implementation File

**File:** `src/preferences/markerssettingspage.cpp`

```cpp
#include <QLabel>
#include <QHeaderView>
#include <QColorDialog>
#include <QHBoxLayout>
#include "markerssettingspage.h"
#include "preferencesmanager.h"
#include "../markerregistry.h"

namespace FlySight {

MarkersSettingsPage::MarkersSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createMarkerColorsGroup());
    layout->addWidget(createResetSection());
    layout->addStretch();
}

QGroupBox* MarkersSettingsPage::createMarkerColorsGroup()
{
    QGroupBox *group = new QGroupBox("Marker Colors", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);

    m_markerTree = new QTreeWidget(this);
    m_markerTree->setColumnCount(2);
    m_markerTree->setHeaderLabels(QStringList() << "Marker" << "Color");
    m_markerTree->header()->setStretchLastSection(false);
    m_markerTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_markerTree->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_markerTree->header()->resizeSection(1, 80);
    m_markerTree->setRootIsDecorated(true);
    m_markerTree->setSelectionMode(QAbstractItemView::NoSelection);
    m_markerTree->setFocusPolicy(Qt::NoFocus);

    populateMarkerTree();

    // Expand all categories by default
    m_markerTree->expandAll();

    groupLayout->addWidget(m_markerTree);

    return group;
}

QWidget* MarkersSettingsPage::createResetSection()
{
    QWidget *resetWidget = new QWidget(this);
    QHBoxLayout *resetLayout = new QHBoxLayout(resetWidget);
    resetLayout->setContentsMargins(0, 0, 0, 0);

    m_resetButton = new QPushButton("Reset All to Defaults", this);
    connect(m_resetButton, &QPushButton::clicked, this, &MarkersSettingsPage::resetAllToDefaults);

    resetLayout->addStretch();
    resetLayout->addWidget(m_resetButton);

    return resetWidget;
}

void MarkersSettingsPage::populateMarkerTree()
{
    m_markerTree->clear();
    m_colorButtons.clear();
    m_defaultColors.clear();

    // Get all markers from registry
    QVector<MarkerDefinition> markers = MarkerRegistry::instance().allMarkers();

    // Group markers by category
    QMap<QString, QVector<MarkerDefinition>> categorizedMarkers;
    for (const MarkerDefinition &marker : markers) {
        categorizedMarkers[marker.category].append(marker);
    }

    // Create tree structure
    for (auto it = categorizedMarkers.constBegin(); it != categorizedMarkers.constEnd(); ++it) {
        const QString &category = it.key();
        const QVector<MarkerDefinition> &categoryMarkers = it.value();

        // Create category item (top-level, non-editable, non-selectable)
        QTreeWidgetItem *categoryItem = new QTreeWidgetItem(m_markerTree);
        categoryItem->setText(0, category);
        categoryItem->setFlags(Qt::ItemIsEnabled); // Not selectable, not editable

        // Make category text bold
        QFont boldFont = categoryItem->font(0);
        boldFont.setBold(true);
        categoryItem->setFont(0, boldFont);

        // Add markers under this category
        for (const MarkerDefinition &marker : categoryMarkers) {
            QTreeWidgetItem *markerItem = new QTreeWidgetItem(categoryItem);
            markerItem->setText(0, marker.label);
            markerItem->setFlags(Qt::ItemIsEnabled); // Not selectable, not editable

            // Store the default color for reset functionality
            m_defaultColors[marker.attributeKey] = marker.color;

            // Load current color from preferences (or use default)
            QColor currentColor = loadMarkerColor(marker.attributeKey, marker.color);

            // Create color button
            QPushButton *colorButton = new QPushButton(this);
            colorButton->setFixedSize(60, 24);
            colorButton->setProperty("attributeKey", marker.attributeKey);
            updateColorButtonStyle(colorButton, currentColor);

            connect(colorButton, &QPushButton::clicked, this, &MarkersSettingsPage::onColorButtonClicked);

            // Store reference to color button
            m_colorButtons[marker.attributeKey] = colorButton;

            // Set the button as the item widget for column 1
            m_markerTree->setItemWidget(markerItem, 1, colorButton);
        }
    }
}

void MarkersSettingsPage::onColorButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    QString attributeKey = button->property("attributeKey").toString();
    if (attributeKey.isEmpty()) {
        return;
    }

    // Get current color from the button's stored property
    QColor currentColor = button->property("currentColor").value<QColor>();
    if (!currentColor.isValid()) {
        currentColor = m_defaultColors.value(attributeKey, Qt::white);
    }

    // Open color dialog
    QColor newColor = QColorDialog::getColor(currentColor, this, "Select Marker Color");

    if (newColor.isValid() && newColor != currentColor) {
        // Update button appearance
        updateColorButtonStyle(button, newColor);

        // Save immediately
        saveMarkerColor(attributeKey, newColor);
    }
}

void MarkersSettingsPage::resetAllToDefaults()
{
    // Reset all markers to their registered default colors
    for (auto it = m_defaultColors.constBegin(); it != m_defaultColors.constEnd(); ++it) {
        const QString &attributeKey = it.key();
        const QColor &defaultColor = it.value();

        // Update the button appearance
        QPushButton *button = m_colorButtons.value(attributeKey);
        if (button) {
            updateColorButtonStyle(button, defaultColor);
        }

        // Save the default color
        saveMarkerColor(attributeKey, defaultColor);
    }
}

void MarkersSettingsPage::updateColorButtonStyle(QPushButton *button, const QColor &color)
{
    // Store current color as a property for later retrieval
    button->setProperty("currentColor", color);

    // Calculate contrasting text color (white or black)
    int brightness = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    QString textColor = (brightness > 128) ? "black" : "white";

    // Set button style to show the color
    QString styleSheet = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 1px solid #888;"
        "  border-radius: 3px;"
        "  color: %2;"
        "}"
        "QPushButton:hover {"
        "  border: 1px solid #555;"
        "}"
        "QPushButton:pressed {"
        "  border: 2px solid #333;"
        "}"
    ).arg(color.name(), textColor);

    button->setStyleSheet(styleSheet);
    button->setText(color.name().toUpper());
}

void MarkersSettingsPage::saveMarkerColor(const QString &attributeKey, const QColor &color)
{
    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.setValue(markerColorKey(attributeKey), color);
}

QColor MarkersSettingsPage::loadMarkerColor(const QString &attributeKey, const QColor &defaultColor)
{
    PreferencesManager &prefs = PreferencesManager::instance();
    QString key = markerColorKey(attributeKey);

    // Register preference if not already registered
    if (!prefs.getValue(key).isValid()) {
        prefs.registerPreference(key, defaultColor);
    }

    return prefs.getValue(key).value<QColor>();
}

QString MarkersSettingsPage::markerColorKey(const QString &attributeKey)
{
    return QString("markers/%1/color").arg(attributeKey);
}

} // namespace FlySight
```

---

## 3. CMakeLists.txt Changes

**File:** `src/CMakeLists.txt`

Add the new files to the `PROJECT_SOURCES` list. Locate the existing preferences files and add the new ones:

```cmake
# Find this section (around line 188-191):
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp

# Change to:
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  preferences/markerssettingspage.h preferences/markerssettingspage.cpp
```

### Full Diff

```diff
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@ -188,6 +188,7 @@ set(PROJECT_SOURCES
   preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
   preferences/generalsettingspage.h preferences/generalsettingspage.cpp
   preferences/importsettingspage.h  preferences/importsettingspage.cpp
+  preferences/markerssettingspage.h preferences/markerssettingspage.cpp
   dependencykey.h
   crosshairmanager.h         crosshairmanager.cpp
   graphinfo.h
```

---

## 4. PreferencesDialog Changes

**File:** `src/preferences/preferencesdialog.cpp`

### 4.1 Add Include

Add the include for the new settings page at the top of the file:

```cpp
#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "markerssettingspage.h"  // Add this line
#include <QDialogButtonBox>
#include <QHBoxLayout>
```

### 4.2 Add Page to Dialog

In the constructor, add the new page to both the category list and stacked widget:

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
    categoryList->addItem("Markers");  // Add this line
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
    stackedWidget->addWidget(new MarkersSettingsPage(this));  // Add this line

    // ... rest of constructor unchanged
}
```

### Full Modified preferencesdialog.cpp

```cpp
#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "markerssettingspage.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>

namespace FlySight {

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
    categoryList->addItem("Markers");
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
    stackedWidget->addWidget(new MarkersSettingsPage(this));

    mainLayout->addWidget(categoryList);
    mainLayout->addWidget(stackedWidget);

    // OK/Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);

    // Add layouts to the dialog
    dialogLayout->addLayout(mainLayout);
    dialogLayout->addWidget(buttonBox);

    // Select first item by default
    categoryList->setCurrentRow(0);

    // Connect category selection to page switching
    connect(categoryList, &QListWidget::currentRowChanged, stackedWidget, &QStackedWidget::setCurrentIndex);
}

} // namespace FlySight
```

---

## 5. Optional: Helper Function in preferencekeys.h

If Phase 1 established a `preferencekeys.h` file, the helper function should be added there. Otherwise, it is implemented as a static member function in `MarkersSettingsPage` as shown above.

If you want to create a shared header:

**File:** `src/preferences/preferencekeys.h`

```cpp
#ifndef PREFERENCEKEYS_H
#define PREFERENCEKEYS_H

#include <QString>

namespace FlySight {

// Marker preference keys
inline QString markerColorKey(const QString &attributeKey) {
    return QString("markers/%1/color").arg(attributeKey);
}

// General preference keys
namespace PreferenceKeys {
    const QString Units = "general/units";
    const QString LogbookFolder = "general/logbookFolder";
    const QString GroundReferenceMode = "import/groundReferenceMode";
    const QString FixedElevation = "import/fixedElevation";
}

} // namespace FlySight

#endif // PREFERENCEKEYS_H
```

---

## 6. Integration with MarkerRegistry

For the marker colors to be used throughout the application, components that display markers should read colors from `PreferencesManager` instead of directly from `MarkerDefinition::color`.

### Example Usage Pattern

```cpp
// In any component that needs marker colors:
#include "preferences/preferencesmanager.h"
#include "markerregistry.h"

QColor getMarkerColor(const QString &attributeKey) {
    QString key = QString("markers/%1/color").arg(attributeKey);
    PreferencesManager &prefs = PreferencesManager::instance();

    // Get default from registry
    QColor defaultColor = Qt::gray;
    for (const auto &marker : MarkerRegistry::instance().allMarkers()) {
        if (marker.attributeKey == attributeKey) {
            defaultColor = marker.color;
            break;
        }
    }

    return prefs.getValue(key).value<QColor>();
}
```

### Listening for Color Changes

Components can connect to `PreferencesManager::preferenceChanged` to update when colors change:

```cpp
connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
        this, [this](const QString &key, const QVariant &value) {
    if (key.startsWith("markers/") && key.endsWith("/color")) {
        // Extract attributeKey and update display
        updateMarkerColors();
    }
});
```

---

## 7. UI Layout Diagram

```
+------------------------------------------+
| Marker Colors                            |
| +--------------------------------------+ |
| | Marker                    | Color    | |
| +--------------------------------------+ |
| | v Reference                          | |
| |   Exit                    | [#007ACC]| |
| |   Start                   | [#009933]| |
| +--------------------------------------+ |
+------------------------------------------+

                        [Reset All to Defaults]
```

---

## 8. Testing Checklist

### 8.1 Build Verification

- [ ] Project compiles without errors after adding new files
- [ ] No new compiler warnings introduced
- [ ] MOC (Meta-Object Compiler) processes the header correctly

### 8.2 UI Tests

- [ ] "Markers" appears in the preferences dialog sidebar
- [ ] Clicking "Markers" shows the Markers Settings Page
- [ ] Tree widget displays categories as top-level items
- [ ] Tree widget displays markers as child items under categories
- [ ] Category items are not selectable
- [ ] Marker items are not selectable
- [ ] Color buttons display current color as background
- [ ] Color buttons display hex color code as text
- [ ] Text on color buttons has appropriate contrast (white on dark, black on light)

### 8.3 Color Selection Tests

- [ ] Clicking a color button opens QColorDialog
- [ ] QColorDialog shows the current marker color as initial selection
- [ ] Selecting a new color updates the button immediately
- [ ] Canceling the color dialog does not change the color
- [ ] Color change is saved to preferences immediately

### 8.4 Reset Functionality Tests

- [ ] "Reset All to Defaults" button is visible
- [ ] Clicking reset restores all markers to their registered default colors
- [ ] Reset updates all button appearances immediately
- [ ] Reset saves default colors to preferences

### 8.5 Persistence Tests

- [ ] Close and reopen preferences dialog - colors persist
- [ ] Restart application - colors persist
- [ ] Change a color, restart app - changed color is loaded

### 8.6 Integration Tests

- [ ] Components using marker colors update when preferences change
- [ ] `preferenceChanged` signal emits with correct key and value
- [ ] Multiple color changes in sequence work correctly

### 8.7 Edge Cases

- [ ] Page works correctly with no markers registered
- [ ] Page works correctly with markers in multiple categories
- [ ] Page works correctly with single marker
- [ ] Very long marker labels display correctly (truncation or scroll)
- [ ] Color button remains functional after multiple rapid clicks

---

## 9. Future Enhancements

### 9.1 Additional Marker Settings

Future phases could extend this page to include:

- Marker visibility toggles
- Marker line style (solid, dashed, dotted)
- Marker line width
- Marker shape/icon selection

### 9.2 Import/Export

- Export marker color scheme to file
- Import marker color scheme from file
- Predefined color themes (e.g., "High Contrast", "Color Blind Friendly")

### 9.3 Per-Session Overrides

- Allow per-session marker color customization
- "Use Global" checkbox to inherit from preferences

---

## 10. Dependencies

### Required Qt Modules

- `Qt::Widgets` - For QWidget, QTreeWidget, QPushButton, QGroupBox, etc.
- `Qt::Core` - For QString, QMap, QColor, QVariant

### Internal Dependencies

- `PreferencesManager` - For reading/writing preferences
- `MarkerRegistry` - For getting all registered markers

### No External Dependencies

This implementation uses only Qt and existing FlySight Viewer components.

---

## 11. Summary

This implementation provides a clean, user-friendly interface for customizing marker colors in FlySight Viewer 2. Key features include:

1. **Organized Display**: Markers grouped by category in a tree structure
2. **Visual Color Buttons**: Buttons show actual color with hex code overlay
3. **Immediate Save**: Changes are persisted immediately without requiring explicit save
4. **Reset Capability**: One-click reset to registered default colors
5. **Consistent Pattern**: Follows existing preferences page patterns in the codebase
6. **Signal Integration**: Emits `preferenceChanged` for components to react to updates

The implementation is self-contained and requires minimal changes to existing code (only `CMakeLists.txt` and `preferencesdialog.cpp` need modification).
