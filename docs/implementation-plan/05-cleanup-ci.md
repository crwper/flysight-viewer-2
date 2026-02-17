# Phase 5: Cleanup & CI Updates

## Overview

This phase performs the final cleanup after the Qt Location to Google Maps migration: deleting the now-obsolete `MapDock.qml` file, removing its entry from `src/resources.qrc`, and updating `README.md` to remove geoservices/QtLocation/QtPositioning references from the deployment directory trees and Qt components list. Phase 1 already handled the bulk of CI workflow changes, CMake dependency removal, and `docs/build-system-plan.md` updates, so this phase focuses narrowly on the three remaining cleanup items.

## Dependencies

- **Depends on:** Phase 1 (Build System & Dependencies -- removed Location/Positioning from CMake, removed geoservices deployment, updated CI workflow and build docs), Phase 4 (Google Maps HTML/JS Implementation -- provided the `map.html.in` replacement for `MapDock.qml`)
- **Blocks:** None
- **Assumptions:**
  - After Phase 1, the CMake build no longer references Qt Location, Qt Positioning, or geoservices plugins. The CI workflow (`build.yml`) no longer installs `qtlocation`/`qtpositioning` or checks for geoservices. The `docs/build-system-plan.md` geoservices section has been updated.
  - After Phase 4, `src/resources/map.html.in` contains the full Google Maps implementation that completely replaces `MapDock.qml`. The map dock is fully functional via `QWebEngineView`.
  - The application no longer loads `qrc:/qml/MapDock.qml` at runtime. `MapWidget` (rewritten in Phase 3) loads `map.html` from the filesystem instead.
  - The Video dock still uses `QQuickWidget` with `qrc:/qml/VideoSurface.qml`, so the `qml/VideoSurface.qml` entry in `resources.qrc` must remain.

## Tasks

### Task 5.1: Delete `src/qml/MapDock.qml`

**Purpose:** The QML-based map implementation has been entirely replaced by the Google Maps HTML/JS page (`map.html.in`). The file is no longer loaded or referenced by any code. Keeping it in the repository would cause confusion about which map implementation is active.

**Files to delete:**
- `src/qml/MapDock.qml` -- 470-line QML file containing the Qt Location/OSM map, polyline rendering, cursor dots, hover detection, gesture handling, and map type selector overlay

**Technical Approach:**

1. Delete the file `src/qml/MapDock.qml`. This file (currently 470 lines) contains:
   - Qt Location `Plugin { name: "osm" }` and `Map` components
   - `MapPolyline` rendering for tracks (lines 293-315)
   - `MapQuickItem` cursor dot overlay (lines 319-347)
   - Hover detection algorithm: `_distPointToSegment2`, `_updateHoverAt`, `_requestHoverUpdate`, `_setMapHover`, `_clearMapHover` (lines 40-160)
   - 16ms hover throttle timer (lines 162-181)
   - `DragHandler`, `PinchHandler`, `MouseArea` gesture handlers (lines 217-290)
   - Custom map type selector dropdown (lines 351-469)

   All of this functionality has been ported to JavaScript in `src/resources/map.html.in` (Phase 4).

2. Verify no remaining references to `MapDock.qml` exist in the codebase. After Phase 3, `MapWidget.cpp` no longer uses `QQuickWidget` or loads any QML. The only place `MapDock.qml` should still be referenced is `src/resources.qrc` (handled in Task 5.2).

**Acceptance Criteria:**
- [ ] `src/qml/MapDock.qml` no longer exists in the repository
- [ ] No file in the repository references `MapDock.qml` (search all `.cpp`, `.h`, `.cmake`, `.yml`, `.qrc`, and `.md` files)
- [ ] `src/qml/VideoSurface.qml` still exists (only `MapDock.qml` is deleted)

**Complexity:** S

---

### Task 5.2: Update `src/resources.qrc` to Remove MapDock.qml Entry

**Purpose:** The Qt resource file still lists `qml/MapDock.qml` as a bundled resource. Since the file is being deleted (Task 5.1), its entry must be removed to prevent a build failure.

**Files to modify:**
- `src/resources.qrc` -- remove the `qml/MapDock.qml` file entry

**Technical Approach:**

1. Edit `src/resources.qrc` to remove line 3, which currently reads:

   ```xml
   <file>qml/MapDock.qml</file>
   ```

   The current content of `src/resources.qrc` is:
   ```xml
   <RCC>
       <qresource prefix="/">
           <file>qml/MapDock.qml</file>
           <file>qml/VideoSurface.qml</file>
           <file>resources/icons/FlySightViewer.png</file>
       </qresource>
   </RCC>
   ```

   After this change, it should be:
   ```xml
   <RCC>
       <qresource prefix="/">
           <file>qml/VideoSurface.qml</file>
           <file>resources/icons/FlySightViewer.png</file>
       </qresource>
   </RCC>
   ```

2. Preserve the existing indentation (4 spaces per level) and the `<qresource prefix="/">` wrapper. Do not change any other entries.

3. The `qml/VideoSurface.qml` entry must remain -- it is used by the Video dock (`VideoWidget.cpp` loads it via `QQuickWidget`).

4. The `resources/icons/FlySightViewer.png` entry must remain -- it is the application icon.

**Acceptance Criteria:**
- [ ] `src/resources.qrc` does not contain the string `MapDock.qml`
- [ ] `src/resources.qrc` still contains the `qml/VideoSurface.qml` entry
- [ ] `src/resources.qrc` still contains the `resources/icons/FlySightViewer.png` entry
- [ ] The file is valid XML with proper indentation matching the existing style (4-space indent)
- [ ] The build compiles successfully with the updated resource file (no missing resource errors)

**Complexity:** S

---

### Task 5.3: Update README.md Deployment Directory Trees

**Purpose:** The deployment directory trees in `README.md` still show `geoservices/` plugin directories and `QtLocation`/`QtPositioning` QML module directories. These are no longer deployed since the map uses Google Maps via Qt WebEngine. The documentation must be updated to reflect the actual deployed package contents.

**Files to modify:**
- `README.md` -- remove geoservices and QtLocation/QtPositioning entries from all three platform directory trees; update the Qt Components list; update the "How It Works" table

**Technical Approach:**

1. **Update the Qt Components list** (lines 79-84). Remove the `Location, Positioning` entry and add `WebEngineWidgets, WebChannel`. The current list:

   ```markdown
   - Core, Widgets, PrintSupport
   - QuickWidgets, Quick, Qml, QuickControls2
   - Location, Positioning
   - Multimedia, MultimediaWidgets
   ```

   Should become:

   ```markdown
   - Core, Widgets, PrintSupport
   - QuickWidgets, Quick, Qml, QuickControls2
   - WebEngineWidgets, WebChannel
   - Multimedia, MultimediaWidgets
   ```

2. **Update the "How It Works" table** (line 327). The row currently reads:

   ```markdown
   | Geoservices & QML modules | CMake install rules | CMake install rules | CMake install rules |
   ```

   Since geoservices are no longer deployed, and QML modules (for the Video dock) are handled by `qt_generate_deploy_qml_app_script`, this row should be removed entirely. The QML deployment is already covered by the "Qt libraries & plugins" row which uses `qt_generate_deploy_app_script` (the QML-aware variant). Alternatively, update the row to reflect the new situation:

   ```markdown
   | map.html (Google Maps) | Installed to `resources/` | Installed to `Resources/resources/` | Installed to `usr/resources/` |
   ```

3. **Update the Windows package structure** (lines 429-449). Remove the `geoservices/` entry under `plugins/` (line 440) and the `QtLocation/` and `QtPositioning/` entries under `qml/` (lines 443-444):

   Current:
   ```
   ├── plugins/
   │   ├── platforms/
   │   ├── geoservices/
   │   └── ...
   ├── qml/
   │   ├── QtLocation/
   │   └── QtPositioning/
   ```

   Should become:
   ```
   ├── plugins/
   │   ├── platforms/
   │   └── ...
   ├── qml/
   │   └── QtMultimedia/
   ├── resources/
   │   └── map.html
   ```

   The `qml/` directory still exists because the Video dock uses `QQuickWidget` which may deploy QML modules (e.g., `QtMultimedia`). The `resources/` directory contains the configured `map.html` installed by the Phase 1 install rule.

4. **Update the macOS package structure** (lines 453-473). Remove `geoservices/` under `PlugIns/` (line 464) and `QtLocation/`/`QtPositioning/` under `Resources/qml/` (lines 468-469):

   Current:
   ```
       ├── PlugIns/
       │   ├── platforms/
       │   └── geoservices/
       └── Resources/
           ├── qt.conf
           ├── qml/
           │   ├── QtLocation/
           │   └── QtPositioning/
   ```

   Should become:
   ```
       ├── PlugIns/
       │   ├── platforms/
       │   └── ...
       └── Resources/
           ├── qt.conf
           ├── qml/
           │   └── QtMultimedia/
           ├── resources/
           │   └── map.html
   ```

5. **Update the Linux package structure** (lines 476-499). Remove `geoservices/` under `plugins/` (line 491) and `QtLocation/`/`QtPositioning/` under `qml/` (lines 493-494):

   Current:
   ```
       ├── plugins/
       │   ├── platforms/
       │   └── geoservices/
       ├── qml/
       │   ├── QtLocation/
       │   └── QtPositioning/
   ```

   Should become:
   ```
       ├── plugins/
       │   ├── platforms/
       │   └── ...
       ├── qml/
       │   └── QtMultimedia/
       ├── resources/
       │   └── map.html
   ```

6. **Verify no other geoservices/QtLocation/QtPositioning references remain** in `README.md`. After the above changes, a case-insensitive search for `geoservices`, `QtLocation`, and `QtPositioning` should return zero matches.

**Acceptance Criteria:**
- [ ] The Qt Components list shows `WebEngineWidgets, WebChannel` instead of `Location, Positioning`
- [ ] The "How It Works" table no longer has a "Geoservices & QML modules" row (or it is updated to reflect the `map.html` install)
- [ ] The Windows package structure does not contain `geoservices/`, `QtLocation/`, or `QtPositioning/`
- [ ] The macOS package structure does not contain `geoservices/`, `QtLocation/`, or `QtPositioning/`
- [ ] The Linux package structure does not contain `geoservices/`, `QtLocation/`, or `QtPositioning/`
- [ ] All three package structures show `resources/map.html` in the appropriate platform-specific location
- [ ] A case-insensitive search for "geoservices", "QtLocation", and "QtPositioning" in `README.md` returns zero matches
- [ ] The `qml/` directory is still shown in all three platform structures (the Video dock still deploys QML modules)
- [ ] No other content in `README.md` is changed unnecessarily

**Complexity:** M

---

## Testing Requirements

### Unit Tests

- No new unit tests are needed for this phase (file deletion, resource file edits, and documentation updates are not unit-testable).

### Integration Tests

- **Build test:** Run `cmake --build build` after deleting `MapDock.qml` and updating `resources.qrc`. Verify the build succeeds without errors about missing resources.
- **Resource verification:** After building, verify that the compiled Qt resource binary does not contain `MapDock.qml` (e.g., by checking that `QFile(":/qml/MapDock.qml").exists()` returns `false` at runtime, or by inspecting the `rcc` output).
- **Install test:** Run `cmake --install build` and verify:
  - No `geoservices` directory appears in any platform's install output
  - `resources/map.html` is present in the install output
  - `qml/VideoSurface.qml` is still embedded in the resource binary (Video dock still works)

### Manual Verification

1. Build the application and open the Video dock -- verify it still loads `VideoSurface.qml` correctly from the Qt resource system.
2. Open the Map dock -- verify the Google Maps view loads (not QML). Confirm there are no console errors about missing `MapDock.qml`.
3. Review `README.md` in a Markdown renderer and verify the deployment directory trees are correct, well-formatted, and visually consistent.
4. Search the entire repository for `MapDock.qml` -- verify zero results (except possibly git history or this document).
5. Search the entire repository for `geoservices` -- verify zero results in active code and documentation files (excluding git history, this phase document, and the overview document's historical context).

## Notes for Implementer

### Gotchas

- **Task 5.1 and 5.2 must be done together.** If you delete `MapDock.qml` without removing it from `resources.qrc`, the build will fail because the Qt Resource Compiler (`rcc`) will look for the file listed in the `.qrc` and fail when it does not exist. Conversely, if you remove the `.qrc` entry but leave the file, nothing breaks but the dead file causes confusion. Implement both tasks atomically.

- **Do not delete `src/qml/VideoSurface.qml`.** Only `MapDock.qml` is being replaced. The `src/qml/` directory itself should remain because it still contains `VideoSurface.qml`.

- **README.md tree formatting.** The directory trees in `README.md` use Unicode box-drawing characters. When editing, preserve the exact character set: `├──` for intermediate entries, `└──` for last entries, `│` for continuing vertical lines. Use a monospace font or diff viewer to verify alignment.

- **The `qml/` directory in deployment trees.** Although `QtLocation/` and `QtPositioning/` are removed, other QML modules may be deployed by `qt_generate_deploy_qml_app_script` (e.g., `QtMultimedia` for the Video dock). The `qml/` directory should remain in the trees but with updated contents. If the exact set of deployed QML modules is uncertain, use a representative example like `QtMultimedia/` with a `└── ...` to indicate there may be others.

- **The "How It Works" table row.** The `Geoservices & QML modules` row conflates two separate concerns. Geoservices are completely gone, but QML module deployment still happens (for the Video dock). The safest approach is to replace the row with one describing the new `map.html` deployment, since QML module deployment is already implied by the Qt deployment row.

### Decisions Made

- **Directory trees show `QtMultimedia/` as a representative QML module** instead of leaving the `qml/` entries empty or removing the `qml/` directory. The Video dock's `QQuickWidget` usage causes `qt_generate_deploy_qml_app_script` to scan for and deploy QML imports, and `QtMultimedia` is the most likely deployed module. This keeps the trees accurate and informative.

- **`resources/map.html` is added to the deployment trees** to document the new file that Phase 1 installs alongside the binary. This makes the trees accurately reflect what a deployed package contains.

- **The "How It Works" table row is replaced** rather than simply deleted, because the `map.html` install is a deployment concern that should be documented in the same table for discoverability.

### Open Questions

- None. This phase is straightforward cleanup with no architectural decisions or ambiguities.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. `src/qml/MapDock.qml` is deleted from the repository
3. `src/resources.qrc` no longer references `MapDock.qml` and still references `VideoSurface.qml` and the app icon
4. `README.md` contains no references to `geoservices`, `QtLocation`, or `QtPositioning`
5. `README.md` deployment directory trees accurately reflect the new package contents (including `resources/map.html`)
6. The application builds and installs successfully
7. The Video dock still works (QML resource loading is unaffected)
8. The Map dock still works (Google Maps loads from `map.html`)
9. No TODOs or placeholder code remains
