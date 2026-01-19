# FlySight Viewer 2 Preference Pages Implementation Plan

## Executive Summary

This document outlines the implementation plan for adding visual customization preferences to FlySight Viewer 2. The enhancement introduces **four new preference pages** that allow users to customize the appearance of plots, markers, legends, and map displays.

**Key Features:**
- **4 New Preference Pages:** Plots, Markers, Legend, and Map
- **Persistent Storage:** All preferences stored via Windows Registry using Qt's QSettings
- **Real-Time Updates:** Preference changes apply immediately without requiring application restart

This implementation follows a phased approach, allowing parallel development of independent components while maintaining clear dependencies between related work.

---

## Feature Overview

### Plots Preferences

**Global Settings:**
| Setting | Description | Default |
|---------|-------------|---------|
| Line Thickness | Thickness of plot lines in pixels | 2 |
| Text Size | Font size for axis labels and titles | 10 |
| Crosshair Color | Color of the crosshair overlay | Red (#FF0000) |
| Crosshair Thickness | Thickness of crosshair lines in pixels | 1 |
| Y-Axis Padding | Percentage padding above/below data range | 5% |

**Per-Plot Settings:**
| Setting | Description | Default |
|---------|-------------|---------|
| Color | Line color for the specific plot | Varies by plot type |
| Y-Axis Mode | Auto-scale or manual range | Auto |
| Y-Axis Min | Minimum Y value (when manual) | 0 |
| Y-Axis Max | Maximum Y value (when manual) | 100 |

### Markers Preferences

**Per-Marker Settings:**
| Setting | Description | Default |
|---------|-------------|---------|
| Color | Color for each marker type | Varies by marker |

**Actions:**
- Reset all marker colors to defaults

### Legend Preferences

| Setting | Description | Default |
|---------|-------------|---------|
| Text Size | Font size for legend entries | 10 |

### Map Preferences

| Setting | Description | Default |
|---------|-------------|---------|
| Track Line Thickness | Width of the flight track line | 3 |
| Track Opacity | Transparency of the track (0.0-1.0) | 1.0 |
| Cursor Marker Size | Diameter of the position cursor dot | 12 |

---

## Implementation Phases

| Phase | Document | Description | Dependencies |
|-------|----------|-------------|--------------|
| 1 | [phase-1-infrastructure.md](phase-1-infrastructure.md) | PreferencesManager extensions, preference key utilities | None |
| 2 | [phase-2-plots-settings-page.md](phase-2-plots-settings-page.md) | Plots settings page UI and logic | Phase 1 |
| 3 | [phase-3-markers-settings-page.md](phase-3-markers-settings-page.md) | Markers settings page UI and logic | Phase 1 |
| 4 | [phase-4-legend-settings-page.md](phase-4-legend-settings-page.md) | Legend settings page UI and logic | Phase 1 |
| 5 | [phase-5-map-settings-page.md](phase-5-map-settings-page.md) | Map settings page UI and logic | Phase 1 |
| 6 | [phase-6-apply-preferences.md](phase-6-apply-preferences.md) | Apply preferences to components | Phases 1-5 |
| 7 | [phase-7-integration-testing.md](phase-7-integration-testing.md) | Final integration and testing | All phases |

---

## Dependency Graph

```
                    +-------------------+
                    |     Phase 1       |
                    |  Infrastructure   |
                    +-------------------+
                             |
         +-------------------+-------------------+
         |         |         |         |         |
         v         v         v         v         |
    +--------+ +--------+ +--------+ +--------+  |
    |Phase 2 | |Phase 3 | |Phase 4 | |Phase 5 |  |
    | Plots  | |Markers | | Legend | |  Map   |  |
    +--------+ +--------+ +--------+ +--------+  |
         |         |         |         |         |
         +-------------------+-------------------+
                             |
                             v
                    +-------------------+
                    |     Phase 6       |
                    | Apply Preferences |
                    +-------------------+
                             |
                             v
                    +-------------------+
                    |     Phase 7       |
                    |Integration Testing|
                    +-------------------+
```

**Parallelization Opportunities:**
- **Phase 1** must be completed first (foundation for all other phases)
- **Phases 2, 3, 4, 5** can be developed in parallel by different developers
- **Phase 6** requires all UI pages (Phases 1-5) to be complete
- **Phase 7** requires all phases complete for final integration testing

---

## Files Created/Modified Summary

### New Files

| File Path | Description |
|-----------|-------------|
| `src/preferences/preferencekeys.h` | Centralized preference key constants and defaults |
| `src/preferences/plotssettingspage.h` | Plots settings page header |
| `src/preferences/plotssettingspage.cpp` | Plots settings page implementation |
| `src/preferences/markerssettingspage.h` | Markers settings page header |
| `src/preferences/markerssettingspage.cpp` | Markers settings page implementation |
| `src/preferences/legendsettingspage.h` | Legend settings page header |
| `src/preferences/legendsettingspage.cpp` | Legend settings page implementation |
| `src/preferences/mapsettingspage.h` | Map settings page header |
| `src/preferences/mapsettingspage.cpp` | Map settings page implementation |

### Modified Files

| File Path | Changes |
|-----------|---------|
| `src/preferences/preferencesmanager.h` | Add new getter/setter methods for visual preferences |
| `src/preferences/preferencesdialog.cpp` | Register new settings pages in the dialog |
| `src/mainwindow.cpp` | Connect preference signals to update handlers |
| `src/plotwidget.cpp` | Apply plot visual preferences |
| `src/crosshairmanager.cpp` | Apply crosshair color and thickness preferences |
| `src/legendwidget.cpp` | Apply legend text size preference |
| `src/trackmapmodel.cpp` | Apply track line thickness and opacity preferences |
| `src/mapcursordotmodel.cpp` | Apply cursor marker size preference |
| `src/mapwidget.cpp` | Coordinate map preference updates |
| `src/qml/MapDock.qml` | Bind QML properties to preference values |
| `src/CMakeLists.txt` | Add new source files to build |

---

## Preference Keys Reference

### Plots Preferences

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `plots/lineThickness` | int | 2 | Global line thickness in pixels |
| `plots/textSize` | int | 10 | Global text size in points |
| `plots/crosshairColor` | QColor | #FF0000 | Crosshair overlay color |
| `plots/crosshairThickness` | int | 1 | Crosshair line thickness in pixels |
| `plots/yAxisPadding` | double | 0.05 | Y-axis padding as decimal (5%) |
| `plots/<plotId>/color` | QColor | (varies) | Per-plot line color |
| `plots/<plotId>/yAxisMode` | QString | "auto" | "auto" or "manual" |
| `plots/<plotId>/yAxisMin` | double | 0.0 | Manual Y-axis minimum |
| `plots/<plotId>/yAxisMax` | double | 100.0 | Manual Y-axis maximum |

### Markers Preferences

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `markers/<markerId>/color` | QColor | (varies) | Per-marker color |

### Legend Preferences

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `legend/textSize` | int | 10 | Legend text size in points |

### Map Preferences

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `map/trackLineThickness` | int | 3 | Track line width in pixels |
| `map/trackOpacity` | double | 1.0 | Track opacity (0.0 to 1.0) |
| `map/cursorMarkerSize` | int | 12 | Cursor dot diameter in pixels |

---

## Estimated Effort

| Phase | Complexity | Notes |
|-------|------------|-------|
| Phase 1: Infrastructure | Medium | Core foundation; requires careful API design |
| Phase 2: Plots Settings | Medium | Most complex UI with per-plot configuration |
| Phase 3: Markers Settings | Low-Medium | Dynamic list of markers with color pickers |
| Phase 4: Legend Settings | Low | Simple single-setting page |
| Phase 5: Map Settings | Low-Medium | Three settings with QML integration |
| Phase 6: Apply Preferences | Medium | Multiple integration points across codebase |
| Phase 7: Integration Testing | Low | Testing and polish; depends on team availability |

**Total Scope:** Medium-sized feature enhancement spanning UI, settings infrastructure, and component integration.

---

## Getting Started

### For Developers

1. **Read this overview document** to understand the full scope and architecture of the implementation.

2. **Implement Phase 1 first.** This phase establishes the infrastructure that all other phases depend on. No other work can begin until Phase 1 is complete and reviewed.

3. **Phases 2-5 can be assigned to different developers** and worked on in parallel. Each phase is self-contained once the infrastructure is in place:
   - Phase 2 (Plots): Recommended for developers familiar with PlotWidget and chart rendering
   - Phase 3 (Markers): Suitable for any developer comfortable with Qt widgets
   - Phase 4 (Legend): Good starting point for developers new to the codebase
   - Phase 5 (Map): Requires familiarity with Qt/QML integration

4. **Phase 6 should be done by someone familiar with all components.** This phase touches multiple parts of the codebase and requires understanding how preferences flow from the UI to the rendering components.

5. **Phase 7 involves the whole team for testing.** Integration testing should cover:
   - Preference persistence across application restarts
   - Real-time updates when preferences change
   - Edge cases and boundary values
   - Performance with many preference changes

### Code Style Guidelines

- Follow existing FlySight Viewer 2 coding conventions
- Use Qt naming conventions for signals and slots
- Document public APIs with Doxygen-style comments
- Write unit tests for PreferencesManager extensions

### Review Checklist

Before marking a phase complete, verify:
- [ ] All new files added to CMakeLists.txt
- [ ] Preferences persist correctly via QSettings
- [ ] UI matches the application's visual style
- [ ] Changes apply in real-time without restart
- [ ] No memory leaks or resource issues
- [ ] Code compiles without warnings

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | Initial | - | Initial implementation plan |

---

## References

- [Qt QSettings Documentation](https://doc.qt.io/qt-6/qsettings.html)
- [Qt Widgets Documentation](https://doc.qt.io/qt-6/qtwidgets-index.html)
- FlySight Viewer 2 Architecture Documentation (if available)
