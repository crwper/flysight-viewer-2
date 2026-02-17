# Phase 2: Model Layer Refactor

## Overview

Replace the `QGeoCoordinate` and `QGeoRectangle` types used for the `center` and `bounds` Q_PROPERTYs in `TrackMapModel` with plain `double` properties (`centerLat`, `centerLon`, `boundsNorth`, `boundsSouth`, `boundsEast`, `boundsWest`). This makes the model compatible with `QWebChannel`, which cannot easily serialize Qt geo types, and removes the dependency on `<QGeoCoordinate>` and `<QGeoRectangle>` headers from the model layer.

## Dependencies

- **Depends on:** None -- can begin immediately
- **Blocks:** Phase 3 (MapBridge & MapWidget Rewrite) -- `MapBridge` reads these new double properties when emitting `fitBounds` to the JS side
- **Assumptions:** The existing `TrackMapModel` is at the state found on the `google_maps` branch (commit `e3aea43` or later). No other phase needs to have been applied first.

## Tasks

### Task 2.1: Replace Q_PROPERTY declarations and member variables in TrackMapModel header

**Purpose:** Remove the `QGeoCoordinate`/`QGeoRectangle` types from the public API and replace them with six plain `double` properties that `QWebChannel` can serialize natively.

**Files to modify:**
- `src/ui/docks/map/TrackMapModel.h` -- replace includes, Q_PROPERTYs, accessors, signals, and member variables

**Technical Approach:**

1. **Remove includes** (lines 6-7):
   - Remove `#include <QGeoCoordinate>`
   - Remove `#include <QGeoRectangle>`

2. **Replace Q_PROPERTY declarations** (lines 34-35). Remove:
   ```cpp
   Q_PROPERTY(QGeoCoordinate center READ center NOTIFY centerChanged)
   Q_PROPERTY(QGeoRectangle bounds READ bounds NOTIFY boundsChanged)
   ```
   Replace with six individual double properties:
   ```cpp
   Q_PROPERTY(double centerLat READ centerLat NOTIFY centerChanged)
   Q_PROPERTY(double centerLon READ centerLon NOTIFY centerChanged)
   Q_PROPERTY(double boundsNorth READ boundsNorth NOTIFY boundsChanged)
   Q_PROPERTY(double boundsSouth READ boundsSouth NOTIFY boundsChanged)
   Q_PROPERTY(double boundsEast READ boundsEast NOTIFY boundsChanged)
   Q_PROPERTY(double boundsWest READ boundsWest NOTIFY boundsChanged)
   ```
   Note: the `centerChanged` and `boundsChanged` signal names are reused -- they still serve the same semantic purpose, just the payload type changes. All six properties are read-only (no WRITE accessor needed).

3. **Replace public accessor methods** (lines 54-55). Remove:
   ```cpp
   QGeoCoordinate center() const { return m_center; }
   QGeoRectangle bounds() const { return m_bounds; }
   ```
   Replace with six inline accessors:
   ```cpp
   double centerLat() const { return m_centerLat; }
   double centerLon() const { return m_centerLon; }
   double boundsNorth() const { return m_boundsNorth; }
   double boundsSouth() const { return m_boundsSouth; }
   double boundsEast() const { return m_boundsEast; }
   double boundsWest() const { return m_boundsWest; }
   ```

4. **Keep signals unchanged** (lines 66-67). The signals `centerChanged()` and `boundsChanged()` remain as-is -- they carry no parameters, so their signature does not depend on the property type. No change needed here.

5. **Replace private member variables** (lines 84-85). Remove:
   ```cpp
   QGeoCoordinate m_center;
   QGeoRectangle m_bounds;
   ```
   Replace with six doubles, initialized to `0.0` (or `qQNaN()` if you want to distinguish "no data" from "center at origin" -- but `0.0` is consistent with the existing `hasData` flag pattern):
   ```cpp
   double m_centerLat = 0.0;
   double m_centerLon = 0.0;
   double m_boundsNorth = 0.0;
   double m_boundsSouth = 0.0;
   double m_boundsEast = 0.0;
   double m_boundsWest = 0.0;
   ```

**Acceptance Criteria:**
- [ ] `#include <QGeoCoordinate>` and `#include <QGeoRectangle>` are removed from the header
- [ ] Six `Q_PROPERTY(double ...)` declarations replace the old two
- [ ] Six inline accessor methods exist: `centerLat()`, `centerLon()`, `boundsNorth()`, `boundsSouth()`, `boundsEast()`, `boundsWest()`
- [ ] Six private `double` member variables exist with default `0.0`
- [ ] The `centerChanged` and `boundsChanged` signals are preserved (same name, no-argument signature)
- [ ] The `hasData`, `count` Q_PROPERTYs and their signals are unchanged
- [ ] The model roles enum (`SessionIdRole`, `TrackPointsRole`, `TrackColorRole`) is unchanged
- [ ] The file compiles without referencing any Qt geo headers

**Complexity:** S

---

### Task 2.2: Update rebuild() bounds computation in TrackMapModel implementation

**Purpose:** Update the `rebuild()` method to store computed bounds and center into the new plain-double member variables instead of constructing `QGeoRectangle`/`QGeoCoordinate` objects.

**Files to modify:**
- `src/ui/docks/map/TrackMapModel.cpp` -- update the `rebuild()` method and remove unnecessary includes

**Technical Approach:**

1. **No include changes needed in the .cpp file.** The .cpp file does not directly include `<QGeoCoordinate>` or `<QGeoRectangle>` -- those came transitively via the header. Once Task 2.1 removes them from the header, they are gone. Verify that no other code in the .cpp uses these types.

2. **Update the local "old value" captures at the top of `rebuild()`** (lines 137-138). Currently:
   ```cpp
   const QGeoCoordinate oldCenter = m_center;
   const QGeoRectangle oldBounds = m_bounds;
   ```
   Replace with captures of the individual doubles:
   ```cpp
   const double oldCenterLat = m_centerLat;
   const double oldCenterLon = m_centerLon;
   const double oldBoundsNorth = m_boundsNorth;
   const double oldBoundsSouth = m_boundsSouth;
   const double oldBoundsEast = m_boundsEast;
   const double oldBoundsWest = m_boundsWest;
   ```

3. **Update the bounds assignment block** (lines 222-230). Currently:
   ```cpp
   if (haveBounds) {
       const QGeoCoordinate topLeft(maxLat, minLon);
       const QGeoCoordinate bottomRight(minLat, maxLon);
       m_bounds = QGeoRectangle(topLeft, bottomRight);
       m_center = m_bounds.center();
   } else {
       m_bounds = QGeoRectangle();
       m_center = QGeoCoordinate();
   }
   ```
   Replace with direct double assignments:
   ```cpp
   if (haveBounds) {
       m_boundsNorth = maxLat;
       m_boundsSouth = minLat;
       m_boundsEast  = maxLon;
       m_boundsWest  = minLon;
       m_centerLat   = (maxLat + minLat) / 2.0;
       m_centerLon   = (maxLon + minLon) / 2.0;
   } else {
       m_boundsNorth = 0.0;
       m_boundsSouth = 0.0;
       m_boundsEast  = 0.0;
       m_boundsWest  = 0.0;
       m_centerLat   = 0.0;
       m_centerLon   = 0.0;
   }
   ```
   Note: the center computation `(max + min) / 2.0` is the simple arithmetic mean, which matches what `QGeoRectangle::center()` does for non-antimeridian-crossing rectangles. Since GNSS skydiving/BASE tracks do not cross the antimeridian, this is correct. See the "Gotchas" section below for the edge case.

4. **Update the change-detection emit block** (lines 234-235). Currently:
   ```cpp
   if (oldCenter != m_center) emit centerChanged();
   if (oldBounds != m_bounds) emit boundsChanged();
   ```
   Replace with comparisons on the individual doubles. Use an epsilon-free exact comparison (matching the existing pattern -- `QGeoCoordinate::operator!=` also uses exact comparison internally):
   ```cpp
   if (oldCenterLat != m_centerLat || oldCenterLon != m_centerLon)
       emit centerChanged();
   if (oldBoundsNorth != m_boundsNorth || oldBoundsSouth != m_boundsSouth ||
       oldBoundsEast != m_boundsEast || oldBoundsWest != m_boundsWest)
       emit boundsChanged();
   ```

5. **The rest of `rebuild()` is unchanged.** The loop that computes `minLat`, `maxLat`, `minLon`, `maxLon` (lines 143-215) already uses plain doubles and requires no modification. The `haveBounds` flag logic is also unchanged.

**Acceptance Criteria:**
- [ ] No `QGeoCoordinate` or `QGeoRectangle` types appear anywhere in `TrackMapModel.cpp`
- [ ] The six member variables (`m_centerLat`, `m_centerLon`, `m_boundsNorth`, `m_boundsSouth`, `m_boundsEast`, `m_boundsWest`) are assigned correctly in the `haveBounds` branch
- [ ] The six member variables are reset to `0.0` in the `!haveBounds` branch
- [ ] `m_centerLat` is computed as `(maxLat + minLat) / 2.0` and `m_centerLon` as `(maxLon + minLon) / 2.0`
- [ ] `centerChanged` is emitted only when either `m_centerLat` or `m_centerLon` changes
- [ ] `boundsChanged` is emitted only when any of the four bounds doubles changes
- [ ] The track-building loop (session iteration, point filtering, `Track` struct population) is unmodified
- [ ] The file compiles and links successfully

**Complexity:** S

---

### Task 2.3: Update MapPreferencesBridge map type index comment

**Purpose:** Document the new Google Maps type index mapping in the `MapPreferencesBridge` header so that future readers (and Phase 3's `MapBridge`) understand the index semantics.

**Files to modify:**
- `src/ui/docks/map/MapPreferencesBridge.h` -- update the class-level or property-level doc comment

**Technical Approach:**

Update the class doc comment (lines 9-14) to note the new Google Maps mapping. The existing comment says "Bridge class to expose map preferences to QML." Update it to reflect the broader usage:

```cpp
/**
 * @brief Bridge class to expose map preferences.
 *
 * This class listens to PreferencesManager changes and notifies consumers
 * when map-related preferences change.
 *
 * Map type index mapping (Google Maps):
 *   0 = roadmap
 *   1 = satellite
 *   2 = terrain
 *   3 = hybrid
 */
```

Also update the `mapTypeIndex` Q_PROPERTY line (line 22) with an inline comment:

```cpp
Q_PROPERTY(int mapTypeIndex READ mapTypeIndex WRITE setMapTypeIndex NOTIFY mapTypeIndexChanged)
// Map type: 0=roadmap, 1=satellite, 2=terrain, 3=hybrid
```

No functional changes to `MapPreferencesBridge.cpp` are required. The `setMapTypeIndex` implementation (line 46-51 of the .cpp) persists the raw integer index and does not need updating.

**Acceptance Criteria:**
- [ ] The class doc comment references Google Maps type names, not QML/OSM
- [ ] The `mapTypeIndex` property has a comment documenting the index-to-type mapping
- [ ] No functional code changes are made to `MapPreferencesBridge.h` or `.cpp`
- [ ] The file compiles without warnings

**Complexity:** S

---

## Testing Requirements

### Unit Tests

No unit tests currently exist for `TrackMapModel` or `MapPreferencesBridge` in this project. Creating new unit tests is not in scope for this phase, but the refactored model should be verifiable through the integration and manual tests below.

If a test harness is added in the future, the following should be verified:
- Constructing `TrackMapModel` with no sessions yields `hasData == false` and all bounds/center doubles equal to `0.0`.
- After `rebuild()` with visible sessions, the six doubles match the expected bounding box and centroid of the track data.
- After `rebuild()` with all sessions hidden, the doubles reset to `0.0`.

### Integration Tests

- The project builds successfully with `cmake --build` after the changes. The removal of `QGeoCoordinate`/`QGeoRectangle` from the header should not break any other translation unit. Verify by checking that `TrackMapModel.h` is only included in:
  - `src/ui/docks/map/TrackMapModel.cpp` (modified in this phase)
  - `src/ui/docks/map/MapWidget.cpp` (does not use the `center`/`bounds` properties directly -- it passes the model pointer to QML context)

### Manual Verification

**Important:** After this phase, the map dock will be temporarily non-functional because the QML file (`MapDock.qml`) still references `trackModel.center` (a `QGeoCoordinate`) and `trackModel.bounds` (a `QGeoRectangle`), which no longer exist. This is expected and will be resolved in Phase 3 when the QML is replaced with `QWebEngineView`.

To verify that the refactor is correct without a working map:

1. Build the project -- it must compile and link without errors.
2. Launch the application and load a session with GNSS data.
3. Open the map dock -- it will likely show errors in the debug console (QML binding failures for `trackModel.center` and `trackModel.bounds`). This is expected and acceptable.
4. Verify no crashes occur when the map dock is opened or when sessions are added/removed.
5. Verify that all other docks (plots, video, legend) continue to work normally.

## Notes for Implementer

### Gotchas

- **Antimeridian crossing:** The simple `(maxLon + minLon) / 2.0` center computation does not handle tracks that cross the 180th meridian. `QGeoRectangle::center()` does handle this internally. However, FlySight tracks (skydiving/BASE jumping) will never cross the antimeridian, so this simplification is safe. If this ever becomes a concern, a more sophisticated midpoint calculation would be needed.

- **QML breakage is intentional.** After this phase, `MapDock.qml` will fail to bind to `trackModel.center` and `trackModel.bounds` because those properties no longer exist. This is expected -- Phase 3 replaces the QML entirely. Do not attempt to "fix" the QML to use the new properties.

- **`QGeoRectangle` default constructor vs `0.0` initialization.** A default-constructed `QGeoRectangle` is invalid (returns `false` from `isValid()`), while the new `0.0` defaults represent a valid point at (0, 0). The `hasData` property already indicates whether the bounds are meaningful, so consumers should check `hasData` before reading bounds. This existing pattern is unchanged.

- **Signal emission order.** The existing code emits `hasDataChanged`, `countChanged`, `centerChanged`, `boundsChanged` in that order. Preserve this order.

- **No changes to `MapCursorDotModel` or `MapCursorProxy`.** These files do not reference `QGeoCoordinate` or `QGeoRectangle` and require no modifications in this phase.

### Decisions Made

- **Use `0.0` defaults (not NaN).** While `qQNaN()` could signal "no data" more explicitly, the existing pattern uses the `hasData` boolean for this purpose. Using `0.0` is simpler and consistent with the rest of the codebase.

- **Keep signal names unchanged.** The `centerChanged()` and `boundsChanged()` signals carry no parameters and can be reused with the new property types without breaking any signal-slot connections. Renaming them would add unnecessary churn.

- **Simple arithmetic mean for center.** `(max + min) / 2.0` is equivalent to `QGeoRectangle::center()` for non-antimeridian-crossing rectangles, which covers all realistic FlySight use cases.

- **Comment-only change for MapPreferencesBridge.** The overview document says "No structural change needed" for `MapPreferencesBridge`. The only change is updating comments to document the new Google Maps type index mapping, since the old OSM mapping is no longer relevant.

### Open Questions

None -- all design decisions for this phase are resolved by the overview document.

## Definition of Done

This phase is complete when:
1. All three tasks have passing acceptance criteria
2. `TrackMapModel.h` contains no references to `QGeoCoordinate` or `QGeoRectangle`
3. `TrackMapModel.cpp` computes bounds and center using plain doubles
4. `MapPreferencesBridge.h` documents the Google Maps type index mapping
5. The project compiles and links without errors
6. No functional changes are made beyond what is specified (model roles, rebuild loop logic, preference handling are all unchanged)
7. No TODOs or placeholder code remains
