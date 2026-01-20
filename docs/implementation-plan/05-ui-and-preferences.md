# Phase 5: UI and Preferences

## Overview

This phase completes the user-facing integration of the unit conversion system by wiring the GeneralSettingsPage combobox to the UnitConverter and adding a "U" keyboard shortcut for quick unit system toggling. After this phase, users can change between Metric and Imperial units via either the Preferences dialog or a single keypress, with all display components updating immediately.

## Dependencies

- **Depends on:** Phase 1 (Core Unit System) - requires UnitConverter singleton with `setSystem()`, `currentSystem()`, `availableSystems()`, and `systemChanged` signal; Phase 4 (Display Integration) - PlotWidget and LegendPresenter already connected to UnitConverter::systemChanged for reactive updates
- **Blocks:** None
- **Assumptions:**
  - UnitConverter singleton exists at `src/units/unitconverter.h` with methods: `setSystem(QString)`, `currentSystem()`, `availableSystems()`, and signal `systemChanged(QString)`
  - PlotWidget and LegendPresenter are already connected to UnitConverter::systemChanged (from Phase 4)
  - GeneralSettingsPage already has a units combobox populated with "Metric" and "Imperial" options
  - GeneralSettingsPage::saveSettings() currently writes to PreferencesManager but does not trigger UnitConverter

## Tasks

### Task 5.1: Wire GeneralSettingsPage to UnitConverter

**Purpose:** Ensure that changing the units combobox in the Preferences dialog immediately triggers UnitConverter to update the active unit system, which in turn updates all display components.

**Files to modify:**
- `src/preferences/generalsettingspage.cpp` - Add include and call UnitConverter::setSystem() in saveSettings()

**Technical Approach:**

1. Add include at the top of `generalsettingspage.cpp` (after line 4):
   ```cpp
   #include "units/unitconverter.h"
   ```

2. Modify the `saveSettings()` method (lines 57-61) to call UnitConverter after saving to preferences:

   Current implementation:
   ```cpp
   void GeneralSettingsPage::saveSettings() {
       PreferencesManager &prefs = PreferencesManager::instance();
       prefs.setValue("general/units", unitsComboBox->currentText());
       prefs.setValue("general/logbookFolder", logbookFolderLineEdit->text());
   }
   ```

   Modified implementation:
   ```cpp
   void GeneralSettingsPage::saveSettings() {
       PreferencesManager &prefs = PreferencesManager::instance();
       prefs.setValue("general/units", unitsComboBox->currentText());
       prefs.setValue("general/logbookFolder", logbookFolderLineEdit->text());

       // Notify UnitConverter of the change
       UnitConverter::instance().setSystem(unitsComboBox->currentText());
   }
   ```

   Note: UnitConverter::setSystem() already checks if the value has changed before emitting systemChanged, so calling it even when only the logbook folder changed is harmless.

3. Alternatively, for cleaner separation, only call setSystem() when the units value actually changes:
   ```cpp
   void GeneralSettingsPage::saveSettings() {
       PreferencesManager &prefs = PreferencesManager::instance();

       QString newUnits = unitsComboBox->currentText();
       if (prefs.getValue("general/units").toString() != newUnits) {
           prefs.setValue("general/units", newUnits);
           UnitConverter::instance().setSystem(newUnits);
       }

       prefs.setValue("general/logbookFolder", logbookFolderLineEdit->text());
   }
   ```

   However, since UnitConverter::setSystem() already has change detection (per Phase 1 spec), the simpler approach is acceptable.

**Reference patterns:**
- `src/preferences/generalsettingspage.cpp` lines 57-61 for current saveSettings() implementation
- `src/preferences/preferencesmanager.h` lines 48-53 for change detection pattern

**Acceptance Criteria:**
- [ ] `units/unitconverter.h` is included in generalsettingspage.cpp
- [ ] saveSettings() calls UnitConverter::setSystem() with the combobox value
- [ ] Changing units in Preferences dialog immediately updates plots and legends
- [ ] Other preferences (logbook folder) continue to work correctly
- [ ] Code compiles without errors

**Complexity:** S

---

### Task 5.2: Populate Units Combobox from UnitConverter

**Purpose:** Ensure the units combobox displays available unit systems from UnitConverter rather than hardcoded strings, enabling future extensibility.

**Files to modify:**
- `src/preferences/generalsettingspage.cpp` - Modify createUnitsGroup() to use UnitConverter::availableSystems()

**Technical Approach:**

1. In `createUnitsGroup()` (lines 20-35), replace the hardcoded combobox items with values from UnitConverter:

   Current implementation (lines 24-25):
   ```cpp
   unitsComboBox = new QComboBox(this);
   unitsComboBox->addItems({"Metric", "Imperial"});
   ```

   Modified implementation:
   ```cpp
   unitsComboBox = new QComboBox(this);
   unitsComboBox->addItems(UnitConverter::instance().availableSystems());
   ```

2. Ensure the current selection initialization (lines 27-29) still works:
   ```cpp
   // Initialize from settings
   QString units = PreferencesManager::instance().getValue("general/units").toString();
   unitsComboBox->setCurrentText(units);
   ```

   This should continue to work since setCurrentText() will find the matching item in the list.

3. As a safety measure, verify the current value exists in the list:
   ```cpp
   // Initialize from settings
   QString units = PreferencesManager::instance().getValue("general/units").toString();
   int index = unitsComboBox->findText(units);
   if (index >= 0) {
       unitsComboBox->setCurrentIndex(index);
   }
   // If not found, combobox defaults to first item, which is acceptable
   ```

   This is optional but adds robustness if preferences contain an invalid value.

**Reference patterns:**
- `src/preferences/generalsettingspage.cpp` lines 20-35 for createUnitsGroup() implementation

**Acceptance Criteria:**
- [ ] Combobox items come from UnitConverter::availableSystems()
- [ ] Current unit system is correctly selected on dialog open
- [ ] Future unit systems (e.g., "Aviation") will appear automatically when added to UnitDefinitions
- [ ] Dialog displays correctly with no visual changes from current behavior

**Complexity:** S

---

### Task 5.3: Add "U" Keyboard Shortcut Action in MainWindow

**Purpose:** Enable users to quickly toggle between unit systems using the "U" key, without opening the Preferences dialog.

**Files to modify:**
- `src/mainwindow.h` - Add private slot declaration
- `src/mainwindow.cpp` - Add action creation, connection, and slot implementation

**Technical Approach:**

1. In `src/mainwindow.h`, add a private slot declaration (after line 75, near other slots):
   ```cpp
   private slots:
       // ... existing slots ...
       void on_action_ToggleUnits_triggered();
   ```

2. In `src/mainwindow.cpp`, add the UnitConverter include (it may already be included; if not, add after line 34):
   ```cpp
   #include "units/unitconverter.h"
   ```

3. In the MainWindow constructor (around line 200-205, after setupPlotTools() or in a logical location with other keyboard shortcut setup), create and connect the action:

   Follow the pattern established in `initializePlotsMenu()` (lines 1720-1780) for action creation:
   ```cpp
   // Create "U" keyboard shortcut for unit system toggle
   QAction *toggleUnitsAction = new QAction(tr("Toggle Units"), this);
   toggleUnitsAction->setShortcut(QKeySequence(Qt::Key_U));
   // Add to window so shortcut works even without menu
   addAction(toggleUnitsAction);
   connect(toggleUnitsAction, &QAction::triggered,
           this, &MainWindow::on_action_ToggleUnits_triggered);
   ```

   Note: We use `addAction()` on the MainWindow to ensure the shortcut works globally without needing to add it to a menu. This follows the pattern used for other non-menu shortcuts.

4. Implement the slot to cycle through available unit systems:
   ```cpp
   void MainWindow::on_action_ToggleUnits_triggered()
   {
       UnitConverter &converter = UnitConverter::instance();
       QStringList systems = converter.availableSystems();

       if (systems.isEmpty())
           return;

       // Find current system index
       QString current = converter.currentSystem();
       int currentIndex = systems.indexOf(current);

       // Cycle to next system (wrap around)
       int nextIndex = (currentIndex + 1) % systems.size();
       QString nextSystem = systems.at(nextIndex);

       // Apply the change
       converter.setSystem(nextSystem);

       qDebug() << "Toggled units to:" << nextSystem;
   }
   ```

   This implementation:
   - Cycles through all available systems (not just toggling between two)
   - Handles the case where current system is not found (defaults to first)
   - Uses modulo for wrap-around behavior
   - Logs the change for debugging

**Reference patterns:**
- `src/mainwindow.cpp` lines 1720-1780 for QAction creation in initializePlotsMenu()
- `src/mainwindow.cpp` lines 1833-1848 for setupPlotTools() action group pattern
- `src/mainwindow.cpp` lines 524-540 for tool trigger slot pattern

**Acceptance Criteria:**
- [ ] Pressing "U" key toggles unit system
- [ ] Shortcut works regardless of which widget has focus
- [ ] Toggle cycles through all available systems (Metric -> Imperial -> Metric)
- [ ] Plots and legends update immediately after toggle
- [ ] Debug message confirms the toggle occurred
- [ ] Shortcut does not conflict with other keyboard shortcuts

**Complexity:** M

---

### Task 5.4: Optionally Add Units Toggle to View Menu

**Purpose:** Provide menu discoverability for the unit toggle feature, showing users that the "U" shortcut exists.

**Files to modify:**
- `src/mainwindow.cpp` - Add toggle units action to a menu (View or Plots menu)

**Technical Approach:**

This task is optional but improves discoverability. The "U" shortcut should be visible in a menu so users learn it exists.

1. Option A: Add to View/Window menu

   In `initializeWindowMenu()` (lines 1782-1803), add the toggle units action:
   ```cpp
   void MainWindow::initializeWindowMenu()
   {
       QMenu *windowMenu = ui->menuWindow;
       Q_ASSERT(windowMenu);

       // ... existing dock actions ...

       windowMenu->addSeparator();

       // Add units toggle action
       QAction *toggleUnitsAction = new QAction(tr("Toggle Units"), this);
       toggleUnitsAction->setShortcut(QKeySequence(Qt::Key_U));
       connect(toggleUnitsAction, &QAction::triggered,
               this, &MainWindow::on_action_ToggleUnits_triggered);
       windowMenu->addAction(toggleUnitsAction);
   }
   ```

   If using this approach, remove the `addAction()` call from Task 5.3 since menu actions automatically register their shortcuts.

2. Option B: Add to Plots menu (more thematically appropriate)

   In `initializePlotsMenu()` (around line 1778, before or after the plot items), add:
   ```cpp
   // Add separator and units toggle at the end
   plotsMenu->addSeparator();
   QAction *toggleUnitsAction = new QAction(tr("Toggle Units"), this);
   toggleUnitsAction->setShortcut(QKeySequence(Qt::Key_U));
   connect(toggleUnitsAction, &QAction::triggered,
           this, &MainWindow::on_action_ToggleUnits_triggered);
   plotsMenu->addAction(toggleUnitsAction);
   ```

**Decision**: Option B (Plots menu) is recommended since unit conversion is primarily about how plot data is displayed.

**Reference patterns:**
- `src/mainwindow.cpp` lines 1782-1803 for initializeWindowMenu()
- `src/mainwindow.cpp` lines 1720-1780 for initializePlotsMenu()

**Acceptance Criteria:**
- [ ] "Toggle Units" menu item appears in Plots menu (or Window menu)
- [ ] Menu item shows "U" as keyboard shortcut
- [ ] Clicking menu item toggles unit system
- [ ] Menu item is placed in a logical location (after separator)

**Complexity:** S

---

### Task 5.5: Synchronize GeneralSettingsPage on Dialog Open

**Purpose:** Ensure the units combobox reflects the current unit system when the Preferences dialog is opened, even if units were changed via keyboard shortcut.

**Files to modify:**
- `src/preferences/generalsettingspage.cpp` - Ensure initialization reads from current state

**Technical Approach:**

The current implementation reads from PreferencesManager when the combobox is created. However, if the user:
1. Opens Preferences (combobox shows "Metric")
2. Closes Preferences without changing
3. Presses "U" to switch to Imperial
4. Opens Preferences again

The combobox should show "Imperial", not "Metric" cached from creation.

Since GeneralSettingsPage is created fresh each time PreferencesDialog is opened (verify by checking preferencesdialog.cpp), the current implementation should already work correctly because:
1. createUnitsGroup() reads from PreferencesManager each time
2. UnitConverter::setSystem() writes to PreferencesManager (per Phase 1 spec)

**Verification needed**: Check if GeneralSettingsPage is recreated on each dialog open or persists.

If GeneralSettingsPage persists (unlikely given typical Qt dialog patterns), add a refresh mechanism:

1. Add a public method to refresh the combobox:
   ```cpp
   // In generalsettingspage.h
   public:
       void refreshSettings();
   ```

   ```cpp
   // In generalsettingspage.cpp
   void GeneralSettingsPage::refreshSettings()
   {
       QString units = PreferencesManager::instance().getValue("general/units").toString();
       unitsComboBox->setCurrentText(units);

       QString folder = PreferencesManager::instance().getValue("general/logbookFolder").toString();
       logbookFolderLineEdit->setText(folder);
   }
   ```

2. Call this method when the dialog is shown (in PreferencesDialog or MainWindow).

**Most likely outcome**: No changes needed if pages are recreated. Verify by testing.

**Reference patterns:**
- `src/preferences/preferencesdialog.cpp` for page lifecycle management

**Acceptance Criteria:**
- [ ] Units combobox shows current system when Preferences dialog opens
- [ ] Works correctly after using "U" shortcut before opening Preferences
- [ ] No stale state from previous dialog opens

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No formal unit test framework is in use. Manual verification is the primary testing approach.

### Integration Tests

1. **Build verification**: Project compiles without errors after all modifications
2. **Signal connectivity**: Verify combobox change triggers UnitConverter::systemChanged
3. **Keyboard shortcut**: Verify "U" key triggers unit toggle

### Manual Verification

1. **Preferences dialog toggle**:
   - Open Preferences (Edit -> Preferences or similar)
   - Navigate to General tab
   - Change units from "Metric" to "Imperial"
   - Observe plots and legends update immediately
   - Close Preferences dialog
   - Verify displays remain in Imperial mode

2. **Keyboard shortcut toggle**:
   - Ensure a plot with data is visible
   - Press "U" key
   - Verify unit system toggles (check Y-axis labels, legend values)
   - Press "U" again
   - Verify system toggles back
   - Verify shortcut works when different widgets have focus (plot, logbook, etc.)

3. **Preference persistence**:
   - Set units to Imperial via Preferences
   - Close the application completely
   - Reopen the application
   - Verify units are still Imperial
   - Open Preferences
   - Verify combobox shows "Imperial"

4. **Shortcut and dialog synchronization**:
   - Set units to Metric via Preferences
   - Close Preferences
   - Press "U" to switch to Imperial
   - Open Preferences
   - Verify combobox shows "Imperial"
   - Change to Metric in combobox
   - Close Preferences
   - Press "U"
   - Verify system is now Imperial

5. **Menu discoverability** (if Task 5.4 completed):
   - Open Plots menu (or Window menu)
   - Verify "Toggle Units" item exists
   - Verify shortcut "U" is shown
   - Click the menu item
   - Verify units toggle

6. **Edge cases**:
   - Open Preferences, change units, click Cancel (if dialog has cancel) - units should revert
   - Rapidly press "U" multiple times - should toggle cleanly without errors
   - Set invalid unit value directly in QSettings (advanced test) - combobox should default gracefully

## Notes for Implementer

### Gotchas

1. **Signal recursion**: When saveSettings() calls setSystem(), which calls PreferencesManager::setValue(), which emits preferenceChanged, ensure this doesn't trigger saveSettings() again. The change detection in UnitConverter::setSystem() and PreferencesManager::setValue() should prevent this.

2. **Combobox index vs text**: Use `setCurrentText()` rather than `setCurrentIndex()` when setting the combobox value from preferences, as it's more robust if the order of items changes.

3. **Action ownership**: When creating QAction for the keyboard shortcut, ensure it's parented to MainWindow (`new QAction(tr("..."), this)`) to avoid memory leaks.

4. **Shortcut conflicts**: Verify "U" doesn't conflict with other shortcuts. Based on the existing code:
   - E, H, V, S are used for plot toggles
   - Shift+H, Shift+V, Shift+S, Shift+N are used for accuracy plots
   - Ctrl+1, Ctrl+2 are used for axis selection
   - "U" appears to be available

5. **Include guards**: Ensure `units/unitconverter.h` is included in generalsettingspage.cpp. The include path should work since both files are in subdirectories of `src/`.

6. **Dialog lifecycle**: Qt dialogs created with `exec()` are typically destroyed after closing. If PreferencesDialog caches its pages, the synchronization issue in Task 5.5 becomes relevant.

### Decisions Made

1. **Simple setSystem() call in saveSettings()**: Rather than adding complex change detection in GeneralSettingsPage, rely on UnitConverter's existing change detection. This keeps the code simpler.

2. **Cycle through systems**: The "U" shortcut cycles through all available systems rather than hardcoding a toggle between Metric and Imperial. This supports future extensibility (e.g., adding "Aviation" system).

3. **Plots menu for toggle action**: Placing the toggle in the Plots menu makes thematic sense since units affect plot display. Window menu would also be acceptable.

4. **Debug output**: Adding `qDebug()` output when toggling helps during development and debugging. Can be removed or changed to `qInfo()` for release.

### Open Questions

None - all design decisions are resolved based on the overview document and dependency phase specifications.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. All tests pass (manual verification steps succeed)
3. Code follows patterns established in reference files (QAction creation, slot naming, signal connections)
4. No TODOs or placeholder code remains
5. Project builds and runs without errors
6. Changing units in Preferences dialog immediately updates all displays
7. "U" keyboard shortcut toggles unit system
8. Toggle cycles through all available systems
9. Preferences persist correctly between sessions
10. Preferences dialog shows current state when opened (even after using shortcut)

---

Phase 5 documentation complete.
- Tasks: 5
- Estimated complexity: 6 (S=1 + S=1 + M=2 + S=1 + S=1)
- Ready for implementation: Yes
