# Phase 1: Build System & Dependencies

## Overview

This phase modifies the CMake build system to add Qt WebEngine dependencies (WebEngineWidgets, WebChannel), remove Qt Location and Positioning dependencies, define the Google Maps API key cache variable with a `configure_file` step for `map.html.in`, and remove the geoservices plugin deployment block. It also updates the CI workflow to stop installing QtLocation/QtPositioning modules and remove geoservices verification checks. These changes lay the build-system foundation for all subsequent phases.

## Dependencies

- **Depends on:** None -- can begin immediately
- **Blocks:** Phase 3 (MapBridge & MapWidget Rewrite), Phase 5 (Cleanup & CI Updates)
- **Assumptions:** Qt 6.7.3 (or compatible Qt 6.x) is installed with the WebEngineWidgets and WebChannel modules available. The Video dock (`VideoWidget.cpp`) uses `QQuickWidget`, so the `QuickWidgets`, `Quick`, and `Qml` Qt modules must remain in the build. The `map.html.in` template file does not yet exist (it will be created in Phase 4), so the `configure_file` call will fail until that file is created; the implementer should create a minimal placeholder `map.html.in` to keep CMake happy.

## Tasks

### Task 1.1: Add WebEngine Dependencies to CMake

**Purpose:** The new map implementation uses `QWebEngineView` and `QWebChannel`, which require the `WebEngineWidgets` and `WebChannel` Qt modules to be found and linked.

**Files to modify:**
- `src/CMakeLists.txt` -- add `WebEngineWidgets WebChannel` to `find_package` and add corresponding link targets to `target_link_libraries`

**Technical Approach:**

1. In `src/CMakeLists.txt` line 83, add `WebEngineWidgets WebChannel` to the `find_package` component list. The current line reads:

   ```cmake
   find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets PrintSupport QuickWidgets Quick Qml Location Positioning Multimedia MultimediaWidgets)
   ```

   After this task (combined with Task 1.2), it should read:

   ```cmake
   find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets PrintSupport QuickWidgets Quick Qml WebEngineWidgets WebChannel Multimedia MultimediaWidgets)
   ```

   Place `WebEngineWidgets WebChannel` where `Location Positioning` used to be (after `Qml`, before `Multimedia`) to maintain logical grouping.

2. In the `target_link_libraries(FlySightViewer PRIVATE ...)` block (lines ~483-504), add two new link targets after the existing Qt link entries. Add them where `Qt${QT_VERSION_MAJOR}::Location` and `Qt${QT_VERSION_MAJOR}::Positioning` currently are (lines 497-498):

   ```cmake
   Qt${QT_VERSION_MAJOR}::WebEngineWidgets
   Qt${QT_VERSION_MAJOR}::WebChannel
   ```

**Acceptance Criteria:**
- [ ] `find_package` on line 83 includes `WebEngineWidgets WebChannel` in the REQUIRED COMPONENTS list
- [ ] `target_link_libraries` for `FlySightViewer` includes `Qt${QT_VERSION_MAJOR}::WebEngineWidgets` and `Qt${QT_VERSION_MAJOR}::WebChannel`
- [ ] CMake configure succeeds when Qt WebEngine is installed

**Complexity:** S

---

### Task 1.2: Remove Location and Positioning Dependencies from CMake

**Purpose:** Qt Location and Positioning are only used by the current QML-based map. Removing them reduces build requirements and eliminates the need for the geoservices plugin deployment.

**Files to modify:**
- `src/CMakeLists.txt` -- remove `Location Positioning` from `find_package` and remove corresponding link targets from `target_link_libraries`

**Technical Approach:**

1. In `src/CMakeLists.txt` line 83, remove `Location Positioning` from the `find_package` component list. See Task 1.1 for the combined final result.

2. In the `target_link_libraries(FlySightViewer PRIVATE ...)` block, remove these two lines (currently lines 497-498):

   ```cmake
   Qt${QT_VERSION_MAJOR}::Location
   Qt${QT_VERSION_MAJOR}::Positioning
   ```

3. **Do NOT remove** `QuickWidgets`, `Quick`, or `Qml` -- the Video dock (`src/ui/docks/video/VideoWidget.cpp`) still uses `QQuickWidget`, which depends on all three.

**Acceptance Criteria:**
- [ ] `find_package` on line 83 does NOT contain `Location` or `Positioning`
- [ ] `target_link_libraries` for `FlySightViewer` does NOT contain `Qt${QT_VERSION_MAJOR}::Location` or `Qt${QT_VERSION_MAJOR}::Positioning`
- [ ] `QuickWidgets`, `Quick`, and `Qml` remain in both `find_package` and `target_link_libraries`
- [ ] CMake configure succeeds without Qt Location / Positioning being installed

**Complexity:** S

---

### Task 1.3: Define Google Maps API Key Cache Variable and configure_file

**Purpose:** The Google Maps API key must be embedded into the HTML template at build time. A CMake cache variable allows developers and CI to pass the key at configure time, and `configure_file` substitutes it into the HTML output.

**Files to modify:**
- `src/CMakeLists.txt` -- add `GOOGLE_MAPS_API_KEY` cache variable and `configure_file` call

**Files to create:**
- `src/resources/map.html.in` -- minimal placeholder template (the real content is created in Phase 4, but the file must exist for `configure_file` to succeed)

**Technical Approach:**

1. Add the cache variable definition near the top of `src/CMakeLists.txt`, in the "Variable Definition Conventions" section (after line 53, near the other cache variables). Follow the established pattern of CACHE STRING variables documented in lines 20-41:

   ```cmake
   set(GOOGLE_MAPS_API_KEY "" CACHE STRING "Google Maps JavaScript API key")
   ```

2. Also update the conventions comment block (lines 22-40) to include `GOOGLE_MAPS_API_KEY` in the list of documented cache variables.

3. Add a `configure_file` call after the `find_package` section and before the source file list. A good location is just before the `# helpers` section (before line 143), or immediately after the Qt find_package block (after line 91). The call:

   ```cmake
   # ─────────────────────────────── Google Maps HTML template
   configure_file(
       "${CMAKE_CURRENT_SOURCE_DIR}/resources/map.html.in"
       "${CMAKE_CURRENT_BINARY_DIR}/resources/map.html"
       @ONLY
   )
   ```

   The `@ONLY` flag ensures only `@VAR@`-style substitutions are processed, avoiding accidental expansion of `${...}` sequences in the HTML/JavaScript content.

4. Add an install rule for the configured `map.html` file so it is deployed alongside the executable. Place this near the existing install rules (after line 556). The install destination must be platform-aware:

   ```cmake
   # Install configured map.html for Google Maps view
   if(APPLE)
       install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/map.html"
           DESTINATION "FlySightViewer.app/Contents/Resources/resources"
       )
   elseif(WIN32)
       install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/map.html"
           DESTINATION "resources"
       )
   else()
       install(FILES "${CMAKE_CURRENT_BINARY_DIR}/resources/map.html"
           DESTINATION "${CMAKE_INSTALL_BINDIR}/../resources"
       )
   endif()
   ```

5. Create a minimal placeholder `src/resources/map.html.in` so that `configure_file` does not fail during Phase 1:

   ```html
   <!DOCTYPE html>
   <html>
   <head><meta charset="utf-8"><title>Map</title></head>
   <body>
   <p>Google Maps API key: @GOOGLE_MAPS_API_KEY@</p>
   <p>This is a placeholder. The full implementation is added in Phase 4.</p>
   </body>
   </html>
   ```

   This file will be overwritten with the real Google Maps implementation in Phase 4.

**Acceptance Criteria:**
- [ ] `GOOGLE_MAPS_API_KEY` appears as a CACHE STRING variable in `src/CMakeLists.txt`
- [ ] `configure_file` processes `src/resources/map.html.in` into `${CMAKE_CURRENT_BINARY_DIR}/resources/map.html` with `@ONLY`
- [ ] Running `cmake -DGOOGLE_MAPS_API_KEY="test123" ...` produces a `map.html` in the build directory containing "test123"
- [ ] Running `cmake ...` without `-DGOOGLE_MAPS_API_KEY` produces a `map.html` with an empty string where the key would be
- [ ] `src/resources/map.html.in` exists and contains the `@GOOGLE_MAPS_API_KEY@` placeholder
- [ ] An install rule deploys the configured `map.html` alongside the executable
- [ ] The conventions comment block documents the new variable

**Complexity:** M

---

### Task 1.4: Remove Geoservices Plugin Deployment from CMakeLists.txt

**Purpose:** With Qt Location removed, the geoservices plugins are no longer needed. The deployment block adds unnecessary complexity and produces warnings when the plugins are not found.

**Files to modify:**
- `src/CMakeLists.txt` -- delete the entire "Geoservices Plugin Deployment" section

**Technical Approach:**

1. Delete lines ~633-679 in `src/CMakeLists.txt` -- the entire block starting with the comment `# ─────────────────────────────── Geoservices Plugin Deployment` and ending just before the `# ─────────────────────────────── Additional Qt Plugin Categories` comment (line 681).

   The block to remove begins at:
   ```cmake
   # ─────────────────────────────── Geoservices Plugin Deployment
   # These are not included by qt_generate_deploy_app_script by default
   ```

   And ends just before:
   ```cmake
   # ─────────────────────────────── Additional Qt Plugin Categories
   ```

   This removes the entire geoservices install() call, the plugin counting, and the install-time verification code.

2. Update the comment on the `qt_generate_deploy_qml_app_script` section (lines 597-604). The current comment references "MapDock" QML components, "QtLocation, QtPositioning, etc." Remove those references. The comment should explain that the QML-aware variant is still used because the Video dock uses QML (VideoSurface.qml). Updated comment:

   ```cmake
   # ─────────────────────────────── Qt deployment (Qt 6.3+)
   # On all platforms with Qt 6, use the built-in deployment API to bundle Qt libraries.
   # We use the QML-aware variant (qt_generate_deploy_qml_app_script) because the app
   # includes QML components (VideoSurface). This is a strict superset of
   # qt_generate_deploy_app_script — it deploys all C++ runtime dependencies AND
   # automatically discovers and deploys QML modules (QtMultimedia, etc.)
   # by scanning QML source files for import statements.
   ```

3. Update the "QML Module & qt.conf Deployment" comment block (lines 753-757) to remove the QtLocation/QtPositioning references. The current comment reads:

   ```cmake
   # QtLocation, QtPositioning, and other QML modules are deployed automatically
   ```

   Change to:

   ```cmake
   # QML modules (e.g., QtMultimedia) are deployed automatically
   ```

**Acceptance Criteria:**
- [ ] The "Geoservices Plugin Deployment" block (lines ~633-679) is completely removed from `src/CMakeLists.txt`
- [ ] No remaining references to "geoservices" exist in `src/CMakeLists.txt`
- [ ] The `qt_generate_deploy_qml_app_script` comment no longer references MapDock, QtLocation, or QtPositioning
- [ ] The "QML Module & qt.conf Deployment" comment no longer references QtLocation or QtPositioning
- [ ] CMake configure and install succeed without geoservices-related warnings

**Complexity:** S

---

### Task 1.5: Remove Geoservices Verification from QtPathDiscovery.cmake

**Purpose:** The geoservices verification in the Qt path discovery module is no longer relevant since geoservices plugins are not deployed.

**Files to modify:**
- `cmake/QtPathDiscovery.cmake` -- remove the geoservices, QtLocation, and QtPositioning verification checks

**Technical Approach:**

1. In `cmake/QtPathDiscovery.cmake`, delete lines 175-196 (the "Additional verification" section at the end of the `find_qt6_resource_dirs()` function, but still inside the function body before the `endfunction()` on line 198). These lines check for:
   - Geoservices plugins directory and count (lines 175-182)
   - QtLocation QML module (lines 184-189)
   - QtPositioning QML module (lines 191-196)

   The block to remove starts with:
   ```cmake
   # Additional verification: Check for geoservices (commonly needed)
   ```

   And ends just before the `endfunction()` line.

2. Update the module's header comment (lines 1-25) to remove any mention of geoservices verification. The comment "After calling find_qt6_resource_dirs(), the following variables are set:" and the variable list are still accurate and should remain.

**Acceptance Criteria:**
- [ ] Lines 175-196 of `cmake/QtPathDiscovery.cmake` (geoservices/QtLocation/QtPositioning checks) are removed
- [ ] The `find_qt6_resource_dirs()` function still works correctly for discovering QT6_PREFIX, QT6_PLUGINS_DIR, and QT6_QML_DIR
- [ ] No references to "geoservices", "QtLocation", or "QtPositioning" remain in `cmake/QtPathDiscovery.cmake`
- [ ] The function's `endfunction()` is preserved

**Complexity:** S

---

### Task 1.6: Update CI Workflow (build.yml)

**Purpose:** The CI workflow installs Qt modules, checks for geoservices plugins, and verifies QtLocation/QtPositioning QML modules. All of these must be updated to reflect the removal of Location/Positioning and the addition of WebEngine.

**Files to modify:**
- `.github/workflows/build.yml` -- update Qt module installation, remove geoservices checks, add GOOGLE_MAPS_API_KEY to CMake configure

**Technical Approach:**

1. **Update Qt module installation** (line 218). The `install-qt-action` step currently installs:

   ```yaml
   modules: qtlocation qtpositioning qtmultimedia
   ```

   Change to:

   ```yaml
   modules: qtwebengine qtwebchannel qtmultimedia
   ```

   Note: `qtwebengine` is the module name for `jurplel/install-qt-action` that provides both `WebEngineWidgets` and `WebEngineCore`. `qtwebchannel` provides `WebChannel`.

2. **Update "Debug Qt installation paths" step** (lines 249-287). Remove the geoservices-specific checks. Currently lines 266-274 check for a geoservices directory inside the plugins directory:

   ```bash
   if [ -d "$dir/geoservices" ]; then
     echo "  Geoservices: $(ls "$dir/geoservices" 2>/dev/null | wc -l) files"
   else
     echo "  Geoservices: directory not found"
   fi
   ```

   Remove these lines. Also remove the QtLocation/QtPositioning QML checks (lines 278-284):

   ```bash
   echo "  QtLocation: $(test -d "$dir/QtLocation" && echo "present" || echo "missing")"
   echo "  QtPositioning: $(test -d "$dir/QtPositioning" && echo "present" || echo "missing")"
   ```

   Replace with WebEngine checks if desired, or simply remove them since WebEngine does not use QML modules.

3. **Add `GOOGLE_MAPS_API_KEY` to CMake configure step** (lines 416-432). Add a new parameter to the `cmake` configure command:

   ```bash
   -DGOOGLE_MAPS_API_KEY="${GOOGLE_MAPS_API_KEY:-}"
   ```

   This uses the environment variable if set (e.g., from a GitHub secret), or defaults to empty. Optionally, the CI can also pass it using the `${VAR:+...}` pattern already used for other variables:

   ```bash
   ${GOOGLE_MAPS_API_KEY:+-DGOOGLE_MAPS_API_KEY="$GOOGLE_MAPS_API_KEY"}
   ```

4. **Update Windows deployment verification** (lines 481-517). Remove the geoservices plugin check (lines 492-498):

   ```bash
   echo "=== Checking Qt plugins ==="
   if [ -d "build/install/plugins/geoservices" ]; then
     ...
   fi
   ```

   And remove the QtLocation/QtPositioning QML module checks (lines 500-508):

   ```bash
   echo "=== Checking QML modules ==="
   for mod in QtLocation QtPositioning; do
     ...
   done
   ```

5. **Update macOS deployment verification** (lines 522-646). Remove the geoservices plugin check (lines 609-624):

   ```bash
   echo "--- Checking Qt Plugins ---"
   if [ -d "$BUNDLE/Contents/PlugIns/geoservices" ]; then
     ...
   fi
   ```

   And remove the QtLocation/QtPositioning QML checks (lines 626-634):

   ```bash
   echo "--- Checking QML modules ---"
   for mod in QtLocation QtPositioning; do
     ...
   done
   ```

6. **Update Linux deployment verification** (lines 648-734). Remove the geoservices plugin check (lines 689-693):

   ```bash
   if [ -d "$APPDIR/usr/plugins/geoservices" ]; then
     ...
   fi
   ```

   And remove the QtLocation/QtPositioning QML checks (lines 694-697):

   ```bash
   for mod in QtLocation QtPositioning; do
     ...
   done
   ```

**Acceptance Criteria:**
- [ ] The `install-qt-action` modules list contains `qtwebengine qtwebchannel qtmultimedia` (no `qtlocation`, no `qtpositioning`)
- [ ] The CMake configure step passes `GOOGLE_MAPS_API_KEY` if the environment variable is set
- [ ] No geoservices-related checks remain in any verification step (Windows, macOS, Linux)
- [ ] No QtLocation or QtPositioning QML module checks remain in any verification step
- [ ] The "Debug Qt installation paths" step no longer checks for geoservices or QtLocation/QtPositioning
- [ ] The workflow file has no remaining references to "geoservices", "qtlocation", or "qtpositioning" (case-insensitive search)

**Complexity:** M

---

### Task 1.7: Update Documentation References

**Purpose:** Remove geoservices references from build system documentation so that the docs accurately reflect the new dependency set.

**Files to modify:**
- `docs/build-system-plan.md` -- remove the geoservices deployment example (lines ~330-337)

**Technical Approach:**

1. In `docs/build-system-plan.md`, locate the "Deploy Plugins Not Handled by Qt" subsection (lines ~326-337) under "Qt Plugin and QML Deployment". The entire subsection is about geoservices deployment:

   ```markdown
   ### Deploy Plugins Not Handled by Qt

   `qt_generate_deploy_app_script` may miss some plugins. Deploy manually:

   ```cmake
   # Geoservices plugins for map functionality
   if(EXISTS "${QT6_PLUGINS_DIR}/geoservices")
       install(DIRECTORY "${QT6_PLUGINS_DIR}/geoservices"
           DESTINATION "${PLUGIN_DEST}"
           FILES_MATCHING PATTERN "*.so" PATTERN "*.dylib" PATTERN "*.dll"
       )
   endif()
   ```
   ```

   Remove this subsection entirely, or replace it with a note that geoservices deployment is no longer needed since the map uses Google Maps via WebEngine. A reasonable replacement:

   ```markdown
   ### Deploy Plugins Not Handled by Qt

   `qt_generate_deploy_app_script` handles most standard plugins. If additional plugin categories are needed in the future, add them to the `ADDITIONAL_QT_PLUGIN_CATEGORIES` list in `src/CMakeLists.txt`. Geoservices plugins are no longer deployed since the map uses Google Maps via Qt WebEngine.
   ```

**Acceptance Criteria:**
- [ ] The geoservices CMake code example is removed from `docs/build-system-plan.md`
- [ ] A brief note explains that geoservices deployment is no longer needed
- [ ] No other docs content is changed unnecessarily

**Complexity:** S

---

## Testing Requirements

### Unit Tests

- No new unit tests are needed for this phase (build system changes are not unit-testable).
- Existing unit tests (if any) should still compile and link correctly after the dependency changes.

### Integration Tests

- **CMake configure test:** Run `cmake -B build -S src` with the WebEngine modules available and without Location/Positioning. Verify it configures without errors.
- **CMake configure with API key:** Run `cmake -B build -S src -DGOOGLE_MAPS_API_KEY="test_key_123"` and verify that `build/resources/map.html` contains "test_key_123".
- **CMake configure without API key:** Run `cmake -B build -S src` (no `-DGOOGLE_MAPS_API_KEY`) and verify that `build/resources/map.html` contains an empty string where the key would be.
- **Full build test:** Run `cmake --build build` and verify the application compiles and links successfully. The application will not have a working map yet (the HTML is a placeholder), but it must build without link errors.
- **Install test:** Run `cmake --install build` and verify that `map.html` is present in the install directory and that no geoservices plugins are deployed.

### Manual Verification

1. Run `cmake -B build -S src` on each target platform (Windows, macOS, Linux) and verify:
   - No CMake warnings about missing Location, Positioning, or geoservices modules
   - No CMake warnings about missing WebEngineWidgets or WebChannel
   - The `GOOGLE_MAPS_API_KEY` variable appears in CMake cache output
2. Run `cmake --build build` and verify the application links without errors.
3. Run `cmake --install build` and verify:
   - No `geoservices` directory in the install output
   - `resources/map.html` (or platform equivalent) exists in the install output
4. Push to a branch and verify CI passes on all three platforms.

## Notes for Implementer

### Gotchas

- **Qt WebEngine is a large module.** On some systems, it may not be installed by default with Qt. The CI step `install-qt-action` must explicitly list `qtwebengine` in its modules list. Locally, developers will need to install the Qt WebEngine component via the Qt Maintenance Tool or their package manager.
- **The `configure_file` call will fail if `src/resources/map.html.in` does not exist.** A placeholder file must be created as part of this phase. Do not skip creating it.
- **Task 1.1 and Task 1.2 should be done together** on line 83 (the `find_package` line) and lines 497-498 (the `target_link_libraries` block). Making them separately could lead to a broken intermediate state where Location is removed but WebEngine is not yet added (or vice versa). The acceptance criteria are written per-task for clarity, but implement them in a single edit pass.
- **The `qt_generate_deploy_qml_app_script` must remain** (not be downgraded to `qt_generate_deploy_app_script`) because the Video dock still uses QML (`VideoSurface.qml`). Only the comments referencing MapDock and QtLocation need updating.
- **On Linux CI, `qtwebengine` from `install-qt-action` may require additional system libraries** (e.g., `libnss3`, `libasound2`). If CI fails on Linux, add these to the `apt-get install` list. Check the Qt WebEngine documentation for required system dependencies.
- **The `ADDITIONAL_QT_PLUGIN_CATEGORIES` list** (lines 690-695 in `src/CMakeLists.txt`) has a commented-out `# tls` entry with the note "May be needed for HTTPS map tiles." Since Google Maps is loaded via WebEngine (which handles its own TLS), this comment could be misleading. Consider updating or removing it.

### Decisions Made

- **Placeholder `map.html.in` is created in this phase** rather than waiting for Phase 4. This is necessary so that `configure_file` does not fail during the build. The placeholder is intentionally minimal and will be fully replaced in Phase 4.
- **Tasks 1.1 and 1.2 are documented separately** for traceability (each maps to a specific section of the overview document), but they should be implemented as a single atomic edit to the `find_package` and `target_link_libraries` lines.
- **The install destination for `map.html` follows Option A from the overview** (filesystem loading, not qrc embedding). The file is installed to a `resources/` subdirectory relative to the executable on each platform, following the existing platform-specific install patterns established in lines 544-556 of `src/CMakeLists.txt`.
- **The `qtwebchannel` module is listed explicitly in the CI `install-qt-action` modules.** While `qtwebchannel` might be pulled in as a dependency of `qtwebengine` by some installers, listing it explicitly ensures it is always available.

### Open Questions

- **Linux CI system dependencies for WebEngine:** The `install-qt-action` for `qtwebengine` on Ubuntu may require additional system packages (e.g., `libnss3-dev`, `libasound2-dev`, `libxcomposite-dev`, `libxdamage-dev`). The implementer should verify this during the first CI run and add any missing packages to the Linux dependency installation step.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. CMake configures, builds, and installs successfully on all three platforms without Location/Positioning/geoservices
3. WebEngineWidgets and WebChannel are found and linked
4. The `GOOGLE_MAPS_API_KEY` cache variable works and `configure_file` produces the expected output
5. The `map.html.in` placeholder exists and is processed correctly
6. CI workflow passes on all three platforms with no geoservices warnings
7. No references to geoservices, QtLocation, or QtPositioning remain in CMake files, CI workflow, or build docs (except where explicitly noted as historical context)
8. Code follows patterns established in reference files (cache variable conventions, install rule patterns, CI step organization)
9. No TODOs or placeholder code remains (except the intentional `map.html.in` placeholder, which is documented)
