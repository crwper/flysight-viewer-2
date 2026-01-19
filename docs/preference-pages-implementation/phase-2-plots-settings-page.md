# Phase 2: Plots Settings Page Implementation

## Overview

This document provides a complete implementation guide for the Plots Settings Page in FlySight Viewer 2. The Plots Settings Page allows users to configure global plot appearance settings (line thickness, text size, crosshair appearance, Y-axis padding) and per-plot settings (custom colors, Y-axis modes, and manual Y-axis ranges).

The implementation follows the existing settings page pattern established by `GeneralSettingsPage` and `ImportSettingsPage`, using:
- `QWidget` as the base class
- `QGroupBox` for organizing related settings
- Immediate save on value change via `PreferencesManager`
- `QTreeWidget` for hierarchical per-plot settings organized by category

### Files to Create
- `src/preferences/plotssettingspage.h`
- `src/preferences/plotssettingspage.cpp`

### Files to Modify
- `src/CMakeLists.txt` - Add new source files
- `src/preferences/preferencesdialog.cpp` - Register the new page
- `src/preferences/preferencesmanager.h` - Add `getDefaultValue()` method (if not already present)

---

## Header File

**File: `src/preferences/plotssettingspage.h`**

```cpp
#ifndef PLOTSSETTINGSPAGE_H
#define PLOTSSETTINGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QColor>
#include <QMap>

namespace FlySight {

class PlotsSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit PlotsSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveGlobalSettings();
    void chooseCrosshairColor();
    void resetAllToDefaults();

private:
    // Global settings widgets
    QDoubleSpinBox *lineThicknessSpinBox;
    QSpinBox *textSizeSpinBox;
    QPushButton *crosshairColorButton;
    QDoubleSpinBox *crosshairThicknessSpinBox;
    QSpinBox *yAxisPaddingSpinBox;

    // Per-plot settings
    QTreeWidget *plotsTreeWidget;

    // Track color buttons for per-plot settings
    // Key: "sensorID/measurementID"
    QMap<QString, QPushButton*> plotColorButtons;
    QMap<QString, QComboBox*> plotYAxisModeComboBoxes;
    QMap<QString, QDoubleSpinBox*> plotYAxisMinSpinBoxes;
    QMap<QString, QDoubleSpinBox*> plotYAxisMaxSpinBoxes;

    // Current crosshair color (stored separately for button display)
    QColor currentCrosshairColor;

    // UI creation helpers
    QGroupBox* createGlobalSettingsGroup();
    QGroupBox* createPerPlotSettingsGroup();
    QWidget* createResetSection();

    // Helper methods
    void updateColorButtonStyle(QPushButton *button, const QColor &color);
    void loadGlobalSettings();
    void loadPerPlotSettings();
    QString getPlotKey(const QString &sensorID, const QString &measurementID) const;

    // Per-plot setting handlers
    void onPlotColorButtonClicked();
    void onYAxisModeChanged(const QString &plotKey, int index);
    void onYAxisMinChanged(const QString &plotKey, double value);
    void onYAxisMaxChanged(const QString &plotKey, double value);
    void updateYAxisWidgetsEnabled(const QString &plotKey, bool manual);
};

} // namespace FlySight

#endif // PLOTSSETTINGSPAGE_H
```

---

## Implementation File

**File: `src/preferences/plotssettingspage.cpp`**

```cpp
#include <QLabel>
#include <QFormLayout>
#include <QColorDialog>
#include <QHeaderView>
#include <QMessageBox>
#include "plotssettingspage.h"
#include "preferencesmanager.h"
#include "../plotregistry.h"

namespace FlySight {

PlotsSettingsPage::PlotsSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createGlobalSettingsGroup());
    layout->addWidget(createPerPlotSettingsGroup(), 1); // Give tree widget stretch
    layout->addWidget(createResetSection());

    // Load current settings
    loadGlobalSettings();
    loadPerPlotSettings();

    // Connect global settings signals
    connect(lineThicknessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlotsSettingsPage::saveGlobalSettings);
    connect(textSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlotsSettingsPage::saveGlobalSettings);
    connect(crosshairColorButton, &QPushButton::clicked,
            this, &PlotsSettingsPage::chooseCrosshairColor);
    connect(crosshairThicknessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PlotsSettingsPage::saveGlobalSettings);
    connect(yAxisPaddingSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlotsSettingsPage::saveGlobalSettings);
}

QGroupBox* PlotsSettingsPage::createGlobalSettingsGroup()
{
    QGroupBox *group = new QGroupBox(tr("Plot Appearance"), this);
    QFormLayout *formLayout = new QFormLayout(group);

    // Line thickness
    lineThicknessSpinBox = new QDoubleSpinBox(this);
    lineThicknessSpinBox->setRange(0.5, 10.0);
    lineThicknessSpinBox->setSingleStep(0.5);
    lineThicknessSpinBox->setSuffix(tr(" px"));
    lineThicknessSpinBox->setDecimals(1);
    formLayout->addRow(tr("Line thickness:"), lineThicknessSpinBox);

    // Plot text size
    textSizeSpinBox = new QSpinBox(this);
    textSizeSpinBox->setRange(6, 24);
    textSizeSpinBox->setSuffix(tr(" pt"));
    formLayout->addRow(tr("Plot text size:"), textSizeSpinBox);

    // Crosshair color
    crosshairColorButton = new QPushButton(this);
    crosshairColorButton->setFixedSize(80, 24);
    crosshairColorButton->setCursor(Qt::PointingHandCursor);
    formLayout->addRow(tr("Crosshair color:"), crosshairColorButton);

    // Crosshair thickness
    crosshairThicknessSpinBox = new QDoubleSpinBox(this);
    crosshairThicknessSpinBox->setRange(0.5, 5.0);
    crosshairThicknessSpinBox->setSingleStep(0.5);
    crosshairThicknessSpinBox->setSuffix(tr(" px"));
    crosshairThicknessSpinBox->setDecimals(1);
    formLayout->addRow(tr("Crosshair thickness:"), crosshairThicknessSpinBox);

    // Y-axis padding (displayed as percentage, stored as decimal)
    yAxisPaddingSpinBox = new QSpinBox(this);
    yAxisPaddingSpinBox->setRange(1, 50);
    yAxisPaddingSpinBox->setSuffix(tr("%"));
    formLayout->addRow(tr("Y-axis padding:"), yAxisPaddingSpinBox);

    return group;
}

QGroupBox* PlotsSettingsPage::createPerPlotSettingsGroup()
{
    QGroupBox *group = new QGroupBox(tr("Per-Plot Settings"), this);
    QVBoxLayout *layout = new QVBoxLayout(group);

    plotsTreeWidget = new QTreeWidget(this);
    plotsTreeWidget->setHeaderLabels({
        tr("Plot"),
        tr("Color"),
        tr("Y-Axis Mode"),
        tr("Y-Axis Min"),
        tr("Y-Axis Max")
    });
    plotsTreeWidget->setRootIsDecorated(true);
    plotsTreeWidget->setAlternatingRowColors(true);

    // Set column widths
    plotsTreeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    plotsTreeWidget->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    plotsTreeWidget->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    plotsTreeWidget->header()->setSectionResizeMode(3, QHeaderView::Fixed);
    plotsTreeWidget->header()->setSectionResizeMode(4, QHeaderView::Fixed);
    plotsTreeWidget->setColumnWidth(1, 80);
    plotsTreeWidget->setColumnWidth(2, 100);
    plotsTreeWidget->setColumnWidth(3, 100);
    plotsTreeWidget->setColumnWidth(4, 100);

    // Build tree from PlotRegistry
    const QVector<PlotValue> plots = PlotRegistry::instance().allPlots();

    // Group plots by category
    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const PlotValue &pv : plots) {
        // Get or create category item
        QTreeWidgetItem *categoryItem = categoryItems.value(pv.category, nullptr);
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(plotsTreeWidget);
            categoryItem->setText(0, pv.category);
            categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsSelectable);
            categoryItem->setExpanded(true);
            QFont boldFont = categoryItem->font(0);
            boldFont.setBold(true);
            categoryItem->setFont(0, boldFont);
            categoryItems[pv.category] = categoryItem;
        }

        // Create plot item
        QTreeWidgetItem *plotItem = new QTreeWidgetItem(categoryItem);
        QString plotKey = getPlotKey(pv.sensorID, pv.measurementID);

        // Column 0: Plot name (read-only, just text)
        QString displayName = pv.plotName;
        if (!pv.plotUnits.isEmpty()) {
            displayName += QString(" (%1)").arg(pv.plotUnits);
        }
        plotItem->setText(0, displayName);
        plotItem->setData(0, Qt::UserRole, plotKey); // Store key for later lookup

        // Column 1: Color button
        QPushButton *colorButton = new QPushButton(this);
        colorButton->setFixedSize(60, 20);
        colorButton->setCursor(Qt::PointingHandCursor);
        colorButton->setProperty("plotKey", plotKey);
        plotsTreeWidget->setItemWidget(plotItem, 1, colorButton);
        plotColorButtons[plotKey] = colorButton;
        connect(colorButton, &QPushButton::clicked,
                this, &PlotsSettingsPage::onPlotColorButtonClicked);

        // Column 2: Y-axis mode combo
        QComboBox *modeCombo = new QComboBox(this);
        modeCombo->addItem(tr("Auto"));
        modeCombo->addItem(tr("Manual"));
        modeCombo->setProperty("plotKey", plotKey);
        plotsTreeWidget->setItemWidget(plotItem, 2, modeCombo);
        plotYAxisModeComboBoxes[plotKey] = modeCombo;
        connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, plotKey](int index) {
                    onYAxisModeChanged(plotKey, index);
                });

        // Column 3: Y-axis min
        QDoubleSpinBox *minSpinBox = new QDoubleSpinBox(this);
        minSpinBox->setRange(-1e9, 1e9);
        minSpinBox->setDecimals(2);
        minSpinBox->setProperty("plotKey", plotKey);
        plotsTreeWidget->setItemWidget(plotItem, 3, minSpinBox);
        plotYAxisMinSpinBoxes[plotKey] = minSpinBox;
        connect(minSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, plotKey](double value) {
                    onYAxisMinChanged(plotKey, value);
                });

        // Column 4: Y-axis max
        QDoubleSpinBox *maxSpinBox = new QDoubleSpinBox(this);
        maxSpinBox->setRange(-1e9, 1e9);
        maxSpinBox->setDecimals(2);
        maxSpinBox->setProperty("plotKey", plotKey);
        plotsTreeWidget->setItemWidget(plotItem, 4, maxSpinBox);
        plotYAxisMaxSpinBoxes[plotKey] = maxSpinBox;
        connect(maxSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this, plotKey](double value) {
                    onYAxisMaxChanged(plotKey, value);
                });
    }

    layout->addWidget(plotsTreeWidget);
    return group;
}

QWidget* PlotsSettingsPage::createResetSection()
{
    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 10, 0, 0);

    layout->addStretch();

    QPushButton *resetButton = new QPushButton(tr("Reset All to Defaults"), this);
    connect(resetButton, &QPushButton::clicked, this, &PlotsSettingsPage::resetAllToDefaults);
    layout->addWidget(resetButton);

    return container;
}

void PlotsSettingsPage::updateColorButtonStyle(QPushButton *button, const QColor &color)
{
    QString styleSheet = QString(
        "QPushButton {"
        "  background-color: %1;"
        "  border: 1px solid #888888;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "  border: 1px solid #555555;"
        "}"
    ).arg(color.name());
    button->setStyleSheet(styleSheet);
    button->setToolTip(color.name());
}

void PlotsSettingsPage::loadGlobalSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    lineThicknessSpinBox->setValue(prefs.getValue("plots/lineThickness").toDouble());
    textSizeSpinBox->setValue(prefs.getValue("plots/textSize").toInt());

    currentCrosshairColor = QColor(prefs.getValue("plots/crosshairColor").toString());
    updateColorButtonStyle(crosshairColorButton, currentCrosshairColor);

    crosshairThicknessSpinBox->setValue(prefs.getValue("plots/crosshairThickness").toDouble());

    // Y-axis padding: stored as decimal (0.01-0.50), displayed as percentage (1-50)
    double paddingDecimal = prefs.getValue("plots/yAxisPadding").toDouble();
    yAxisPaddingSpinBox->setValue(static_cast<int>(paddingDecimal * 100));
}

void PlotsSettingsPage::loadPerPlotSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();
    const QVector<PlotValue> plots = PlotRegistry::instance().allPlots();

    for (const PlotValue &pv : plots) {
        QString plotKey = getPlotKey(pv.sensorID, pv.measurementID);
        QString prefKeyBase = QString("plots/%1/%2").arg(pv.sensorID, pv.measurementID);

        // Color
        if (plotColorButtons.contains(plotKey)) {
            QColor color(prefs.getValue(prefKeyBase + "/color").toString());
            if (!color.isValid()) {
                color = pv.defaultColor;
            }
            updateColorButtonStyle(plotColorButtons[plotKey], color);
        }

        // Y-axis mode
        if (plotYAxisModeComboBoxes.contains(plotKey)) {
            QString mode = prefs.getValue(prefKeyBase + "/yAxisMode").toString();
            int index = (mode == "Manual") ? 1 : 0;
            plotYAxisModeComboBoxes[plotKey]->setCurrentIndex(index);
            updateYAxisWidgetsEnabled(plotKey, index == 1);
        }

        // Y-axis min/max
        if (plotYAxisMinSpinBoxes.contains(plotKey)) {
            double minVal = prefs.getValue(prefKeyBase + "/yAxisMin").toDouble();
            plotYAxisMinSpinBoxes[plotKey]->setValue(minVal);
        }
        if (plotYAxisMaxSpinBoxes.contains(plotKey)) {
            double maxVal = prefs.getValue(prefKeyBase + "/yAxisMax").toDouble();
            plotYAxisMaxSpinBoxes[plotKey]->setValue(maxVal);
        }
    }
}

QString PlotsSettingsPage::getPlotKey(const QString &sensorID, const QString &measurementID) const
{
    return QString("%1/%2").arg(sensorID, measurementID);
}

void PlotsSettingsPage::saveGlobalSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    prefs.setValue("plots/lineThickness", lineThicknessSpinBox->value());
    prefs.setValue("plots/textSize", textSizeSpinBox->value());
    prefs.setValue("plots/crosshairColor", currentCrosshairColor.name());
    prefs.setValue("plots/crosshairThickness", crosshairThicknessSpinBox->value());

    // Convert percentage to decimal for storage
    double paddingDecimal = yAxisPaddingSpinBox->value() / 100.0;
    prefs.setValue("plots/yAxisPadding", paddingDecimal);
}

void PlotsSettingsPage::chooseCrosshairColor()
{
    QColor color = QColorDialog::getColor(currentCrosshairColor, this, tr("Choose Crosshair Color"));
    if (color.isValid()) {
        currentCrosshairColor = color;
        updateColorButtonStyle(crosshairColorButton, currentCrosshairColor);
        saveGlobalSettings();
    }
}

void PlotsSettingsPage::onPlotColorButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) return;

    QString plotKey = button->property("plotKey").toString();
    QStringList parts = plotKey.split('/');
    if (parts.size() != 2) return;

    QString sensorID = parts[0];
    QString measurementID = parts[1];
    QString prefKey = QString("plots/%1/%2/color").arg(sensorID, measurementID);

    PreferencesManager &prefs = PreferencesManager::instance();
    QColor currentColor(prefs.getValue(prefKey).toString());

    // Find default color from registry for dialog title
    QString plotName = plotKey;
    const QVector<PlotValue> plots = PlotRegistry::instance().allPlots();
    for (const PlotValue &pv : plots) {
        if (pv.sensorID == sensorID && pv.measurementID == measurementID) {
            plotName = pv.plotName;
            if (!currentColor.isValid()) {
                currentColor = pv.defaultColor;
            }
            break;
        }
    }

    QColor newColor = QColorDialog::getColor(currentColor, this,
                                              tr("Choose Color for %1").arg(plotName));
    if (newColor.isValid()) {
        prefs.setValue(prefKey, newColor.name());
        updateColorButtonStyle(button, newColor);
    }
}

void PlotsSettingsPage::onYAxisModeChanged(const QString &plotKey, int index)
{
    QStringList parts = plotKey.split('/');
    if (parts.size() != 2) return;

    QString sensorID = parts[0];
    QString measurementID = parts[1];
    QString prefKey = QString("plots/%1/%2/yAxisMode").arg(sensorID, measurementID);

    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.setValue(prefKey, (index == 1) ? "Manual" : "Auto");

    updateYAxisWidgetsEnabled(plotKey, index == 1);
}

void PlotsSettingsPage::onYAxisMinChanged(const QString &plotKey, double value)
{
    QStringList parts = plotKey.split('/');
    if (parts.size() != 2) return;

    QString sensorID = parts[0];
    QString measurementID = parts[1];
    QString prefKey = QString("plots/%1/%2/yAxisMin").arg(sensorID, measurementID);

    PreferencesManager::instance().setValue(prefKey, value);
}

void PlotsSettingsPage::onYAxisMaxChanged(const QString &plotKey, double value)
{
    QStringList parts = plotKey.split('/');
    if (parts.size() != 2) return;

    QString sensorID = parts[0];
    QString measurementID = parts[1];
    QString prefKey = QString("plots/%1/%2/yAxisMax").arg(sensorID, measurementID);

    PreferencesManager::instance().setValue(prefKey, value);
}

void PlotsSettingsPage::updateYAxisWidgetsEnabled(const QString &plotKey, bool manual)
{
    if (plotYAxisMinSpinBoxes.contains(plotKey)) {
        plotYAxisMinSpinBoxes[plotKey]->setEnabled(manual);
    }
    if (plotYAxisMaxSpinBoxes.contains(plotKey)) {
        plotYAxisMaxSpinBoxes[plotKey]->setEnabled(manual);
    }
}

void PlotsSettingsPage::resetAllToDefaults()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Reset to Defaults"),
        tr("Are you sure you want to reset all plot settings to their default values?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    PreferencesManager &prefs = PreferencesManager::instance();

    // Reset global settings to defaults
    prefs.setValue("plots/lineThickness", prefs.getDefaultValue("plots/lineThickness"));
    prefs.setValue("plots/textSize", prefs.getDefaultValue("plots/textSize"));
    prefs.setValue("plots/crosshairColor", prefs.getDefaultValue("plots/crosshairColor"));
    prefs.setValue("plots/crosshairThickness", prefs.getDefaultValue("plots/crosshairThickness"));
    prefs.setValue("plots/yAxisPadding", prefs.getDefaultValue("plots/yAxisPadding"));

    // Reset per-plot settings to defaults
    const QVector<PlotValue> plots = PlotRegistry::instance().allPlots();
    for (const PlotValue &pv : plots) {
        QString prefKeyBase = QString("plots/%1/%2").arg(pv.sensorID, pv.measurementID);

        prefs.setValue(prefKeyBase + "/color", pv.defaultColor.name());
        prefs.setValue(prefKeyBase + "/yAxisMode", "Auto");
        prefs.setValue(prefKeyBase + "/yAxisMin", 0.0);
        prefs.setValue(prefKeyBase + "/yAxisMax", 100.0);
    }

    // Reload UI
    loadGlobalSettings();
    loadPerPlotSettings();
}

} // namespace FlySight
```

---

## PreferencesManager Update

The `resetAllToDefaults()` function requires a `getDefaultValue()` method. If not already present in `PreferencesManager`, add it.

**Add to `src/preferences/preferencesmanager.h`** (inside the class, after `setValue()`):

```cpp
    QVariant getDefaultValue(const QString &key) const {
        if (m_preferences.contains(key)) {
            return m_preferences.value(key).defaultValue;
        }
        return QVariant();
    }
```

---

## CMakeLists.txt Changes

**File: `src/CMakeLists.txt`**

Add the new source files to the `PROJECT_SOURCES` list. Locate the existing preferences files:

```cmake
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
```

Add the new files after `importsettingspage`:

```cmake
  preferences/preferencesmanager.h
  preferences/preferencesdialog.h  preferences/preferencesdialog.cpp
  preferences/generalsettingspage.h preferences/generalsettingspage.cpp
  preferences/importsettingspage.h  preferences/importsettingspage.cpp
  preferences/plotssettingspage.h  preferences/plotssettingspage.cpp
```

---

## PreferencesDialog Changes

**File: `src/preferences/preferencesdialog.cpp`**

### Add Include

Add the include for the new page at the top of the file:

```cpp
#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "plotssettingspage.h"    // Add this line
#include <QDialogButtonBox>
#include <QHBoxLayout>
```

### Register the Page

In the constructor, add the new category and page. Find these lines:

```cpp
    categoryList->addItem("General");
    categoryList->addItem("Import");
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
```

Add the Plots page:

```cpp
    categoryList->addItem("General");
    categoryList->addItem("Import");
    categoryList->addItem("Plots");    // Add this line
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));
    stackedWidget->addWidget(new PlotsSettingsPage(this));    // Add this line
```

### Update Dialog Size

The Plots page has more content, so consider increasing the dialog size:

```cpp
    resize(600, 500);    // Changed from (400, 300)
```

---

## Preference Registration

The plot preferences should be registered at application startup (typically in `main.cpp` or a dedicated initialization function). This ensures default values are available.

**Example registration code (add to initialization):**

```cpp
void registerPlotPreferences()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // Global plot settings
    prefs.registerPreference("plots/lineThickness", 2.0);
    prefs.registerPreference("plots/textSize", 10);
    prefs.registerPreference("plots/crosshairColor", "#FF0000");
    prefs.registerPreference("plots/crosshairThickness", 1.0);
    prefs.registerPreference("plots/yAxisPadding", 0.05);  // 5% as decimal

    // Per-plot settings are registered dynamically based on PlotRegistry
    // This should happen after all plots are registered
    const QVector<PlotValue> plots = PlotRegistry::instance().allPlots();
    for (const PlotValue &pv : plots) {
        QString keyBase = QString("plots/%1/%2").arg(pv.sensorID, pv.measurementID);
        prefs.registerPreference(keyBase + "/color", pv.defaultColor.name());
        prefs.registerPreference(keyBase + "/yAxisMode", "Auto");
        prefs.registerPreference(keyBase + "/yAxisMin", 0.0);
        prefs.registerPreference(keyBase + "/yAxisMax", 100.0);
    }
}
```

---

## Testing Checklist

### Build Verification
- [ ] Project compiles without errors
- [ ] No new compiler warnings related to the settings page
- [ ] MOC (Meta-Object Compiler) processes the header correctly

### UI Verification
- [ ] "Plots" category appears in PreferencesDialog sidebar
- [ ] Clicking "Plots" displays the PlotsSettingsPage
- [ ] "Plot Appearance" group box displays with all global settings
- [ ] "Per-Plot Settings" group box displays with tree widget
- [ ] "Reset All to Defaults" button is visible at the bottom

### Global Settings
- [ ] Line thickness spin box:
  - [ ] Range is 0.5 to 10.0
  - [ ] Step increment is 0.5
  - [ ] Suffix shows " px"
  - [ ] Value loads from preferences on page open
  - [ ] Changes save immediately to preferences
- [ ] Plot text size spin box:
  - [ ] Range is 6 to 24
  - [ ] Suffix shows " pt"
  - [ ] Value loads/saves correctly
- [ ] Crosshair color button:
  - [ ] Shows current color as background
  - [ ] Clicking opens QColorDialog
  - [ ] Selected color updates button and saves
- [ ] Crosshair thickness spin box:
  - [ ] Range is 0.5 to 5.0
  - [ ] Step increment is 0.5
  - [ ] Suffix shows " px"
- [ ] Y-axis padding spin box:
  - [ ] Range is 1 to 50 (percentage)
  - [ ] Suffix shows "%"
  - [ ] Value stored as decimal (0.01-0.50) in preferences

### Per-Plot Settings Tree
- [ ] Tree widget displays all categories from PlotRegistry
- [ ] Categories are expanded by default
- [ ] Categories have bold text and are non-selectable
- [ ] Each plot shows under its category with:
  - [ ] Plot name with units in column 0
  - [ ] Color button in column 1
  - [ ] Y-axis mode combo (Auto/Manual) in column 2
  - [ ] Y-axis min spin box in column 3
  - [ ] Y-axis max spin box in column 4
- [ ] Clicking color button opens QColorDialog
- [ ] Color button background updates after selection
- [ ] Y-axis mode changes save immediately
- [ ] Switching to "Auto" disables min/max spin boxes
- [ ] Switching to "Manual" enables min/max spin boxes
- [ ] Y-axis min/max values save immediately when changed

### Reset Functionality
- [ ] Clicking "Reset All to Defaults" shows confirmation dialog
- [ ] Clicking "No" cancels the reset
- [ ] Clicking "Yes" resets all values to defaults
- [ ] After reset, global settings UI updates to show defaults
- [ ] After reset, per-plot settings UI updates to show defaults
- [ ] After reset, PreferencesManager contains default values

### Persistence
- [ ] Close preferences dialog, reopen, verify values persisted
- [ ] Close application, reopen, verify values persisted
- [ ] Change values in one plot, verify other plots unaffected

### Integration
- [ ] Changes to line thickness affect actual plot rendering
- [ ] Changes to text size affect actual plot text
- [ ] Changes to crosshair color/thickness affect crosshair display
- [ ] Changes to Y-axis padding affect plot Y-axis range
- [ ] Changes to per-plot colors affect plot line colors
- [ ] Switching plot Y-axis mode affects actual plot Y-axis behavior
- [ ] Manual Y-axis min/max values affect actual plot Y-axis range

### Edge Cases
- [ ] Empty PlotRegistry: Page displays without crashing
- [ ] Large number of plots: Tree widget scrolls correctly
- [ ] Invalid stored color: Falls back to default color
- [ ] preferenceChanged signal emitted for each change

---

## Architecture Notes

### Signal Flow
1. User changes value in UI widget
2. Widget emits signal (valueChanged, clicked, etc.)
3. Slot saves to PreferencesManager via `setValue()`
4. PreferencesManager emits `preferenceChanged()` signal
5. Other components (PlotWidget, etc.) respond to signal and update

### Data Flow for Per-Plot Settings
```
PlotRegistry::allPlots()
    -> PlotsSettingsPage builds tree with widgets
    -> User changes value
    -> Save to PreferencesManager with key "plots/{sensorID}/{measurementID}/{setting}"
    -> PlotWidget reads preference when rendering
```

### Key Naming Convention
- Global: `plots/{settingName}` (e.g., `plots/lineThickness`)
- Per-plot: `plots/{sensorID}/{measurementID}/{settingName}` (e.g., `plots/GNSS/hMSL/color`)

---

## Future Enhancements

1. **Search/Filter**: Add a search box to filter plots by name
2. **Bulk Edit**: Select multiple plots and edit common settings
3. **Import/Export**: Save and load plot configurations as files
4. **Preset Themes**: Predefined color schemes for different use cases
5. **Undo/Redo**: Track changes and allow reverting recent edits
