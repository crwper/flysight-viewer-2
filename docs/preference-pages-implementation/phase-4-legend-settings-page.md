# Phase 4: Legend Settings Page Implementation

## Overview

This document provides a complete implementation guide for the Legend Settings Page in FlySight Viewer 2. The Legend Settings Page allows users to customize the appearance of the LegendWidget, which displays data values during flight analysis.

### Current State

The `LegendWidget` (`src/legendwidget.cpp`) currently has hardcoded styling values:
- Layout margins: 4px all sides (line 22)
- Layout spacing: 6px (line 23)
- Header spacing: 2px (line 29)
- Session label font: Italic (line 34)
- Table grid: disabled (line 57)

### Goals

1. Create a new settings page for legend customization
2. Start with a simple text size setting (expandable for future settings)
3. Follow the established preference page pattern used by `GeneralSettingsPage` and `ImportSettingsPage`
4. Enable immediate persistence of settings changes

### Files to Create/Modify

| File | Action |
|------|--------|
| `src/preferences/legendsettingspage.h` | Create |
| `src/preferences/legendsettingspage.cpp` | Create |
| `src/preferences/preferencesdialog.cpp` | Modify |
| `src/CMakeLists.txt` | Modify |

---

## 1. Header File

Create `src/preferences/legendsettingspage.h`:

```cpp
#ifndef LEGENDSETTINGSPAGE_H
#define LEGENDSETTINGSPAGE_H

#include <QWidget>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace FlySight {

class LegendSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit LegendSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveSettings();
    void resetToDefaults();

private:
    QSpinBox *m_textSizeSpinBox;
    QPushButton *m_resetButton;

    QGroupBox* createTextSettingsGroup();
    QWidget* createResetSection();

    void loadSettings();
};

} // namespace FlySight

#endif // LEGENDSETTINGSPAGE_H
```

---

## 2. Implementation File

Create `src/preferences/legendsettingspage.cpp`:

```cpp
#include "legendsettingspage.h"
#include "preferencesmanager.h"

#include <QLabel>
#include <QHBoxLayout>

namespace FlySight {

// Preference key constants
static const QString KEY_TEXT_SIZE = QStringLiteral("legend/textSize");

// Default values
static const int DEFAULT_TEXT_SIZE = 9;

LegendSettingsPage::LegendSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    // Register preferences with defaults
    PreferencesManager::instance().registerPreference(KEY_TEXT_SIZE, DEFAULT_TEXT_SIZE);

    // Build UI
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createTextSettingsGroup());
    layout->addStretch();
    layout->addWidget(createResetSection());

    // Load current settings
    loadSettings();

    // Connect signals for immediate save
    connect(m_textSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LegendSettingsPage::saveSettings);

    // Connect reset button
    connect(m_resetButton, &QPushButton::clicked,
            this, &LegendSettingsPage::resetToDefaults);
}

QGroupBox* LegendSettingsPage::createTextSettingsGroup()
{
    QGroupBox *group = new QGroupBox(tr("Legend Text"), this);
    QHBoxLayout *groupLayout = new QHBoxLayout(group);

    QLabel *label = new QLabel(tr("Text size:"), this);

    m_textSizeSpinBox = new QSpinBox(this);
    m_textSizeSpinBox->setRange(6, 24);
    m_textSizeSpinBox->setSuffix(tr(" pt"));
    m_textSizeSpinBox->setToolTip(tr("Font size for legend text (6-24 points)"));

    groupLayout->addWidget(label);
    groupLayout->addWidget(m_textSizeSpinBox);
    groupLayout->addStretch();

    return group;
}

QWidget* LegendSettingsPage::createResetSection()
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    m_resetButton = new QPushButton(tr("Reset to Defaults"), this);
    m_resetButton->setToolTip(tr("Restore all legend settings to their default values"));

    layout->addStretch();
    layout->addWidget(m_resetButton);

    return container;
}

void LegendSettingsPage::loadSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // Block signals while loading to prevent triggering saveSettings
    m_textSizeSpinBox->blockSignals(true);
    m_textSizeSpinBox->setValue(prefs.getValue(KEY_TEXT_SIZE).toInt());
    m_textSizeSpinBox->blockSignals(false);
}

void LegendSettingsPage::saveSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.setValue(KEY_TEXT_SIZE, m_textSizeSpinBox->value());
}

void LegendSettingsPage::resetToDefaults()
{
    // Reset the spin box to default value (this will trigger saveSettings)
    m_textSizeSpinBox->setValue(DEFAULT_TEXT_SIZE);
}

} // namespace FlySight
```

---

## 3. CMakeLists.txt Changes

In `src/CMakeLists.txt`, add the new files to the `PROJECT_SOURCES` list. Locate the existing preference page entries and add the legend settings page:

```cmake
set(PROJECT_SOURCES
  # ... existing sources ...
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  preferences/legendsettingspage.h  preferences/legendsettingspage.cpp   # ADD THIS LINE
  # ... remaining sources ...
)
```

**Specific change:**

Find this section (around lines 188-191):
```cmake
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
```

Change it to:
```cmake
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  preferences/legendsettingspage.h  preferences/legendsettingspage.cpp
```

---

## 4. PreferencesDialog Changes

Modify `src/preferences/preferencesdialog.cpp` to include the new settings page.

### 4.1 Add Include

Add the include statement at the top of the file:

```cpp
#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "legendsettingspage.h"    // ADD THIS LINE
#include <QDialogButtonBox>
#include <QHBoxLayout>
```

### 4.2 Add Page to Dialog

In the constructor, add the new page to both the category list and stacked widget.

Find this section:
```cpp
    // Sidebar list
    categoryList = new QListWidget(this);
    categoryList->addItem("General");
    categoryList->addItem("Import");
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
```

Change it to:
```cpp
    // Sidebar list
    categoryList = new QListWidget(this);
    categoryList->addItem("General");
    categoryList->addItem("Import");
    categoryList->addItem("Legend");    // ADD THIS LINE
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
    stackedWidget->addWidget(new LegendSettingsPage(this));    // ADD THIS LINE
```

### 4.3 Complete Modified preferencesdialog.cpp

For reference, here is the complete modified file:

```cpp
#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "legendsettingspage.h"
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
    categoryList->addItem("Legend");
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
    stackedWidget->addWidget(new LegendSettingsPage(this));

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

## 5. Applying Settings to LegendWidget

To make the settings actually affect the LegendWidget, you need to modify `src/legendwidget.cpp` to read from preferences. This is a separate step that can be done after the settings page is working.

### 5.1 Register Preference on Application Startup

In `main.cpp` or an initialization function, register the preference:

```cpp
#include "preferences/preferencesmanager.h"

// In main() or application init:
FlySight::PreferencesManager::instance().registerPreference("legend/textSize", 9);
```

**Note:** The `LegendSettingsPage` constructor already registers the preference, but it's good practice to ensure preferences are registered early in the application lifecycle.

### 5.2 Modify LegendWidget to Use Preferences

Add to `legendwidget.cpp`:

```cpp
#include "preferences/preferencesmanager.h"

// In LegendWidget constructor, after creating m_table:
void LegendWidget::applyFontSettings()
{
    int textSize = PreferencesManager::instance().getValue("legend/textSize").toInt();

    QFont font = this->font();
    font.setPointSize(textSize);

    // Apply to table
    m_table->setFont(font);

    // Apply to header labels
    m_sessionLabel->setFont(font);
    m_utcLabel->setFont(font);
    m_coordsLabel->setFont(font);

    // Restore italic for session label
    QFont sessionFont = m_sessionLabel->font();
    sessionFont.setItalic(true);
    m_sessionLabel->setFont(sessionFont);
}
```

### 5.3 Connect to Preference Changes

In `LegendWidget` constructor:

```cpp
// Apply initial settings
applyFontSettings();

// Listen for preference changes
connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
        this, [this](const QString &key, const QVariant &) {
    if (key == "legend/textSize") {
        applyFontSettings();
    }
});
```

---

## 6. Testing Checklist

### 6.1 Build Verification

- [ ] Project compiles without errors after adding new files
- [ ] No linker errors related to new classes
- [ ] MOC properly processes the Q_OBJECT macro

### 6.2 UI Verification

- [ ] "Legend" category appears in Preferences dialog sidebar
- [ ] Clicking "Legend" switches to the Legend Settings Page
- [ ] "Legend Text" group box is displayed
- [ ] Text size spin box shows correct label ("Text size:")
- [ ] Spin box shows " pt" suffix
- [ ] Spin box minimum value is 6
- [ ] Spin box maximum value is 24
- [ ] "Reset to Defaults" button is visible at the bottom

### 6.3 Persistence Verification

- [ ] Default value (9) is shown on first launch
- [ ] Changed value persists after closing and reopening dialog
- [ ] Changed value persists after restarting application
- [ ] Settings stored in correct location (QSettings)

### 6.4 Functionality Verification

- [ ] Changing spin box value immediately saves to preferences
- [ ] "Reset to Defaults" button resets text size to 9
- [ ] Reset is immediately saved to preferences
- [ ] No crash when rapidly changing values

### 6.5 Integration Verification (After LegendWidget Integration)

- [ ] Changing text size in preferences updates LegendWidget font
- [ ] Font change is visible immediately (no restart required)
- [ ] Session label remains italic after font size change
- [ ] All legend elements (table, headers) use new font size

### 6.6 Edge Cases

- [ ] Verify behavior at minimum value (6)
- [ ] Verify behavior at maximum value (24)
- [ ] Verify keyboard input in spin box works correctly
- [ ] Verify spin box arrow buttons work correctly

---

## 7. Future Expansion

The Legend Settings Page is designed to accommodate additional settings. Potential future additions include:

| Setting | Key | Type | Default |
|---------|-----|------|---------|
| Layout margins | `legend/margins` | int | 4 |
| Layout spacing | `legend/spacing` | int | 6 |
| Header spacing | `legend/headerSpacing` | int | 2 |
| Show grid | `legend/showGrid` | bool | false |
| Session label italic | `legend/sessionItalic` | bool | true |

To add a new setting:

1. Add member widget in header file
2. Create/modify group box in implementation
3. Register preference with default in constructor
4. Load value in `loadSettings()`
5. Connect widget signal to `saveSettings()`
6. Reset value in `resetToDefaults()`
7. Update LegendWidget to read and apply the setting

---

## 8. Preference Key Reference

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `legend/textSize` | int | 9 | Font size in points for legend text |

---

## 9. Summary

This implementation adds a Legend Settings Page to the FlySight Viewer 2 preferences dialog. The page follows the established pattern of other settings pages in the application:

1. **Constructor**: Registers preferences, builds UI, loads settings, connects signals
2. **Group creation methods**: Encapsulate UI component creation
3. **saveSettings()**: Immediately persists changes to PreferencesManager
4. **loadSettings()**: Reads current values from PreferencesManager
5. **resetToDefaults()**: Restores all settings to their registered defaults

The implementation is minimal but extensible, starting with a single text size setting that can be expanded to include additional legend customization options in the future.
