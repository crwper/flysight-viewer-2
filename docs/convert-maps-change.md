# Change Spec: Convert Map Dock from Qt Location to Google Maps

## Goal

Replace the Qt Location / OSM-based map in the Map dock with an embedded Google Maps instance rendered via `QWebEngineView` and the Google Maps JavaScript API. All existing user-visible features must be preserved. The C++ model layer (`TrackMapModel`, `MapCursorDotModel`, `MapCursorProxy`, `MapPreferencesBridge`) stays largely intact — only the view layer (currently QML) and its host widget change.

---

## Why

Qt Location's OSM plugin provides limited tile quality and no satellite imagery. Google Maps offers high-quality tiles, satellite/terrain/hybrid views, and a well-documented JavaScript API. Moving to `QWebEngineView` also eliminates the `QtLocation`, `QtPositioning`, and `QQuickWidget` dependencies from this dock (along with the geoservices plugin deployment headache).

---

## Architecture Overview

### Current stack

```
MapDockFeature
  └─ MapWidget (QWidget)
       └─ QQuickWidget  ──loads──▶  MapDock.qml (Qt Location Map)
            context properties: trackModel, mapCursorDots, mapCursorProxy, mapPreferences
```

### New stack

```
MapDockFeature
  └─ MapWidget (QWidget)
       └─ QWebEngineView  ──loads──▶  map.html (Google Maps JS API)
            communication: QWebChannel  ◄──►  MapBridge (QObject)
```

Key change: the QML file and `QQuickWidget` are replaced by an HTML/JS page and `QWebEngineView`. A new `MapBridge` QObject is exposed over `QWebChannel` so that C++ and JavaScript can call each other.

---

## Detailed Changes

### 1. New dependency: Qt WebEngine

**Add** `WebEngineWidgets` and `WebChannel` to the Qt `find_package` and link lists.

**File:** `src/CMakeLists.txt`
- Line 83: add `WebEngineWidgets WebChannel` to the `find_package(Qt… REQUIRED COMPONENTS …)` call.
- Lines 497-498: add `Qt${QT_VERSION_MAJOR}::WebEngineWidgets` and `Qt${QT_VERSION_MAJOR}::WebChannel` to `target_link_libraries`.

### 2. Remove dependency: Qt Location & Positioning

These modules are only used by the map dock. Once it no longer uses QML/Qt Location, they can be removed.

**File:** `src/CMakeLists.txt`
- Line 83: remove `Location Positioning` from `find_package`.
- Lines 497-498: remove `Qt${QT_VERSION_MAJOR}::Location` and `Qt${QT_VERSION_MAJOR}::Positioning`.

Additionally check whether `QuickWidgets`, `Quick`, and `Qml` are still needed by other code. They **are** — the Video dock (`VideoWidget.cpp`) uses `QQuickWidget` — so leave those in place.

### 3. Remove geoservices plugin deployment

The entire "Geoservices Plugin Deployment" section in `src/CMakeLists.txt` (lines ~633-679) can be deleted. The geoservices verification in `cmake/QtPathDiscovery.cmake` (lines ~175-180) can also be removed or made a no-op. Corresponding checks in `.github/workflows/build.yml` should be removed.

### 4. Delete `src/qml/MapDock.qml`

This file is entirely replaced by the HTML/JS page. Remove it from `src/resources.qrc` as well (line 3). The `VideoSurface.qml` entry stays.

### 5. Create `src/resources/map.html`

The source template is `src/resources/map.html.in`. CMake's `configure_file` substitutes `@GOOGLE_MAPS_API_KEY@` to produce the build output `map.html` (see section 5a).

The file:
- Loads the Google Maps JavaScript API via `<script src="https://maps.googleapis.com/maps/api/js?key=@GOOGLE_MAPS_API_KEY@">`.
- Creates a `google.maps.Map` instance filling the viewport.
- Connects to the C++ side via `QWebChannel` (include `qrc:///qtwebchannel/qwebchannel.js`).
- Exposes JS functions that the C++ side calls (via `QWebEnginePage::runJavaScript` or through the bridge object) and calls C++ slots through the bridge.
- If the API key placeholder is empty, shows an error message instead of loading the Maps API.

#### 5a. Google Maps API key

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

### 6. Rewrite `MapWidget` (`MapWidget.h` / `MapWidget.cpp`)

Replace the `QQuickWidget` + QML context-property setup with:

1. Create a `QWebEngineView` as the child widget (same layout pattern).
2. Create a `QWebChannel` and register a `MapBridge` object on it (see below).
3. Set `page()->setWebChannel(channel)`.
4. Load `qrc:/resources/map.html`.
5. Keep the existing member pointers to `TrackMapModel`, `MapCursorDotModel`, `MapCursorProxy`, and `MapPreferencesBridge` — they are still the data sources.
6. Connect model signals to bridge methods that push data to JS.

Remove `#include <QQuickWidget>`, `#include <QQmlContext>`, `#include <QQmlEngine>`. Remove the QML import-path setup line.

### 7. New class: `MapBridge` (QObject for QWebChannel)

Create `src/ui/docks/map/MapBridge.h` and `MapBridge.cpp`.

This object is the sole communication channel between C++ and the JS running in `QWebEngineView`. It is registered on the `QWebChannel` so that JS can call its slots/invokables, and C++ can emit signals that JS receives.

#### 7a. C++ → JS (signals that JS listens to)

| Signal | Payload | Purpose |
|--------|---------|---------|
| `tracksChanged(QJsonArray tracks)` | Array of `{ sessionId, points: [{lat, lon, t}…], color }` | Full track rebuild. Emitted when `TrackMapModel` resets. |
| `cursorDotsChanged(QJsonArray dots)` | Array of `{ sessionId, lat, lon, color }` | Cursor dot rebuild. Emitted when `MapCursorDotModel` resets. |
| `fitBounds(double south, double west, double north, double east)` | Bounding box | Auto-zoom to tracks. Emitted when `TrackMapModel::boundsChanged`. |
| `preferenceChanged(QString key, QVariant value)` | key + value | Forward individual preference changes (line thickness, marker size, opacity, map type). |

Internally, `MapBridge` connects to the existing models' signals and converts the model data to JSON before emitting. `TrackMapModel` already stores `QVariantList` points — convert to `QJsonArray`. `MapCursorDotModel` is small (a few dots at most) — serialize the full list each time.

#### 7b. JS → C++ (invokable slots)

| Slot | Parameters | Purpose |
|------|------------|---------|
| `onMapHover(QString sessionId, double utcSeconds)` | session + time | JS detected a hover near a polyline. Forwards to `MapCursorProxy::setMapHover`. |
| `onMapHoverClear()` | — | JS hover ended. Forwards to `MapCursorProxy::clearMapHover`. |
| `onMapTypeChanged(int index)` | index | User selected a different map type in JS. Forwards to `MapPreferencesBridge::setMapTypeIndex`. |

### 8. JavaScript implementation (inside `map.html`)

The JS side must implement the following features, replacing the QML equivalents.

#### 8a. Map initialization

- Create `new google.maps.Map(document.getElementById("map"), { … })`.
- Default zoom: 12, center: (0, 0).
- Disable default UI controls except zoom buttons (the map type control is handled by the app — see 8g).
- Set initial map type from preferences (received during init handshake).

#### 8b. Track polylines

- Maintain a `Map<string, google.maps.Polyline>` keyed by sessionId.
- On `tracksChanged`: clear all existing polylines, create new ones.
- Each polyline: `path` from the points array, `strokeColor` from `color`, `strokeWeight` from preference `lineThickness`, `strokeOpacity` from preference `trackOpacity`.
- Store the points array (including `t` values) on each polyline object for hover-time lookup.

#### 8c. Cursor dot markers

- Maintain a `Map<string, google.maps.Marker>` (or `google.maps.marker.AdvancedMarkerElement`) keyed by sessionId.
- On `cursorDotsChanged`: clear existing markers, create new ones.
- Each marker: positioned at `{lat, lng: lon}`, icon is a colored circle with white border (use an SVG symbol or a `google.maps.Symbol` with `path: google.maps.SymbolPath.CIRCLE`).
- Size from preference `markerSize`.

#### 8d. Hover detection on polylines

This is the most complex piece. The current QML does brute-force screen-space distance checking against all polyline segments at ~60 Hz. The JS implementation should:

1. Listen to `mousemove` on the map.
2. For each mousemove, convert the mouse LatLng to a pixel position and check distance to each polyline segment in pixel space (same algorithm as the current QML `_distPointToSegment2` / `_updateHoverAt`).
3. Throttle to ~60 Hz (use `requestAnimationFrame` or a 16 ms timer).
4. Apply the same 10px threshold and 2px minimum-movement filter.
5. When a hit is found, interpolate the UTC time from the two bracketing track points and call `bridge.onMapHover(sessionId, utcSeconds)`.
6. When no hit, call `bridge.onMapHoverClear()`.

The `google.maps.Projection` from `map.getProjection()` plus `fromLatLngToPoint` can convert coordinates to world-pixel space. Then scale by `2^zoom` to get screen pixels. Alternatively, use the `OverlayView` trick to get a `MapCanvasProjection` with `fromLatLngToContainerPixel`.

#### 8e. Fit-to-bounds

On `fitBounds(s, w, n, e)`:
```js
map.fitBounds(new google.maps.LatLngBounds({lat: s, lng: w}, {lat: n, lng: e}), 40);
```
The `40` is padding in pixels, matching the current `fitViewportToGeoShape(bounds, 40)`.

#### 8f. Pan / zoom gestures

Google Maps handles drag-to-pan, pinch-to-zoom, and scroll-wheel zoom natively. No custom gesture code is needed. The current QML custom `DragHandler`, `PinchHandler`, and `onWheel` logic are all replaced by built-in Google Maps behavior (which already keeps the point under the cursor stationary during wheel zoom).

#### 8g. Map type selector

The current QML draws a custom dropdown overlay. Two options for the Google Maps version:

**Option A (recommended):** Use Google Maps' built-in `MapTypeControlOptions` with `style: google.maps.MapTypeControlStyle.DROPDOWN_MENU` positioned at `TOP_RIGHT`. This gives users the standard Google Maps experience and requires zero custom UI code.

**Option B:** Replicate the current custom dropdown in HTML/CSS overlaid on the map. This preserves the exact current look but is more work.

Either way, when the user changes the map type, persist it via `bridge.onMapTypeChanged(index)` → `MapPreferencesBridge::setMapTypeIndex`. On startup, the C++ side should send the saved map type to JS.

#### 8h. Preference reactivity

When JS receives `preferenceChanged(key, value)` from the bridge:
- `map/lineThickness`: update `strokeWeight` on all polylines.
- `map/trackOpacity`: update `strokeOpacity` on all polylines.
- `map/markerSize`: update marker icon scale.
- `map/type`: call `map.setMapTypeId(…)`. Map the integer index to Google Maps type IDs: 0→`roadmap`, 1→`satellite`, 2→`terrain`, 3→`hybrid` (adjust mapping as appropriate — the old OSM plugin had different type names).

### 9. Changes to `TrackMapModel`

Minimal changes:
- **Remove** `#include <QGeoCoordinate>` and `#include <QGeoRectangle>`.
- **Replace** the `center` and `bounds` Q_PROPERTYs (currently `QGeoCoordinate` and `QGeoRectangle`) with simple doubles: `boundsNorth`, `boundsSouth`, `boundsEast`, `boundsWest`, `centerLat`, `centerLon`. These are easier to pass over `QWebChannel` (no need to register custom QML types). The `MapBridge` reads these when emitting `fitBounds`.
- The model roles (`sessionId`, `trackPoints`, `trackColor`) and rebuild logic stay the same. `MapBridge` iterates the model rows and serializes to JSON.

### 10. Changes to `MapCursorDotModel`

No changes needed. `MapBridge` reads the model data and serializes it to JSON for JS.

### 11. Changes to `MapCursorProxy`

No changes needed. `MapBridge` calls its `setMapHover` / `clearMapHover` methods directly.

### 12. Changes to `MapPreferencesBridge`

- The `mapTypeIndex` property currently maps to OSM plugin type indices. With Google Maps, the mapping is different. Update the default and document the new mapping (roadmap=0, satellite=1, terrain=2, hybrid=3). Existing users' saved preference will be reset to 0 on first launch since the old indices won't match — this is acceptable.
- The QML-facing Q_PROPERTYs are no longer consumed by QML. However, `MapBridge` can read them directly. No structural change needed; just remove the `Q_PROPERTY` macros if desired (they're harmless to keep).

### 13. Changes to `MapSettingsPage`

No changes. The preferences UI is purely C++ widgets and doesn't reference Qt Location or QML.

### 14. Changes to `MapDockFeature`

No changes. It creates a `MapWidget` and puts it in a dock — that interface is unchanged.

### 15. Changes to `preferencekeys.h`

No new keys needed (the API key is a build-time variable, not a preference). Optionally update the comment on `MapType` to note the new index mapping (roadmap=0, satellite=1, terrain=2, hybrid=3).

### 16. `resources.qrc` and `configure_file` setup

The HTML template lives at `src/resources/map.html.in` (committed to the repo). CMake processes it:

```cmake
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/map.html.in"
    "${CMAKE_CURRENT_BINARY_DIR}/resources/map.html"
    @ONLY
)
```

The generated `map.html` must be reachable by `QWebEngineView`. Two options:

**Option A (recommended):** Load it directly from the filesystem at runtime:

```cpp
view->setUrl(QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/resources/map.html"));
```

And install the configured file alongside the binary. This avoids qrc entirely for this file.

**Option B:** Use a build-directory-aware `.qrc` file generated by CMake so the configured `map.html` is embedded. This is more complex and less necessary.

Either way, update `src/resources.qrc` to remove the `qml/MapDock.qml` entry:

```xml
<RCC>
    <qresource prefix="/">
        <file>qml/VideoSurface.qml</file>
        <file>resources/icons/FlySightViewer.png</file>
    </qresource>
</RCC>
```

Delete the file `src/qml/MapDock.qml`.

---

## Files Changed (Summary)

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
| `src/ui/docks/map/MapCursorDotModel.h` | No change | |
| `src/ui/docks/map/MapCursorDotModel.cpp` | No change | |
| `src/ui/docks/map/MapCursorProxy.h` | No change | |
| `src/ui/docks/map/MapCursorProxy.cpp` | No change | |
| `src/ui/docks/map/MapPreferencesBridge.h` | Minor edit | Update map type index comment/mapping |
| `src/ui/docks/map/MapPreferencesBridge.cpp` | No change | |
| `src/ui/docks/map/MapDockFeature.h` | No change | |
| `src/ui/docks/map/MapDockFeature.cpp` | No change | |
| `src/preferences/preferencekeys.h` | No change | |
| `src/preferences/mapsettingspage.h` | No change | |
| `src/preferences/mapsettingspage.cpp` | No change | |
| `cmake/QtPathDiscovery.cmake` | Edit | Remove geoservices verification |
| `.github/workflows/build.yml` | Edit | Remove geoservices checks; pass `GOOGLE_MAPS_API_KEY` to CMake configure |
| `docs/build-system-plan.md` | Edit | Remove geoservices references |
| `README.md` | Edit | Remove geoservices from deployment directory trees |

---

## Out of Scope

- **Video dock** — uses QQuickWidget/QML independently; not affected.
- **Plot docks, legend, session model** — no changes.
- **Python scripting bridge** — no changes.
- **macOS/Linux deployment scripts** — only geoservices removal; no new deployment needs (WebEngine is handled by `qt_generate_deploy_app_script`).

---

## Testing Checklist

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
