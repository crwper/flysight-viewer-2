# Implementation Plan: Convert Map Dock from Qt Location to Google Maps

## Feature Specification

Replace the Qt Location / OSM-based map in the Map dock with an embedded Google Maps instance rendered via `QWebEngineView` and the Google Maps JavaScript API. All existing user-visible features must be preserved. The C++ model layer (`TrackMapModel`, `MapCursorDotModel`, `MapCursorProxy`, `MapPreferencesBridge`) stays largely intact — only the view layer (currently QML) and its host widget change.

### Why

Qt Location's OSM plugin provides limited tile quality and no satellite imagery. Google Maps offers high-quality tiles, satellite/terrain/hybrid views, and a well-documented JavaScript API. Moving to `QWebEngineView` also eliminates the `QtLocation`, `QtPositioning`, and `QQuickWidget` dependencies from this dock (along with the geoservices plugin deployment headache).

### Architecture Overview

**Current stack:**
```
MapDockFeature
  └─ MapWidget (QWidget)
       └─ QQuickWidget  ──loads──▶  MapDock.qml (Qt Location Map)
            context properties: trackModel, mapCursorDots, mapCursorProxy, mapPreferences
```

**New stack:**
```
MapDockFeature
  └─ MapWidget (QWidget)
       └─ QWebEngineView  ──loads──▶  map.html (Google Maps JS API)
            communication: QWebChannel  ◄──►  MapBridge (QObject)
```

Key change: the QML file and `QQuickWidget` are replaced by an HTML/JS page and `QWebEngineView`. A new `MapBridge` QObject is exposed over `QWebChannel` so that C++ and JavaScript can call each other.

### Detailed Changes

#### 1. New dependency: Qt WebEngine

**Add** `WebEngineWidgets` and `WebChannel` to the Qt `find_package` and link lists.

**File:** `src/CMakeLists.txt`
- Line 83: add `WebEngineWidgets WebChannel` to the `find_package(Qt… REQUIRED COMPONENTS …)` call.
- Lines 497-498: add `Qt${QT_VERSION_MAJOR}::WebEngineWidgets` and `Qt${QT_VERSION_MAJOR}::WebChannel` to `target_link_libraries`.

#### 2. Remove dependency: Qt Location & Positioning

These modules are only used by the map dock. Once it no longer uses QML/Qt Location, they can be removed.

**File:** `src/CMakeLists.txt`
- Line 83: remove `Location Positioning` from `find_package`.
- Lines 497-498: remove `Qt${QT_VERSION_MAJOR}::Location` and `Qt${QT_VERSION_MAJOR}::Positioning`.

Additionally check whether `QuickWidgets`, `Quick`, and `Qml` are still needed by other code. They **are** — the Video dock (`VideoWidget.cpp`) uses `QQuickWidget` — so leave those in place.

#### 3. Remove geoservices plugin deployment

The entire "Geoservices Plugin Deployment" section in `src/CMakeLists.txt` (lines ~633-679) can be deleted. The geoservices verification in `cmake/QtPathDiscovery.cmake` (lines ~175-196) can also be removed or made a no-op. Corresponding checks in `.github/workflows/build.yml` should be removed.

#### 4. Delete `src/qml/MapDock.qml`

This file is entirely replaced by the HTML/JS page. Remove it from `src/resources.qrc` as well (line 3). The `VideoSurface.qml` entry stays.

#### 5. Create `src/resources/map.html`

The source template is `src/resources/map.html.in`. CMake's `configure_file` substitutes `@GOOGLE_MAPS_API_KEY@` to produce the build output `map.html` (see section 5a).

The file:
- Loads the Google Maps JavaScript API via `<script src="https://maps.googleapis.com/maps/api/js?key=@GOOGLE_MAPS_API_KEY@">`.
- Creates a `google.maps.Map` instance filling the viewport.
- Connects to the C++ side via `QWebChannel` (include `qrc:///qtwebchannel/qwebchannel.js`).
- Exposes JS functions that the C++ side calls (via `QWebEnginePage::runJavaScript` or through the bridge object) and calls C++ slots through the bridge.
- If the API key placeholder is empty, shows an error message instead of loading the Maps API.

##### 5a. Google Maps API key

The API key is a build-time CMake cache variable. It gets embedded into the HTML at configure time.

In `src/CMakeLists.txt`, define:
```cmake
set(GOOGLE_MAPS_API_KEY "" CACHE STRING "Google Maps JavaScript API key")
```

Then use `configure_file` to process `map.html.in` → `map.html`, replacing `@GOOGLE_MAPS_API_KEY@` in the `<script>` tag. The configured output goes into `${CMAKE_CURRENT_BINARY_DIR}/resources/map.html` and is added to the Qt resource file from there (or loaded at runtime from the build directory — see section 16 for the resource file update).

The key is **not** committed to the repository. Developers pass it at configure time:
```bash
cmake -DGOOGLE_MAPS_API_KEY="AIza…" …
```

If the variable is empty at build time, the map should display an error message in the HTML (e.g., a centered `<div>` saying "Google Maps API key not configured") instead of attempting to load the Maps API.

#### 6. Rewrite `MapWidget` (`MapWidget.h` / `MapWidget.cpp`)

Replace the `QQuickWidget` + QML context-property setup with:

1. Create a `QWebEngineView` as the child widget (same layout pattern).
2. Create a `QWebChannel` and register a `MapBridge` object on it (see below).
3. Set `page()->setWebChannel(channel)`.
4. Load `qrc:/resources/map.html`.
5. Keep the existing member pointers to `TrackMapModel`, `MapCursorDotModel`, `MapCursorProxy`, and `MapPreferencesBridge` — they are still the data sources.
6. Connect model signals to bridge methods that push data to JS.

Remove `#include <QQuickWidget>`, `#include <QQmlContext>`, `#include <QQmlEngine>`. Remove the QML import-path setup line.

#### 7. New class: `MapBridge` (QObject for QWebChannel)

Create `src/ui/docks/map/MapBridge.h` and `MapBridge.cpp`.

This object is the sole communication channel between C++ and the JS running in `QWebEngineView`. It is registered on the `QWebChannel` so that JS can call its slots/invokables, and C++ can emit signals that JS receives.

##### 7a. C++ → JS (signals that JS listens to)

| Signal | Payload | Purpose |
|--------|---------|---------|
| `tracksChanged(QJsonArray tracks)` | Array of `{ sessionId, points: [{lat, lon, t}…], color }` | Full track rebuild. Emitted when `TrackMapModel` resets. |
| `cursorDotsChanged(QJsonArray dots)` | Array of `{ sessionId, lat, lon, color }` | Cursor dot rebuild. Emitted when `MapCursorDotModel` resets. |
| `fitBounds(double south, double west, double north, double east)` | Bounding box | Auto-zoom to tracks. Emitted when `TrackMapModel::boundsChanged`. |
| `preferenceChanged(QString key, QVariant value)` | key + value | Forward individual preference changes (line thickness, marker size, opacity, map type). |

##### 7b. JS → C++ (invokable slots)

| Slot | Parameters | Purpose |
|------|------------|---------|
| `onMapHover(QString sessionId, double utcSeconds)` | session + time | JS detected a hover near a polyline. Forwards to `MapCursorProxy::setMapHover`. |
| `onMapHoverClear()` | — | JS hover ended. Forwards to `MapCursorProxy::clearMapHover`. |
| `onMapTypeChanged(int index)` | index | User selected a different map type in JS. Forwards to `MapPreferencesBridge::setMapTypeIndex`. |

#### 8. JavaScript implementation (inside `map.html`)

The JS side must implement the following features, replacing the QML equivalents.

##### 8a. Map initialization
- Create `new google.maps.Map(document.getElementById("map"), { … })`.
- Default zoom: 12, center: (0, 0).
- Disable default UI controls except zoom buttons (the map type control is handled by the app — see 8g).
- Set initial map type from preferences (received during init handshake).

##### 8b. Track polylines
- Maintain a `Map<string, google.maps.Polyline>` keyed by sessionId.
- On `tracksChanged`: clear all existing polylines, create new ones.
- Each polyline: `path` from the points array, `strokeColor` from `color`, `strokeWeight` from preference `lineThickness`, `strokeOpacity` from preference `trackOpacity`.
- Store the points array (including `t` values) on each polyline object for hover-time lookup.

##### 8c. Cursor dot markers
- Maintain a `Map<string, google.maps.Marker>` (or `google.maps.marker.AdvancedMarkerElement`) keyed by sessionId.
- On `cursorDotsChanged`: clear existing markers, create new ones.
- Each marker: positioned at `{lat, lng: lon}`, icon is a colored circle with white border (use an SVG symbol or a `google.maps.Symbol` with `path: google.maps.SymbolPath.CIRCLE`).
- Size from preference `markerSize`.

##### 8d. Hover detection on polylines
1. Listen to `mousemove` on the map.
2. For each mousemove, convert the mouse LatLng to a pixel position and check distance to each polyline segment in pixel space (same algorithm as the current QML `_distPointToSegment2` / `_updateHoverAt`).
3. Throttle to ~60 Hz (use `requestAnimationFrame` or a 16 ms timer).
4. Apply the same 10px threshold and 2px minimum-movement filter.
5. When a hit is found, interpolate the UTC time from the two bracketing track points and call `bridge.onMapHover(sessionId, utcSeconds)`.
6. When no hit, call `bridge.onMapHoverClear()`.

##### 8e. Fit-to-bounds
On `fitBounds(s, w, n, e)`:
```js
map.fitBounds(new google.maps.LatLngBounds({lat: s, lng: w}, {lat: n, lng: e}), 40);
```

##### 8f. Pan / zoom gestures
Google Maps handles drag-to-pan, pinch-to-zoom, and scroll-wheel zoom natively. No custom gesture code is needed.

##### 8g. Map type selector
Use Google Maps' built-in `MapTypeControlOptions` with `style: google.maps.MapTypeControlStyle.DROPDOWN_MENU` positioned at `TOP_RIGHT`. When the user changes the map type, persist it via `bridge.onMapTypeChanged(index)` → `MapPreferencesBridge::setMapTypeIndex`. On startup, the C++ side should send the saved map type to JS.

##### 8h. Preference reactivity
When JS receives `preferenceChanged(key, value)` from the bridge:
- `map/lineThickness`: update `strokeWeight` on all polylines.
- `map/trackOpacity`: update `strokeOpacity` on all polylines.
- `map/markerSize`: update marker icon scale.
- `map/type`: call `map.setMapTypeId(…)`. Map the integer index to Google Maps type IDs: 0→`roadmap`, 1→`satellite`, 2→`terrain`, 3→`hybrid`.

#### 9. Changes to `TrackMapModel`
- **Remove** `#include <QGeoCoordinate>` and `#include <QGeoRectangle>`.
- **Replace** the `center` and `bounds` Q_PROPERTYs (currently `QGeoCoordinate` and `QGeoRectangle`) with simple doubles: `boundsNorth`, `boundsSouth`, `boundsEast`, `boundsWest`, `centerLat`, `centerLon`.
- The model roles (`sessionId`, `trackPoints`, `trackColor`) and rebuild logic stay the same.

#### 10. Changes to `MapCursorDotModel`
No changes needed. `MapBridge` reads the model data and serializes it to JSON for JS.

#### 11. Changes to `MapCursorProxy`
No changes needed. `MapBridge` calls its `setMapHover` / `clearMapHover` methods directly.

#### 12. Changes to `MapPreferencesBridge`
- Update the default map type comment and document the new mapping (roadmap=0, satellite=1, terrain=2, hybrid=3).
- QML-facing Q_PROPERTYs are no longer consumed by QML but `MapBridge` can read them directly. No structural change needed.

#### 13-15. No changes to MapSettingsPage, MapDockFeature, preferencekeys.h

#### 16. `resources.qrc` and `configure_file` setup
- The HTML template lives at `src/resources/map.html.in` (committed to the repo).
- CMake processes it with `configure_file`.
- Load from filesystem at runtime via `QUrl::fromLocalFile`.
- Update `src/resources.qrc` to remove the `qml/MapDock.qml` entry.
- Delete the file `src/qml/MapDock.qml`.

### Files Changed (Summary)

| File | Action | Notes |
|------|--------|-------|
| `src/CMakeLists.txt` | Edit | Add `GOOGLE_MAPS_API_KEY` cache var; add `configure_file` for map.html.in; add WebEngineWidgets + WebChannel; remove Location + Positioning; remove geoservices deployment block |
| `src/resources.qrc` | Edit | Remove MapDock.qml entry |
| `src/qml/MapDock.qml` | **Delete** | Replaced by map.html |
| `src/resources/map.html.in` | **Create** | Google Maps JS API page template (`@GOOGLE_MAPS_API_KEY@` placeholder) |
| `src/ui/docks/map/MapWidget.h` | Edit | Replace QQuickWidget with QWebEngineView + QWebChannel |
| `src/ui/docks/map/MapWidget.cpp` | Edit | Rewrite to set up WebEngine + bridge; load configured map.html |
| `src/ui/docks/map/MapBridge.h` | **Create** | QWebChannel bridge object |
| `src/ui/docks/map/MapBridge.cpp` | **Create** | Bridge implementation |
| `src/ui/docks/map/TrackMapModel.h` | Edit | Replace QGeoCoordinate/QGeoRectangle with plain doubles |
| `src/ui/docks/map/TrackMapModel.cpp` | Edit | Update bounds computation to use plain doubles |
| `src/ui/docks/map/MapPreferencesBridge.h` | Minor edit | Update map type index comment/mapping |
| `cmake/QtPathDiscovery.cmake` | Edit | Remove geoservices verification |
| `.github/workflows/build.yml` | Edit | Remove geoservices checks; pass `GOOGLE_MAPS_API_KEY` to CMake configure |
| `docs/build-system-plan.md` | Edit | Remove geoservices references |
| `README.md` | Edit | Remove geoservices from deployment directory trees |

### Testing Checklist

1. Map loads and shows tiles with a valid API key.
2. Map shows a helpful message when no API key is configured.
3. Track polylines render for all visible sessions with correct colors.
4. Polylines update when sessions are added, removed, or hidden.
5. Polylines update when the plot range changes (time filtering).
6. Cursor dots appear at the correct interpolated position when hovering the map.
7. Cursor dots appear when moving cursors in plot docks (bidirectional sync).
8. Hover detection works: moving the mouse near a polyline highlights the cursor.
9. Hover clears when the mouse moves away from all polylines.
10. Map auto-zooms to fit all tracks on data load.
11. Map type selector works and persists across sessions.
12. Line thickness, marker size, and track opacity preferences update the map live.
13. Reset to Defaults in the settings page works.
14. Pan (drag), zoom (wheel), and pinch gestures work naturally.
15. Build succeeds on Windows, macOS, and Linux.
16. CI pipeline passes (no geoservices warnings).

### Out of Scope

- **Video dock** — uses QQuickWidget/QML independently; not affected.
- **Plot docks, legend, session model** — no changes.
- **Python scripting bridge** — no changes.
- **macOS/Linux deployment scripts** — only geoservices removal; no new deployment needs.

---

## Phases

| Phase | Name | Purpose | Dependencies |
|-------|------|---------|--------------|
| 1 | Build System & Dependencies | Add WebEngine deps, remove Location deps, set up API key and configure_file, remove geoservices deployment | None |
| 2 | Model Layer Refactor | Replace QGeoCoordinate/QGeoRectangle in TrackMapModel with plain doubles | None |
| 3 | MapBridge & MapWidget Rewrite | Create the QWebChannel bridge object and rewrite MapWidget to use QWebEngineView | Phase 1, Phase 2 |
| 4 | Google Maps HTML/JS Implementation | Create map.html.in with full Google Maps JS API features | Phase 3 |
| 5 | Cleanup & CI Updates | Delete old QML, update resources.qrc, update CI workflow, update docs | Phase 1, Phase 4 |

## Dependency Graph

```
Phase 1 (Build System) ──────────────┐
                                      ├──▶ Phase 3 (MapBridge & MapWidget) ──▶ Phase 4 (HTML/JS) ──▶ Phase 5 (Cleanup & CI)
Phase 2 (Model Layer) ───────────────┘
```

Phases 1 and 2 can be documented (and implemented) in parallel. Phase 3 depends on both. Phase 4 depends on Phase 3. Phase 5 depends on Phase 4 (and transitively Phase 1).

## Key Patterns & References

### Map Dock Core
- `src/ui/docks/map/MapWidget.h` — Current QQuickWidget host; pattern for widget setup and model wiring
- `src/ui/docks/map/MapWidget.cpp` — Constructor pattern: create child widget, set context properties, load QML
- `src/ui/docks/map/MapDockFeature.h` — DockFeature subclass; creates MapWidget with AppContext
- `src/ui/docks/map/MapDockFeature.cpp` — Shows how MapWidget receives sessionModel, cursorModel, rangeModel

### Data Models
- `src/ui/docks/map/TrackMapModel.h` — QAbstractListModel with Track struct, QGeoCoordinate/QGeoRectangle properties
- `src/ui/docks/map/TrackMapModel.cpp` — rebuild() logic, bounds computation, color generation from sessionId hash
- `src/ui/docks/map/MapCursorDotModel.h` — QAbstractListModel with Dot struct (sessionId, lat, lon, color)
- `src/ui/docks/map/MapCursorDotModel.cpp` — rebuild() logic, effective cursor selection, lat/lon interpolation via sampleLatLonAtUtc
- `src/ui/docks/map/MapCursorProxy.h` — Q_INVOKABLE setMapHover/clearMapHover methods
- `src/ui/docks/map/MapCursorProxy.cpp` — Cursor model interaction: setCursorTargetsExplicit, setCursorPositionUtc, setCursorActive

### Preferences
- `src/ui/docks/map/MapPreferencesBridge.h` — Q_PROPERTY declarations: lineThickness, markerSize, trackOpacity, mapTypeIndex
- `src/ui/docks/map/MapPreferencesBridge.cpp` — Preference read/write/change pattern, setMapTypeIndex implementation
- `src/preferences/preferencekeys.h` — MapLineThickness, MapMarkerSize, MapTrackOpacity, MapType key definitions
- `src/preferences/mapsettingspage.h` — Settings page structure (no changes needed, but shows preference keys in use)
- `src/preferences/mapsettingspage.cpp` — Spin boxes, sliders, reset button wiring

### QML (being replaced)
- `src/qml/MapDock.qml` — Full QML implementation: Plugin{name:"osm"}, MapPolyline rendering, MapQuickItem cursor dots, hover detection algorithm (_updateHoverAt, _distPointToSegment2), map type selector overlay, DragHandler/PinchHandler/onWheel gesture code

### Build System
- `src/CMakeLists.txt` — find_package (line 83), target_link_libraries (lines ~492-500), geoservices deployment (lines ~633-679), resources.qrc usage
- `cmake/QtPathDiscovery.cmake` — Geoservices/QtLocation/QtPositioning verification (lines ~175-196)
- `src/resources.qrc` — Current resource entries: MapDock.qml, VideoSurface.qml, FlySightViewer.png

### CI & Documentation
- `.github/workflows/build.yml` — Qt module installation (qtlocation, qtpositioning at line ~217-218), geoservices verification (lines ~268-274, ~493-498, ~610-624, ~689-693)
- `docs/build-system-plan.md` — Geoservices plugin deployment pattern (lines ~331-337)
- `README.md` — Deployment directory trees with geoservices entries (lines ~440, ~464, ~491)

### Video Dock (reference — not modified)
- `src/ui/docks/video/VideoWidget.cpp` — Uses QQuickWidget (confirms Quick/Qml/QuickWidgets must stay in CMake)

## Decisions & Constraints

- **Qt WebEngine is required.** This is a heavyweight dependency (~100MB on some platforms) but is the only way to embed a full browser engine for the Google Maps JS API.
- **QuickWidgets, Quick, and Qml remain in CMake** — the Video dock still uses `QQuickWidget`. Only Location and Positioning are removed.
- **API key is build-time, not runtime** — embedded via `configure_file` into HTML. Not stored as a preference. This means users cannot change the key without rebuilding.
- **Load map.html from filesystem (Option A)** — simpler than embedding in qrc; avoids needing a build-dir-aware .qrc. The configured file is installed alongside the binary.
- **Use Google Maps built-in map type control (Option A)** — saves custom UI effort; standard UX.
- **Map type index mapping changes** — old OSM indices won't match Google Maps types. Existing saved preferences will map to `roadmap` (index 0) on first launch, which is acceptable.
- **Hover detection port** — the brute-force pixel-distance algorithm from QML must be faithfully ported to JS. The Google Maps `Projection` API or `OverlayView` trick provides coordinate-to-pixel conversion.

---

## Planning Complete

### Structure
- Phases: 5
- Total tasks: 27 across all phases
- Parallel tracks possible: Phase 1 and Phase 2 can be implemented simultaneously (no dependencies between them). Phases 3→4→5 are sequential.

### Coverage
All requirements from the original feature specification are addressed:
- Build system changes: Phase 1 (7 tasks)
- Model layer refactor: Phase 2 (3 tasks)
- Bridge + widget rewrite: Phase 3 (5 tasks)
- Full Google Maps JS implementation: Phase 4 (9 tasks)
- Cleanup and documentation: Phase 5 (3 tasks)

Phase 4 identified one cross-phase addition: a `requestInitialData()` handshake mechanism must be added to `MapBridge` (created in Phase 3) to handle the timing issue where JS misses the initial data push. This is fully documented in Phase 4, Task 4.2.

One open question remains: `QWebEngineSettings::LocalContentCanAccessRemoteUrls` may need to be enabled for `QWebEngineView` to load Google Maps from a local HTML file. This should be tested during Phase 3/4 implementation and addressed if needed.

### Ready for Implementation
The plan is ready for handoff to the implementation orchestrator.
