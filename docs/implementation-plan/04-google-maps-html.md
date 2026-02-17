# Phase 4: Google Maps HTML/JS Implementation

## Overview

This phase creates the `src/resources/map.html.in` template file containing the full Google Maps JavaScript API implementation. The file replaces the Phase 1 placeholder with a complete Google Maps view that renders track polylines, cursor dot markers, hover detection, fit-to-bounds, map type control, and preference reactivity -- all communicating with the C++ side through the `MapBridge` object registered on `QWebChannel` by Phase 3. After this phase, the map dock is fully functional.

## Dependencies

- **Depends on:** Phase 3 (MapBridge & MapWidget Rewrite -- provides the `MapBridge` registered on `QWebChannel` as `"bridge"`, the signal/slot API, and the `QWebEngineView` loading `map.html`)
- **Blocks:** Phase 5 (Cleanup & CI Updates)
- **Assumptions:**
  - After Phase 1, `src/resources/map.html.in` exists as a placeholder and is processed by `configure_file` with `@ONLY`, substituting `@GOOGLE_MAPS_API_KEY@`. The output is `${CMAKE_CURRENT_BINARY_DIR}/resources/map.html` and is installed alongside the binary.
  - After Phase 3, `MapBridge` is registered on the `QWebChannel` under the string key `"bridge"`. The bridge emits four signals toward JS (`tracksChanged`, `cursorDotsChanged`, `fitBounds`, `preferenceChanged`) and exposes three invokable slots for JS to call (`onMapHover`, `onMapHoverClear`, `onMapTypeChanged`).
  - After Phase 3, `MapWidget` connects model resets and preference changes to `MapBridge::push*` methods, but the initial data push is missed by JS because the page is not yet loaded when those signals fire. This phase must implement an initialization handshake to request initial data.
  - Colors from the bridge are serialized as `#AARRGGBB` (Qt's `QColor::HexArgb` format), not CSS-standard `#RRGGBBAA`. JS must parse this format.
  - Track data arrives as `QJsonArray` of `{sessionId: string, points: [{lat, lon, t}], color: "#AARRGGBB"}`.
  - Cursor dot data arrives as `QJsonArray` of `{sessionId: string, lat: double, lon: double, color: "#AARRGGBB"}`.

## Tasks

### Task 4.1: HTML Skeleton and API Key Gating

**Purpose:** Establish the HTML document structure, load the Google Maps JavaScript API (gated on the API key being present), include the `qwebchannel.js` bridge library, and provide error UI when no API key is configured.

**Files to modify:**
- `src/resources/map.html.in` -- replace the Phase 1 placeholder with the full HTML skeleton

**Technical Approach:**

1. The file must be a complete HTML5 document with `<!DOCTYPE html>`, `<html>`, `<head>`, `<body>`.

2. In `<head>`, include:
   - `<meta charset="utf-8">`
   - `<meta name="viewport" content="width=device-width, initial-scale=1">`
   - A `<style>` block that makes `html, body, #map` fill the viewport (`width: 100%; height: 100%; margin: 0; padding: 0;`). Also style an `#error` div for the no-key case (centered, readable font, light gray background).

3. In `<body>`, include:
   - `<div id="map"></div>` -- the map container.
   - `<div id="error" style="display:none">` -- the error message container, shown only when the API key is missing.

4. **API key gating:** Use an inline `<script>` that checks whether the substituted key is non-empty:
   ```js
   var API_KEY = "@GOOGLE_MAPS_API_KEY@";
   ```
   If `API_KEY` is an empty string, show the `#error` div with a message like "Google Maps API key not configured. Please rebuild with -DGOOGLE_MAPS_API_KEY=your_key." and do not attempt to load the Maps API or `qwebchannel.js`.

5. If the API key is present, dynamically create and append a `<script>` element to load the Google Maps API:
   ```js
   var script = document.createElement("script");
   script.src = "https://maps.googleapis.com/maps/api/js?key=" + API_KEY + "&callback=onMapsApiReady";
   script.async = true;
   script.defer = true;
   document.head.appendChild(script);
   ```
   The `callback=onMapsApiReady` parameter causes Google Maps to call `window.onMapsApiReady()` when the API is loaded.

6. Include `qwebchannel.js` from the Qt resource system:
   ```html
   <script src="qrc:///qtwebchannel/qwebchannel.js"></script>
   ```
   This must be loaded regardless of the API key (it is needed for the bridge). Place it before the inline script block so `QWebChannel` is available.

**Acceptance Criteria:**
- [ ] `src/resources/map.html.in` contains `@GOOGLE_MAPS_API_KEY@` exactly once (inside the JS variable assignment)
- [ ] When the key is empty, the `#error` div is shown and no Google Maps script is loaded
- [ ] When the key is present, the Google Maps API script tag is dynamically appended with the key
- [ ] `qrc:///qtwebchannel/qwebchannel.js` is included
- [ ] The `#map` div fills the entire viewport (no scrollbars, no margins)
- [ ] The file is valid HTML5

**Complexity:** S

---

### Task 4.2: QWebChannel Initialization and Bridge Connection

**Purpose:** Establish the `QWebChannel` connection to the C++ `MapBridge`, wire up signal listeners, and request initial data from the bridge to handle the timing issue where the initial data push from C++ is missed because the page was not yet loaded.

**Files to modify:**
- `src/resources/map.html.in` -- add the channel initialization logic (inside the main `<script>` block)
- `src/ui/docks/map/MapBridge.h` -- add `requestInitialData()` invokable slot
- `src/ui/docks/map/MapBridge.cpp` -- implement `requestInitialData()`
- `src/ui/docks/map/MapWidget.h` -- add `pushAllData()` private method
- `src/ui/docks/map/MapWidget.cpp` -- implement `pushAllData()` and connect it to `MapBridge`

**Technical Approach:**

**C++ side (MapBridge + MapWidget):**

1. Add a new signal to `MapBridge`:
   ```cpp
   signals:
       // ... existing signals ...
       void initialDataRequested();
   ```

2. Add a new invokable slot to `MapBridge`:
   ```cpp
   Q_INVOKABLE void requestInitialData();
   ```

   Implementation:
   ```cpp
   void MapBridge::requestInitialData()
   {
       emit initialDataRequested();
   }
   ```

3. In `MapWidget`, add a private method `pushAllData()` that calls the existing serialization slots:
   ```cpp
   void MapWidget::pushAllData()
   {
       onTracksReset();
       onCursorDotsReset();
       onBoundsChanged();

       // Push current preference values
       m_bridge->pushPreference(QStringLiteral("map/lineThickness"),
                                m_preferencesBridge->lineThickness());
       m_bridge->pushPreference(QStringLiteral("map/markerSize"),
                                m_preferencesBridge->markerSize());
       m_bridge->pushPreference(QStringLiteral("map/trackOpacity"),
                                m_preferencesBridge->trackOpacity());
       m_bridge->pushPreference(QStringLiteral("map/type"),
                                m_preferencesBridge->mapTypeIndex());
   }
   ```

4. In the `MapWidget` constructor (after creating `MapBridge`), connect:
   ```cpp
   connect(m_bridge, &MapBridge::initialDataRequested,
           this, &MapWidget::pushAllData);
   ```

**JS side:**

5. Define a global `bridge` variable (initially `null`). After `onMapsApiReady` fires (Task 4.3 creates the map), initialize the `QWebChannel`:

   ```js
   var bridge = null;

   function initBridge() {
       new QWebChannel(qt.webChannelTransport, function(channel) {
           bridge = channel.objects.bridge;

           // Wire signal listeners
           bridge.tracksChanged.connect(onTracksChanged);
           bridge.cursorDotsChanged.connect(onCursorDotsChanged);
           bridge.fitBounds.connect(onFitBounds);
           bridge.preferenceChanged.connect(onPreferenceChanged);

           // Request initial data that was missed during page load
           bridge.requestInitialData();
       });
   }
   ```

6. The `initBridge()` function must be called only after both conditions are met: (a) the Google Maps API is loaded, and (b) the DOM is ready. Since `onMapsApiReady` is called by Google Maps after the API loads, and `qwebchannel.js` is loaded synchronously, calling `initBridge()` at the end of `onMapsApiReady` (after creating the map) is the correct sequencing.

**Acceptance Criteria:**
- [ ] `MapBridge` has a `requestInitialData()` invokable slot that emits `initialDataRequested()`
- [ ] `MapWidget` has a `pushAllData()` method connected to `MapBridge::initialDataRequested`
- [ ] `pushAllData()` calls `onTracksReset()`, `onCursorDotsReset()`, `onBoundsChanged()`, and pushes all four preference values
- [ ] JS creates a `QWebChannel` using `qt.webChannelTransport`
- [ ] JS connects to all four bridge signals (`tracksChanged`, `cursorDotsChanged`, `fitBounds`, `preferenceChanged`)
- [ ] JS calls `bridge.requestInitialData()` after signal wiring is complete
- [ ] The initialization sequence is: Maps API loads -> map created -> bridge initialized -> initial data requested

**Complexity:** M

---

### Task 4.3: Map Initialization

**Purpose:** Create the Google Maps instance with the correct default options, including zoom level, center, disabled default UI (except zoom), and initial map type.

**Files to modify:**
- `src/resources/map.html.in` -- implement `onMapsApiReady()` and map creation

**Technical Approach:**

1. Define the `onMapsApiReady` function as the callback for the Google Maps API script load (from Task 4.1):

   ```js
   var map = null;

   function onMapsApiReady() {
       map = new google.maps.Map(document.getElementById("map"), {
           zoom: 12,
           center: { lat: 0, lng: 0 },
           disableDefaultUI: true,
           zoomControl: true,
           mapTypeControl: true,
           mapTypeControlOptions: {
               style: google.maps.MapTypeControlStyle.DROPDOWN_MENU,
               position: google.maps.ControlPosition.TOP_RIGHT
           },
           mapTypeId: google.maps.MapTypeId.ROADMAP
       });

       // Listen for map type changes from the UI control
       map.addListener("maptypeid_changed", function() {
           if (!bridge) return;
           var typeId = map.getMapTypeId();
           var index = mapTypeIdToIndex(typeId);
           bridge.onMapTypeChanged(index);
       });

       // Initialize the QWebChannel bridge (Task 4.2)
       initBridge();
   }
   ```

2. The map options follow the overview spec (Section 8a):
   - `disableDefaultUI: true` removes all default controls.
   - `zoomControl: true` re-enables the zoom buttons.
   - `mapTypeControl: true` with `DROPDOWN_MENU` style at `TOP_RIGHT` replaces the custom QML map type selector (overview Section 8g).
   - Default `mapTypeId` is `ROADMAP`; the actual saved preference will be applied when `preferenceChanged("map/type", ...)` arrives via the init handshake.

3. Define the map type index mapping (matching the overview spec Section 8h and `MapPreferencesBridge` default of index 0):

   ```js
   var MAP_TYPE_IDS = [
       google.maps.MapTypeId.ROADMAP,    // 0
       google.maps.MapTypeId.SATELLITE,  // 1
       google.maps.MapTypeId.TERRAIN,    // 2
       google.maps.MapTypeId.HYBRID      // 3
   ];

   function mapTypeIdToIndex(typeId) {
       for (var i = 0; i < MAP_TYPE_IDS.length; i++) {
           if (MAP_TYPE_IDS[i] === typeId) return i;
       }
       return 0;
   }

   function indexToMapTypeId(index) {
       if (index >= 0 && index < MAP_TYPE_IDS.length)
           return MAP_TYPE_IDS[index];
       return google.maps.MapTypeId.ROADMAP;
   }
   ```

**Acceptance Criteria:**
- [ ] `onMapsApiReady` creates a `google.maps.Map` on the `#map` div
- [ ] Default zoom is 12, center is (0, 0)
- [ ] Default UI is disabled except zoom control and map type control
- [ ] Map type control uses `DROPDOWN_MENU` style at `TOP_RIGHT`
- [ ] `maptypeid_changed` listener calls `bridge.onMapTypeChanged(index)` with the correct integer index
- [ ] Map type index mapping: 0=roadmap, 1=satellite, 2=terrain, 3=hybrid
- [ ] `initBridge()` is called at the end of `onMapsApiReady`

**Complexity:** S

---

### Task 4.4: Color Format Conversion Utility

**Purpose:** Provide a JS utility function to convert Qt's `#AARRGGBB` hex color format to CSS-compatible formats, since all colors from the bridge use this non-standard format.

**Files to modify:**
- `src/resources/map.html.in` -- add color conversion utility functions

**Technical Approach:**

1. Qt's `QColor::name(QColor::HexArgb)` produces strings like `#ff3498db` where the first two hex chars after `#` are the alpha channel. CSS expects either `#RRGGBB` (no alpha) or `rgba(r, g, b, a)`. Define a conversion function:

   ```js
   /**
    * Convert Qt #AARRGGBB to a CSS rgba() string.
    * Input: "#AARRGGBB" (9 chars) or "#RRGGBB" (7 chars, treated as full opacity).
    */
   function qtColorToCSS(qtColor) {
       if (!qtColor || qtColor.length < 7) return "rgba(0,0,0,1)";
       if (qtColor.length === 7) {
           // Standard #RRGGBB -- no alpha
           return qtColor;
       }
       // #AARRGGBB: chars 1-2 are alpha, 3-8 are RRGGBB
       var a = parseInt(qtColor.substring(1, 3), 16) / 255.0;
       var rgb = "#" + qtColor.substring(3, 9);
       var r = parseInt(qtColor.substring(3, 5), 16);
       var g = parseInt(qtColor.substring(5, 7), 16);
       var b = parseInt(qtColor.substring(7, 9), 16);
       return "rgba(" + r + "," + g + "," + b + "," + a.toFixed(3) + ")";
   }

   /**
    * Extract opacity (0..1) from Qt #AARRGGBB color string.
    */
   function qtColorOpacity(qtColor) {
       if (!qtColor || qtColor.length < 9) return 1.0;
       return parseInt(qtColor.substring(1, 3), 16) / 255.0;
   }

   /**
    * Extract #RRGGBB from Qt #AARRGGBB color string.
    */
   function qtColorRGB(qtColor) {
       if (!qtColor || qtColor.length < 9) return qtColor || "#000000";
       return "#" + qtColor.substring(3, 9);
   }
   ```

2. Google Maps polyline options use separate `strokeColor` (hex string) and `strokeOpacity` (0..1) properties. For polylines, use `qtColorRGB()` for the color and the preference-controlled `trackOpacity` for opacity (not the alpha from the color string). For cursor dot markers, use `qtColorToCSS()` which preserves the full RGBA.

**Acceptance Criteria:**
- [ ] `qtColorToCSS("#ff3498db")` returns `"rgba(52,152,219,1.000)"`
- [ ] `qtColorToCSS("#803498db")` returns `"rgba(52,152,219,0.502)"`
- [ ] `qtColorRGB("#ff3498db")` returns `"#3498db"`
- [ ] `qtColorOpacity("#ff3498db")` returns `1.0`
- [ ] `qtColorOpacity("#803498db")` returns approximately `0.502`
- [ ] 7-char colors (no alpha) are handled gracefully
- [ ] Null/undefined input does not throw

**Complexity:** S

---

### Task 4.5: Track Polylines

**Purpose:** Render GNSS track polylines on the map, maintaining a collection keyed by session ID. Clear and recreate polylines when track data changes. Apply line thickness and opacity from preferences.

**Files to modify:**
- `src/resources/map.html.in` -- implement `onTracksChanged()` and polyline management

**Technical Approach:**

1. Maintain a JS `Map` for polylines and a parallel `Map` for the raw point data (needed by hover detection):

   ```js
   var polylines = new Map();      // sessionId -> google.maps.Polyline
   var trackData = new Map();      // sessionId -> [{lat, lon, t}, ...]
   ```

2. Maintain preference state variables with defaults matching `MapPreferencesBridge` defaults:

   ```js
   var prefLineThickness = 3.0;
   var prefTrackOpacity = 0.85;
   var prefMarkerSize = 10;
   ```

3. Implement `onTracksChanged(tracks)`:

   ```js
   function onTracksChanged(tracks) {
       // Clear existing polylines
       polylines.forEach(function(pl) { pl.setMap(null); });
       polylines.clear();
       trackData.clear();

       for (var i = 0; i < tracks.length; i++) {
           var track = tracks[i];
           var sessionId = track.sessionId;
           var points = track.points;
           var color = track.color;

           // Store raw points for hover detection
           trackData.set(sessionId, points);

           // Build the path array
           var path = [];
           for (var j = 0; j < points.length; j++) {
               path.push({ lat: points[j].lat, lng: points[j].lon });
           }

           var polyline = new google.maps.Polyline({
               path: path,
               strokeColor: qtColorRGB(color),
               strokeOpacity: prefTrackOpacity,
               strokeWeight: prefLineThickness,
               map: map,
               zIndex: 10
           });

           polylines.set(sessionId, polyline);
       }
   }
   ```

4. Note that `strokeOpacity` is set from `prefTrackOpacity` (the preference value), not from the alpha channel in the color. The color's alpha channel already encodes the track opacity (set by `TrackMapModel::colorForSession`), but Google Maps polylines use separate `strokeColor` and `strokeOpacity` properties. The preference `prefTrackOpacity` is the authoritative source.

5. Follow the pattern in `MapDock.qml` lines 293-315 where the QML `MapItemView` creates `MapPolyline` delegates with `line.width: mapPreferences.lineThickness` and `line.color: trackColor`.

**Acceptance Criteria:**
- [ ] `polylines` Map is keyed by `sessionId`
- [ ] `trackData` Map stores the raw points array (with `t` values) for each session
- [ ] On `tracksChanged`, all existing polylines are removed from the map before creating new ones
- [ ] Each polyline's `strokeColor` is extracted from the Qt `#AARRGGBB` color (RGB portion only)
- [ ] Each polyline's `strokeOpacity` comes from the `prefTrackOpacity` preference variable
- [ ] Each polyline's `strokeWeight` comes from the `prefLineThickness` preference variable
- [ ] Polylines are added to the map (`map` option set)

**Complexity:** M

---

### Task 4.6: Cursor Dot Markers

**Purpose:** Render cursor position dots on the map as colored circle markers with white borders, matching the visual appearance of the QML implementation. Maintain a collection keyed by session ID and recreate on each update.

**Files to modify:**
- `src/resources/map.html.in` -- implement `onCursorDotsChanged()` and marker management

**Technical Approach:**

1. Maintain a JS `Map` for markers:

   ```js
   var cursorMarkers = new Map();  // sessionId -> google.maps.Marker
   ```

2. Implement `onCursorDotsChanged(dots)`:

   ```js
   function onCursorDotsChanged(dots) {
       // Clear existing markers
       cursorMarkers.forEach(function(marker) { marker.setMap(null); });
       cursorMarkers.clear();

       for (var i = 0; i < dots.length; i++) {
           var dot = dots[i];
           var cssColor = qtColorRGB(dot.color);

           var marker = new google.maps.Marker({
               position: { lat: dot.lat, lng: dot.lon },
               map: map,
               icon: {
                   path: google.maps.SymbolPath.CIRCLE,
                   fillColor: cssColor,
                   fillOpacity: 1.0,
                   strokeColor: "#ffffff",
                   strokeWeight: 2,
                   scale: prefMarkerSize / 2
               },
               zIndex: 20,
               clickable: false
           });

           cursorMarkers.set(dot.sessionId, marker);
       }
   }
   ```

3. The marker icon uses `google.maps.SymbolPath.CIRCLE` as a `google.maps.Symbol`, following the overview spec (Section 8c). The `scale` property sets the radius of the circle, so dividing `prefMarkerSize` by 2 produces a marker whose total diameter matches the QML `Rectangle` width/height. This matches the QML pattern in `MapDock.qml` lines 324-345 where `dotItem` has `width: mapPreferences.markerSize` and `radius: width/2`.

4. The `strokeColor: "#ffffff"` and `strokeWeight: 2` match the QML `border.width: 2` and `border.color: "white"` from `MapDock.qml` line 343-344.

5. `clickable: false` prevents the marker from consuming mouse events, which would interfere with hover detection on polylines.

**Acceptance Criteria:**
- [ ] `cursorMarkers` Map is keyed by `sessionId`
- [ ] On `cursorDotsChanged`, all existing markers are removed before creating new ones
- [ ] Each marker is positioned at `{lat: dot.lat, lng: dot.lon}`
- [ ] Each marker uses a `CIRCLE` symbol path with the dot's color as fill
- [ ] Marker has white border (`strokeColor: "#ffffff"`, `strokeWeight: 2`)
- [ ] Marker scale is derived from `prefMarkerSize`
- [ ] Markers have `zIndex: 20` (above polylines at `zIndex: 10`)
- [ ] Markers are not clickable

**Complexity:** S

---

### Task 4.7: Hover Detection on Polylines

**Purpose:** Port the QML hover detection algorithm to JavaScript, detecting when the mouse is near a track polyline in screen-pixel space, interpolating the UTC time at the closest point, and notifying the C++ bridge. This is the most complex task in this phase and must faithfully reproduce the behavior of `MapDock.qml` lines 40-160.

**Files to modify:**
- `src/resources/map.html.in` -- implement hover detection system

**Technical Approach:**

1. **Coordinate-to-pixel conversion:** Use the `OverlayView` technique to obtain a `MapCanvasProjection` that can convert `LatLng` to container pixel coordinates. Create an invisible overlay:

   ```js
   var projection = null;

   var ProjectionOverlay = function() {};
   ProjectionOverlay.prototype = new google.maps.OverlayView();
   ProjectionOverlay.prototype.onAdd = function() {};
   ProjectionOverlay.prototype.onRemove = function() {};
   ProjectionOverlay.prototype.draw = function() {};

   // In onMapsApiReady, after creating the map:
   var overlay = new ProjectionOverlay();
   overlay.setMap(map);
   ```

   The projection is obtained via `overlay.getProjection()` and the method `fromLatLngToContainerPixel(latLng)` converts geographic coordinates to screen-space pixel positions relative to the map container.

2. **State variables** (matching `MapDock.qml` lines 9-19):

   ```js
   var HOVER_THRESHOLD_PX = 10;
   var HOVER_MIN_MOVE_PX = 2;
   var HOVER_TIME_EPSILON_SEC = 0.02;

   var pendingMouseX = -1, pendingMouseY = -1;
   var lastProcessedX = -1, lastProcessedY = -1;
   var hoverSessionId = "";
   var hoverUtcSeconds = NaN;
   var hoverRafId = 0;
   ```

3. **Point-to-segment distance function** (faithfully ported from `MapDock.qml` lines 40-59, `_distPointToSegment2`):

   ```js
   function distPointToSegment2(px, py, ax, ay, bx, by) {
       var abx = bx - ax;
       var aby = by - ay;
       var apx = px - ax;
       var apy = py - ay;

       var denom = abx * abx + aby * aby;
       var u = 0.0;
       if (denom > 1e-9)
           u = (apx * abx + apy * aby) / denom;

       if (u < 0) u = 0;
       else if (u > 1) u = 1;

       var cx = ax + u * abx;
       var cy = ay + u * aby;
       var dx = px - cx;
       var dy = py - cy;

       return { dist2: dx * dx + dy * dy, u: u };
   }
   ```

   This is a direct port of the QML function. The variable names match (`abx`, `aby`, `apx`, `apy`, `denom`, `u`, `cx`, `cy`, `dx`, `dy`). The threshold constant `1e-9` is preserved. The return value is `{dist2, u}` where `u` is the projection parameter used for time interpolation.

4. **Main hover update function** (ported from `MapDock.qml` lines 113-160, `_updateHoverAt`):

   ```js
   function updateHoverAt(mx, my) {
       if (!bridge) return;

       var proj = overlay.getProjection();
       if (!proj) return;

       var thr2 = HOVER_THRESHOLD_PX * HOVER_THRESHOLD_PX;
       var bestDist2 = thr2 + 1;
       var bestSessionId = "";
       var bestUtc = NaN;

       trackData.forEach(function(points, sessionId) {
           if (!points || points.length < 2) return;

           for (var i = 0; i < points.length - 1; i++) {
               var p0 = points[i];
               var p1 = points[i + 1];

               if (p0.t === undefined || p1.t === undefined) continue;

               var A = proj.fromLatLngToContainerPixel(
                   new google.maps.LatLng(p0.lat, p0.lon));
               var B = proj.fromLatLngToContainerPixel(
                   new google.maps.LatLng(p1.lat, p1.lon));

               if (!A || !B) continue;

               var r = distPointToSegment2(mx, my, A.x, A.y, B.x, B.y);

               if (r.dist2 < bestDist2) {
                   bestDist2 = r.dist2;
                   bestSessionId = sessionId;
                   bestUtc = p0.t + r.u * (p1.t - p0.t);
               }
           }
       });

       if (bestSessionId !== "" && bestDist2 <= thr2 && !isNaN(bestUtc)) {
           setMapHover(bestSessionId, bestUtc);
       } else {
           clearMapHover();
       }
   }
   ```

   This follows the same structure as the QML `_updateHoverAt` function: iterate all tracks, iterate all segments, convert each endpoint to screen pixels, compute point-to-segment distance, track the best (closest) hit, then call `setMapHover` or `clearMapHover`.

5. **Hover set/clear with epsilon filter** (ported from `MapDock.qml` lines 76-111):

   ```js
   function setMapHover(sessionId, utcSeconds) {
       var sessionChanged = (sessionId !== hoverSessionId);
       var timeChanged = sessionChanged
           || isNaN(hoverUtcSeconds)
           || Math.abs(utcSeconds - hoverUtcSeconds) > HOVER_TIME_EPSILON_SEC;

       if (!timeChanged) return;

       hoverSessionId = sessionId;
       hoverUtcSeconds = utcSeconds;

       bridge.onMapHover(sessionId, utcSeconds);
   }

   function clearMapHover() {
       if (hoverSessionId === "" && isNaN(hoverUtcSeconds)) return;

       hoverSessionId = "";
       hoverUtcSeconds = NaN;

       if (bridge) bridge.onMapHoverClear();
   }
   ```

6. **Mousemove listener with throttling** (matching the 16ms timer in `MapDock.qml` lines 162-181):

   ```js
   map.getDiv().addEventListener("mousemove", function(e) {
       var rect = map.getDiv().getBoundingClientRect();
       pendingMouseX = e.clientX - rect.left;
       pendingMouseY = e.clientY - rect.top;

       if (!hoverRafId) {
           hoverRafId = requestAnimationFrame(processHover);
       }
   });

   map.getDiv().addEventListener("mouseleave", function() {
       pendingMouseX = -1;
       pendingMouseY = -1;
       if (hoverRafId) {
           cancelAnimationFrame(hoverRafId);
           hoverRafId = 0;
       }
       clearMapHover();
   });

   function processHover() {
       hoverRafId = 0;

       if (pendingMouseX < 0 || pendingMouseY < 0) return;

       var dx = pendingMouseX - lastProcessedX;
       var dy = pendingMouseY - lastProcessedY;
       var min2 = HOVER_MIN_MOVE_PX * HOVER_MIN_MOVE_PX;

       if (dx * dx + dy * dy < min2) return;

       lastProcessedX = pendingMouseX;
       lastProcessedY = pendingMouseY;

       updateHoverAt(pendingMouseX, pendingMouseY);
   }
   ```

   The `requestAnimationFrame` approach is used instead of `setInterval(16)` because it aligns with the display refresh rate (~60Hz), matches the intent of the QML 16ms timer, and automatically pauses when the tab is not visible.

7. **Force recalculation on zoom/pan:** When the map moves or zooms while the mouse is stationary over it, the screen-pixel positions of the polylines change but no `mousemove` fires. This matches the QML `onZoomLevelChanged` / `onCenterChanged` handlers in `MapDock.qml` lines 211-214:

   ```js
   map.addListener("zoom_changed", pokeHoverRecalc);
   map.addListener("center_changed", pokeHoverRecalc);

   function pokeHoverRecalc() {
       if (pendingMouseX < 0 || pendingMouseY < 0) return;
       // Force bypass the min-move filter by resetting lastProcessed
       lastProcessedX = -999;
       lastProcessedY = -999;
       if (!hoverRafId) {
           hoverRafId = requestAnimationFrame(processHover);
       }
   }
   ```

**Acceptance Criteria:**
- [ ] `distPointToSegment2` produces identical results to `MapDock.qml` `_distPointToSegment2` for the same inputs
- [ ] Hover threshold is 10 pixels, matching `hoverThresholdPx: 10` in QML
- [ ] Minimum mouse movement filter is 2 pixels, matching `hoverMinMovePx: 2` in QML
- [ ] Time epsilon filter is 0.02 seconds, matching `hoverTimeEpsilonSec: 0.02` in QML
- [ ] Hover is throttled to ~60Hz via `requestAnimationFrame`
- [ ] When a polyline hit is found, `bridge.onMapHover(sessionId, utcSeconds)` is called
- [ ] When no hit, `bridge.onMapHoverClear()` is called
- [ ] Mouse leaving the map container clears hover
- [ ] Zoom and pan changes trigger hover recalculation if the mouse is over the map
- [ ] Time is interpolated as `p0.t + u * (p1.t - p0.t)` where `u` is the segment projection parameter

**Complexity:** L

---

### Task 4.8: Fit-to-Bounds

**Purpose:** Implement the `fitBounds` bridge signal handler to auto-zoom the map to contain all tracks, with padding.

**Files to modify:**
- `src/resources/map.html.in` -- implement `onFitBounds()`

**Technical Approach:**

1. Implement the handler matching overview spec Section 8e:

   ```js
   function onFitBounds(south, west, north, east) {
       if (!map) return;
       var bounds = new google.maps.LatLngBounds(
           { lat: south, lng: west },
           { lat: north, lng: east }
       );
       map.fitBounds(bounds, 40);
   }
   ```

2. The padding value of 40 pixels matches the QML `fitViewportToGeoShape(trackModel.bounds, 40)` call in `MapDock.qml` line 200.

3. This function is connected to `bridge.fitBounds.connect(onFitBounds)` in the bridge initialization (Task 4.2).

**Acceptance Criteria:**
- [ ] `onFitBounds(south, west, north, east)` calls `map.fitBounds()` with the correct `LatLngBounds`
- [ ] Padding is 40 pixels, matching the QML implementation
- [ ] Function handles being called before map is initialized (null guard)

**Complexity:** S

---

### Task 4.9: Preference Reactivity

**Purpose:** Handle `preferenceChanged` signals from the bridge to update polyline stroke weight, stroke opacity, marker size, and map type in real time.

**Files to modify:**
- `src/resources/map.html.in` -- implement `onPreferenceChanged()`

**Technical Approach:**

1. Implement the handler matching overview spec Section 8h:

   ```js
   function onPreferenceChanged(key, value) {
       if (key === "map/lineThickness") {
           prefLineThickness = value;
           polylines.forEach(function(pl) {
               pl.setOptions({ strokeWeight: value });
           });
       }
       else if (key === "map/trackOpacity") {
           prefTrackOpacity = value;
           polylines.forEach(function(pl) {
               pl.setOptions({ strokeOpacity: value });
           });
       }
       else if (key === "map/markerSize") {
           prefMarkerSize = value;
           cursorMarkers.forEach(function(marker) {
               var icon = marker.getIcon();
               if (icon) {
                   icon.scale = value / 2;
                   marker.setIcon(icon);
               }
           });
       }
       else if (key === "map/type") {
           var typeId = indexToMapTypeId(value);
           if (map && map.getMapTypeId() !== typeId) {
               map.setMapTypeId(typeId);
           }
       }
   }
   ```

2. For `map/lineThickness`: Update `prefLineThickness` and call `setOptions({strokeWeight})` on every polyline.

3. For `map/trackOpacity`: Update `prefTrackOpacity` and call `setOptions({strokeOpacity})` on every polyline.

4. For `map/markerSize`: Update `prefMarkerSize` and update each marker's icon `scale` property (divided by 2 to convert diameter to radius).

5. For `map/type`: Convert the integer index to a `MapTypeId` using `indexToMapTypeId()` (from Task 4.3) and call `map.setMapTypeId()`. Guard against redundant sets to avoid triggering the `maptypeid_changed` listener, which would call `bridge.onMapTypeChanged()` and create a feedback loop.

6. The preference key strings (`"map/lineThickness"`, `"map/trackOpacity"`, `"map/markerSize"`, `"map/type"`) must exactly match those used in the `MapWidget` constructor lambdas documented in Phase 3, Task 3.5 step 2h.

**Acceptance Criteria:**
- [ ] `"map/lineThickness"` updates `strokeWeight` on all existing polylines
- [ ] `"map/trackOpacity"` updates `strokeOpacity` on all existing polylines
- [ ] `"map/markerSize"` updates the icon `scale` on all existing cursor markers
- [ ] `"map/type"` calls `map.setMapTypeId()` with the correct `MapTypeId`
- [ ] Map type set avoids feedback loop (checks current type before setting)
- [ ] Preference state variables (`prefLineThickness`, `prefTrackOpacity`, `prefMarkerSize`) are updated so newly created polylines/markers use the current values
- [ ] Preference key strings exactly match those emitted by `MapWidget`

**Complexity:** M

---

## Testing Requirements

### Unit Tests

No unit test framework is currently active for the map dock or for JavaScript in this project. However, the following functions are independently testable if a harness is added:

- `qtColorToCSS` / `qtColorRGB` / `qtColorOpacity`: pure functions, easily tested with known inputs.
- `distPointToSegment2`: pure function, can be tested against the QML `_distPointToSegment2` with identical inputs.
- `mapTypeIdToIndex` / `indexToMapTypeId`: mapping functions, testable with all 4 valid indices and edge cases.

### Integration Tests

- **Build test:** `cmake --build build` succeeds after replacing the placeholder `map.html.in` with the full implementation.
- **configure_file test:** The built `map.html` in the build directory contains the substituted API key (no `@GOOGLE_MAPS_API_KEY@` literal remaining) and all the JavaScript code.
- **Bridge handshake test:** With a valid API key, launch the application, open the map dock, and verify (via `qDebug` or browser dev tools in `QWebEngineView`) that `requestInitialData()` is called by JS and `pushAllData()` fires on the C++ side.

### Manual Verification

1. **No API key:** Build without `-DGOOGLE_MAPS_API_KEY`. Open the map dock. Verify the error message is displayed instead of a map.
2. **Map loads:** Build with a valid API key. Open the map dock. Verify Google Maps tiles render with zoom controls and the map type dropdown in the top-right corner.
3. **Track polylines:** Load a session with GNSS data. Verify colored polylines appear on the map corresponding to the GPS tracks.
4. **Polyline updates:** Toggle session visibility or change the plot range. Verify polylines appear/disappear/update accordingly.
5. **Cursor dots:** Hover over a plot dock. Verify colored circle markers with white borders appear on the map at the correct positions.
6. **Hover detection:** Slowly move the mouse over a track polyline on the map. Verify cursor dots appear on the plot docks (bidirectional cursor sync). Move the mouse away from all polylines and verify cursors clear.
7. **Hover edge cases:** Zoom in very close to a track and verify hover still works. Zoom out far and verify hover works with the 10px threshold. Hover near a track intersection and verify the closest track is selected.
8. **Fit-to-bounds:** Load a session. Verify the map auto-zooms to show all tracks with padding.
9. **Map type selector:** Click the map type dropdown. Switch between Roadmap, Satellite, Terrain, Hybrid. Verify the map changes. Close and reopen the application. Verify the last-selected map type is restored.
10. **Preference changes:** Open Settings > Map. Change line thickness -- verify polylines update immediately. Change marker size -- verify cursor dots resize. Change track opacity -- verify polyline opacity changes. Reset to defaults -- verify all properties revert.
11. **Pan and zoom:** Drag to pan, scroll to zoom, pinch to zoom (on touch devices). Verify these work natively via Google Maps.
12. **Hover during zoom/pan:** While hovering near a track, use the scroll wheel to zoom. Verify hover continues to track correctly as the map view changes under the stationary cursor.

## Notes for Implementer

### Gotchas

- **`@ONLY` in `configure_file`:** The `map.html.in` template is processed with `@ONLY`, so `@GOOGLE_MAPS_API_KEY@` is the only CMake-substituted variable. Do NOT use `${...}` syntax for CMake variables in the file -- those will be treated as literal JavaScript template literals or object destructuring. However, JavaScript `${...}` inside template literals (backtick strings) is safe because `@ONLY` does not process `${}` syntax.

- **Qt `#AARRGGBB` color format:** This is not standard CSS. The first two hex digits after `#` are alpha, not red. CSS interprets `#AARRGGBB` as a broken 8-digit hex where the bytes are in the wrong order. You MUST convert before passing to Google Maps APIs. Use `qtColorRGB()` for polyline `strokeColor` and `qtColorToCSS()` for any CSS property that needs alpha.

- **`fromLatLngToContainerPixel` availability:** The `MapCanvasProjection` from `OverlayView.getProjection()` is not available until the overlay's `onAdd` is called, which happens asynchronously after `overlay.setMap(map)`. However, by the time the user starts moving the mouse, the overlay will be ready. If `getProjection()` returns `null`, the hover function should silently return (no crash, just no hover).

- **Google Maps `Marker` deprecation:** Google has introduced `AdvancedMarkerElement` in the newer Maps JS API. However, `google.maps.Marker` with `google.maps.Symbol` is still supported and is simpler to use for programmatic colored circles. Use `Marker` with `SymbolPath.CIRCLE` as specified in the overview. If a deprecation warning appears in the console, it can be addressed in a future phase.

- **`mousemove` on `map.getDiv()` vs `google.maps.event.addListener(map, "mousemove")`:** The Google Maps `mousemove` event provides a `LatLng` but NOT screen pixel coordinates. We need screen pixels for the distance calculation. Using a DOM `mousemove` listener on `map.getDiv()` provides `clientX`/`clientY` which can be converted to container-relative coordinates. This is more reliable than trying to convert `LatLng` back to pixels.

- **Feedback loop prevention for map type:** When JS calls `bridge.onMapTypeChanged(index)`, C++ saves the preference and emits `preferenceChanged("map/type", index)` back to JS. The `onPreferenceChanged` handler must check `map.getMapTypeId() !== typeId` before calling `map.setMapTypeId()` to avoid an infinite loop.

- **Initial data ordering:** When `bridge.requestInitialData()` triggers `pushAllData()`, the preference values (including `map/type`) arrive via `preferenceChanged` signals. These may arrive before or after `tracksChanged`. The code must handle any ordering. Since preference state variables have defaults and `onPreferenceChanged` updates both the state and existing objects, this should work correctly regardless of order.

- **Performance of hover detection:** The brute-force approach (iterating all segments of all tracks, converting each endpoint to screen pixels) can be expensive with many points. The QML implementation uses the same brute-force approach, so performance should be comparable. The 2px minimum-movement filter and `requestAnimationFrame` throttling mitigate this. For future optimization, consider spatial indexing or pre-projecting points on zoom change, but this is out of scope for this phase.

### Decisions Made

- **`requestAnimationFrame` instead of `setInterval(16)`:** `requestAnimationFrame` is the standard web approach for ~60Hz throttling and has the advantage of automatically pausing when the tab is not visible. It is semantically equivalent to the QML `Timer { interval: 16; repeat: false }`.

- **DOM `mousemove` listener instead of Google Maps event:** The DOM event provides pixel coordinates directly, while the Google Maps `mousemove` event only provides `LatLng`. Since the hover algorithm operates in screen-pixel space, using the DOM event avoids an unnecessary LatLng-to-pixel conversion for the mouse position.

- **`OverlayView` technique for projection:** The Google Maps JS API does not expose `map.getProjection().fromLatLngToContainerPixel()` directly. The `OverlayView` trick is the documented approach to get a `MapCanvasProjection` with container-pixel conversion. This is a well-known pattern in the Google Maps developer community.

- **Polyline stroke opacity from preference, not color alpha:** `TrackMapModel::colorForSession` encodes opacity in the color's alpha channel. However, Google Maps `Polyline` uses separate `strokeColor` and `strokeOpacity` properties. Using the preference `trackOpacity` directly for `strokeOpacity` (and ignoring the alpha in the color string) ensures that real-time preference changes update correctly without rebuilding the track data.

- **`requestInitialData` added to MapBridge:** This is the simplest solution to the initial data timing problem documented in Phase 3. JS calls `requestInitialData()` once the channel is established, and C++ responds by re-pushing all current data. This avoids complex page-load detection on the C++ side.

- **Marker icon `scale` is `prefMarkerSize / 2`:** The QML implementation uses a `Rectangle` with `width: markerSize` and `radius: width/2` to create a circle of diameter `markerSize`. Google Maps `Symbol` `scale` sets the radius (for `CIRCLE` path), so dividing by 2 achieves the same visual diameter.

### Open Questions

- **Google Maps JS API version pinning:** The current implementation loads the Maps API without a version parameter (`v=`), which uses the weekly channel (latest stable). For production stability, consider pinning to a specific version (e.g., `v=3.58`). This is a minor concern and can be addressed post-launch.

- **Content Security Policy (CSP) in QWebEngineView:** By default, `QWebEngineView` loading local files may restrict network requests to Google Maps servers. If the Maps API fails to load, the `QWebEnginePage` may need its settings adjusted (e.g., `QWebEngineSettings::LocalContentCanAccessRemoteUrls` set to `true`). The implementer should test this and add the setting in `MapWidget.cpp` if needed. This is a Phase 3/4 boundary issue.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. `src/resources/map.html.in` contains the full Google Maps JavaScript implementation (no placeholder content remains)
3. The Google Maps view loads with a valid API key and shows an error message without one
4. Track polylines render with correct colors, thickness, and opacity
5. Cursor dot markers render with correct colors, size, and white borders
6. Hover detection works bidirectionally (map hover drives plot cursors via the bridge)
7. Map auto-zooms to fit tracks on data load
8. Map type selector works and persists the selection via the bridge
9. All four preference keys update the map in real time
10. The initialization handshake (`requestInitialData`) correctly delivers initial data to JS
11. No TODOs or placeholder code remains
12. Code follows the patterns established in `MapDock.qml` for hover detection and visual appearance
