# Phase 3: MapBridge & MapWidget Rewrite

## Overview

This phase creates a new `MapBridge` QObject that serves as the sole communication channel between C++ and the Google Maps JavaScript running inside a `QWebEngineView`, then rewrites `MapWidget` to replace its current `QQuickWidget` + QML setup with a `QWebEngineView` + `QWebChannel` setup. After this phase, the map dock will load the HTML page (created in Phase 4) and all data flow between the C++ models and the JS map will pass through `MapBridge`.

## Dependencies

- **Depends on:** Phase 1 (Build System -- WebEngineWidgets and WebChannel are available to link), Phase 2 (Model Layer -- TrackMapModel uses plain doubles for bounds/center)
- **Blocks:** Phase 4 (Google Maps HTML/JS Implementation -- needs `MapBridge` registered on a `QWebChannel` and `MapWidget` loading the HTML page)
- **Assumptions:**
  - After Phase 1, `Qt::WebEngineWidgets` and `Qt::WebChannel` are in `find_package` and `target_link_libraries`.
  - After Phase 1, `map.html` is produced by `configure_file` from `map.html.in` and installed to a `resources/` subdirectory alongside the binary.
  - After Phase 2, `TrackMapModel` exposes `centerLat()`, `centerLon()`, `boundsNorth()`, `boundsSouth()`, `boundsEast()`, `boundsWest()` as plain `double` accessors, and the `centerChanged()` / `boundsChanged()` signals are unchanged (no-argument).
  - The `MapCursorDotModel`, `MapCursorProxy`, and `MapPreferencesBridge` classes are unchanged from their current state.

## Tasks

### Task 3.1: Create MapBridge header

**Purpose:** Define the `MapBridge` QObject class that exposes C++ signals (for JS to listen to) and invokable slots (for JS to call) over `QWebChannel`.

**Files to create:**
- `src/ui/docks/map/MapBridge.h` -- QWebChannel bridge object header

**Technical Approach:**

1. Create the file inside the `FlySight` namespace, following the established header guard and namespace pattern from existing files in `src/ui/docks/map/` (e.g., `MapCursorProxy.h` lines 1-37).

2. Forward-declare the classes `MapCursorProxy` and `MapPreferencesBridge`. The bridge holds pointers to these but does not need their full definitions in the header.

3. Declare `MapBridge` as a `QObject` subclass with `Q_OBJECT` macro:

   ```cpp
   class MapBridge : public QObject
   {
       Q_OBJECT
   public:
       explicit MapBridge(MapCursorProxy *cursorProxy,
                          MapPreferencesBridge *preferencesBridge,
                          QObject *parent = nullptr);
   ```

4. Declare the four **C++ to JS signals** (JS listens to these via the QWebChannel proxy object):

   | Signal | Signature |
   |--------|-----------|
   | `tracksChanged` | `void tracksChanged(const QJsonArray &tracks)` |
   | `cursorDotsChanged` | `void cursorDotsChanged(const QJsonArray &dots)` |
   | `fitBounds` | `void fitBounds(double south, double west, double north, double east)` |
   | `preferenceChanged` | `void preferenceChanged(const QString &key, const QVariant &value)` |

5. Declare three **JS to C++ invokable slots** (JS calls these via the QWebChannel proxy object):

   | Slot | Signature |
   |------|-----------|
   | `onMapHover` | `Q_INVOKABLE void onMapHover(const QString &sessionId, double utcSeconds)` |
   | `onMapHoverClear` | `Q_INVOKABLE void onMapHoverClear()` |
   | `onMapTypeChanged` | `Q_INVOKABLE void onMapTypeChanged(int index)` |

6. Declare two **public methods** that `MapWidget` will call to push data into the bridge (which in turn emits the signals that JS receives):

   ```cpp
   void pushTracks(const QJsonArray &tracks);
   void pushCursorDots(const QJsonArray &dots);
   void pushFitBounds(double south, double west, double north, double east);
   void pushPreference(const QString &key, const QVariant &value);
   ```

   These are thin wrappers that simply emit the corresponding signal. They exist so `MapWidget` has a clear imperative API to call rather than emitting signals on another object directly.

7. Declare private member pointers:

   ```cpp
   private:
       MapCursorProxy *m_cursorProxy = nullptr;
       MapPreferencesBridge *m_preferencesBridge = nullptr;
   ```

8. Required includes: `<QObject>`, `<QJsonArray>`, `<QString>`, `<QVariant>`.

**Acceptance Criteria:**
- [ ] `src/ui/docks/map/MapBridge.h` exists with header guard `MAPBRIDGE_H`
- [ ] Class is inside `namespace FlySight`
- [ ] Four signals declared: `tracksChanged`, `cursorDotsChanged`, `fitBounds`, `preferenceChanged` with correct parameter types
- [ ] Three `Q_INVOKABLE` slots declared: `onMapHover`, `onMapHoverClear`, `onMapTypeChanged`
- [ ] Four `push*` public methods declared
- [ ] Constructor takes `MapCursorProxy*` and `MapPreferencesBridge*`
- [ ] Private member pointers for `m_cursorProxy` and `m_preferencesBridge`
- [ ] File compiles when included (no missing forward declarations or includes)

**Complexity:** S

---

### Task 3.2: Create MapBridge implementation

**Purpose:** Implement the `MapBridge` methods -- the invokable slots delegate to existing model objects, and the `push*` methods emit signals toward JS.

**Files to create:**
- `src/ui/docks/map/MapBridge.cpp` -- QWebChannel bridge object implementation

**Technical Approach:**

1. Include `MapBridge.h`, `MapCursorProxy.h`, and `MapPreferencesBridge.h`.

2. **Constructor:** Store the two pointers. No signal/slot connections are made here -- `MapWidget` handles the wiring.

   ```cpp
   MapBridge::MapBridge(MapCursorProxy *cursorProxy,
                        MapPreferencesBridge *preferencesBridge,
                        QObject *parent)
       : QObject(parent)
       , m_cursorProxy(cursorProxy)
       , m_preferencesBridge(preferencesBridge)
   {
   }
   ```

3. **`onMapHover` implementation:** Delegate to `MapCursorProxy::setMapHover`. Follow the pattern in `MapCursorProxy.cpp` lines 22-39 where `setMapHover` is the full implementation:

   ```cpp
   void MapBridge::onMapHover(const QString &sessionId, double utcSeconds)
   {
       if (m_cursorProxy)
           m_cursorProxy->setMapHover(sessionId, utcSeconds);
   }
   ```

4. **`onMapHoverClear` implementation:** Delegate to `MapCursorProxy::clearMapHover`:

   ```cpp
   void MapBridge::onMapHoverClear()
   {
       if (m_cursorProxy)
           m_cursorProxy->clearMapHover();
   }
   ```

5. **`onMapTypeChanged` implementation:** Delegate to `MapPreferencesBridge::setMapTypeIndex`:

   ```cpp
   void MapBridge::onMapTypeChanged(int index)
   {
       if (m_preferencesBridge)
           m_preferencesBridge->setMapTypeIndex(index);
   }
   ```

6. **`push*` methods:** Each simply emits the corresponding signal:

   ```cpp
   void MapBridge::pushTracks(const QJsonArray &tracks)
   {
       emit tracksChanged(tracks);
   }

   void MapBridge::pushCursorDots(const QJsonArray &dots)
   {
       emit cursorDotsChanged(dots);
   }

   void MapBridge::pushFitBounds(double south, double west, double north, double east)
   {
       emit fitBounds(south, west, north, east);
   }

   void MapBridge::pushPreference(const QString &key, const QVariant &value)
   {
       emit preferenceChanged(key, value);
   }
   ```

**Acceptance Criteria:**
- [ ] `src/ui/docks/map/MapBridge.cpp` exists inside `namespace FlySight`
- [ ] `onMapHover` calls `m_cursorProxy->setMapHover(sessionId, utcSeconds)` with a null guard
- [ ] `onMapHoverClear` calls `m_cursorProxy->clearMapHover()` with a null guard
- [ ] `onMapTypeChanged` calls `m_preferencesBridge->setMapTypeIndex(index)` with a null guard
- [ ] Each `push*` method emits the corresponding signal with the same arguments
- [ ] File compiles and links without errors

**Complexity:** S

---

### Task 3.3: Add MapBridge source files to CMakeLists.txt

**Purpose:** Register the new `MapBridge.h` and `MapBridge.cpp` files in the build system so they are compiled and linked.

**Files to modify:**
- `src/CMakeLists.txt` -- add new source file entries

**Technical Approach:**

1. In `src/CMakeLists.txt`, locate the map dock source file block (lines 445-450). Currently:

   ```cmake
   ui/docks/map/MapDockFeature.h    ui/docks/map/MapDockFeature.cpp
   ui/docks/map/MapWidget.h    ui/docks/map/MapWidget.cpp
   ui/docks/map/TrackMapModel.h    ui/docks/map/TrackMapModel.cpp
   ui/docks/map/MapCursorDotModel.h    ui/docks/map/MapCursorDotModel.cpp
   ui/docks/map/MapCursorProxy.h    ui/docks/map/MapCursorProxy.cpp
   ui/docks/map/MapPreferencesBridge.h    ui/docks/map/MapPreferencesBridge.cpp
   ```

2. Add the new entry immediately after the `MapWidget` line (line 446), following the same `header    source` spacing pattern:

   ```cmake
   ui/docks/map/MapBridge.h    ui/docks/map/MapBridge.cpp
   ```

   This places `MapBridge` logically between `MapWidget` (which creates it) and `TrackMapModel` (which provides data to it).

**Acceptance Criteria:**
- [ ] `ui/docks/map/MapBridge.h` and `ui/docks/map/MapBridge.cpp` appear in the `PROJECT_SOURCES` list in `src/CMakeLists.txt`
- [ ] The entry follows the existing `header    source` formatting convention (tab-separated, same indentation)
- [ ] CMake configure succeeds after this change

**Complexity:** S

---

### Task 3.4: Rewrite MapWidget header for QWebEngineView

**Purpose:** Replace the QQuickWidget-oriented header with one that adds member pointers for `QWebEngineView`, `QWebChannel`, and `MapBridge`, while retaining the existing model/proxy member pointers.

**Files to modify:**
- `src/ui/docks/map/MapWidget.h` -- update includes, add forward declarations, add new member pointers

**Technical Approach:**

1. **Update the class doc comment** (lines 16-19). Replace:
   ```cpp
   /**
    * QWidget wrapper around a QML Map (Qt Location) that overlays
    * raw GNSS tracks for all visible sessions.
    */
   ```
   With:
   ```cpp
   /**
    * QWidget wrapper around a QWebEngineView that displays a Google Maps
    * instance with GNSS track overlays for all visible sessions.
    * Communication with the JS page goes through MapBridge via QWebChannel.
    */
   ```

2. **Add forward declarations** for the new types. The existing forward declarations block (lines 8-14) currently includes:
   ```cpp
   class SessionModel;
   class TrackMapModel;
   class MapCursorDotModel;
   class MapCursorProxy;
   class MapPreferencesBridge;
   class CursorModel;
   class PlotRangeModel;
   ```

   Add `MapBridge` to this block:
   ```cpp
   class MapBridge;
   ```

   Also add Qt forward declarations outside the namespace (before `namespace FlySight`):
   ```cpp
   class QWebEngineView;
   class QWebChannel;
   ```

3. **Add new private member pointers** (after the existing members at lines 30-34):
   ```cpp
   QWebEngineView *m_webView = nullptr;
   QWebChannel *m_channel = nullptr;
   MapBridge *m_bridge = nullptr;
   ```

4. **Add private helper methods** for serializing model data to JSON and pushing it to the bridge:
   ```cpp
   private slots:
       void onTracksReset();
       void onCursorDotsReset();
       void onBoundsChanged();
       void onPreferenceChanged(const QString &key, const QVariant &value);
   ```

   These slots will be connected to the model signals and will serialize data then call the appropriate `MapBridge::push*` method.

5. **Keep the existing constructor signature unchanged** -- `MapDockFeature.cpp` (line 17) creates `MapWidget` with the same arguments:
   ```cpp
   explicit MapWidget(SessionModel *sessionModel,
                      CursorModel *cursorModel,
                      PlotRangeModel *rangeModel,
                      QWidget *parent = nullptr);
   ```

6. **Keep the existing private member pointers** for `m_trackModel`, `m_cursorDotModel`, `m_cursorProxy`, `m_preferencesBridge`, and `m_cursorModel`. These are still created and owned by `MapWidget`.

**Acceptance Criteria:**
- [ ] No references to `QQuickWidget`, `QQmlContext`, or `QQmlEngine` in the header
- [ ] Forward declarations for `QWebEngineView`, `QWebChannel`, and `MapBridge` are present
- [ ] Private member pointers `m_webView`, `m_channel`, `m_bridge` are declared
- [ ] Private slots `onTracksReset`, `onCursorDotsReset`, `onBoundsChanged`, `onPreferenceChanged` are declared
- [ ] Existing member pointers (`m_trackModel`, `m_cursorDotModel`, `m_cursorProxy`, `m_preferencesBridge`, `m_cursorModel`) are preserved
- [ ] Constructor signature is unchanged
- [ ] File compiles when included

**Complexity:** S

---

### Task 3.5: Rewrite MapWidget implementation for QWebEngineView

**Purpose:** Replace the `QQuickWidget` + QML context-property setup in `MapWidget.cpp` with a `QWebEngineView` + `QWebChannel` + `MapBridge` setup, and implement the data serialization slots that push model data to the bridge as JSON.

**Files to modify:**
- `src/ui/docks/map/MapWidget.cpp` -- complete rewrite of constructor and addition of slot implementations

**Technical Approach:**

1. **Replace includes** (lines 1-15). Remove:
   ```cpp
   #include <QQuickWidget>
   #include <QQmlContext>
   #include <QQmlEngine>
   ```
   Add:
   ```cpp
   #include <QWebEngineView>
   #include <QWebChannel>
   #include <QJsonArray>
   #include <QJsonObject>
   ```
   Also add:
   ```cpp
   #include "MapBridge.h"
   ```
   Keep existing includes: `"TrackMapModel.h"`, `"MapCursorDotModel.h"`, `"MapCursorProxy.h"`, `"MapPreferencesBridge.h"`, `"sessionmodel.h"`, `"plotrangemodel.h"`, `<QVBoxLayout>`, `<QDebug>`, `<QCoreApplication>`.

2. **Rewrite the constructor** (lines 19-64). The new constructor should follow this pattern, which mirrors the old constructor's structure (create layout, create models, create view widget, configure, add to layout):

   a. Create the layout (same as before):
      ```cpp
      auto *layout = new QVBoxLayout(this);
      layout->setContentsMargins(0, 0, 0, 0);
      ```

   b. Create the four model/proxy objects (same as before, lines 30-41 pattern):
      ```cpp
      m_trackModel = new TrackMapModel(sessionModel, rangeModel, this);
      m_cursorDotModel = new MapCursorDotModel(sessionModel, m_cursorModel, this);
      m_cursorProxy = new MapCursorProxy(sessionModel, m_cursorModel, this);
      m_preferencesBridge = new MapPreferencesBridge(this);
      ```

   c. Create the `MapBridge`, passing `m_cursorProxy` and `m_preferencesBridge`:
      ```cpp
      m_bridge = new MapBridge(m_cursorProxy, m_preferencesBridge, this);
      ```

   d. Create the `QWebChannel` and register the bridge:
      ```cpp
      m_channel = new QWebChannel(this);
      m_channel->registerObject(QStringLiteral("bridge"), m_bridge);
      ```

   e. Create the `QWebEngineView` and set the channel on its page:
      ```cpp
      m_webView = new QWebEngineView(this);
      m_webView->page()->setWebChannel(m_channel);
      ```

   f. Load the map HTML from the filesystem. The file is installed to a `resources/` subdirectory next to the binary (per Phase 1, Task 1.3):
      ```cpp
      const QString htmlPath = QCoreApplication::applicationDirPath()
                             + QStringLiteral("/resources/map.html");
      m_webView->load(QUrl::fromLocalFile(htmlPath));
      ```

   g. Add the web view to the layout:
      ```cpp
      layout->addWidget(m_webView);
      ```

   h. Connect model/preference signals to the serialization slots:
      ```cpp
      // TrackMapModel resets (full track data changed)
      connect(m_trackModel, &TrackMapModel::modelReset,
              this, &MapWidget::onTracksReset);

      // MapCursorDotModel resets (cursor dots changed)
      connect(m_cursorDotModel, &MapCursorDotModel::modelReset,
              this, &MapWidget::onCursorDotsReset);

      // Bounds changed (auto-zoom)
      connect(m_trackModel, &TrackMapModel::boundsChanged,
              this, &MapWidget::onBoundsChanged);

      // Preference changes
      connect(m_preferencesBridge, &MapPreferencesBridge::lineThicknessChanged,
              this, [this]() {
                  m_bridge->pushPreference(QStringLiteral("map/lineThickness"),
                                           m_preferencesBridge->lineThickness());
              });
      connect(m_preferencesBridge, &MapPreferencesBridge::markerSizeChanged,
              this, [this]() {
                  m_bridge->pushPreference(QStringLiteral("map/markerSize"),
                                           m_preferencesBridge->markerSize());
              });
      connect(m_preferencesBridge, &MapPreferencesBridge::trackOpacityChanged,
              this, [this]() {
                  m_bridge->pushPreference(QStringLiteral("map/trackOpacity"),
                                           m_preferencesBridge->trackOpacity());
              });
      connect(m_preferencesBridge, &MapPreferencesBridge::mapTypeIndexChanged,
              this, [this]() {
                  m_bridge->pushPreference(QStringLiteral("map/type"),
                                           m_preferencesBridge->mapTypeIndex());
              });
      ```

      **Note on `modelReset` signal:** `QAbstractListModel` (the base class of both `TrackMapModel` and `MapCursorDotModel`) inherits `modelReset()` from `QAbstractItemModel`. The `rebuild()` methods in both models call `beginResetModel()` / `endResetModel()`, which causes `modelReset` to be emitted automatically. This is the correct signal to connect to for full-data push updates.

3. **Implement `onTracksReset`:** Serialize `TrackMapModel` data into a `QJsonArray` and push to the bridge. Iterate over all rows in the model, extract `sessionId`, `trackPoints`, and `trackColor` using the model's `data()` method with the defined roles:

   ```cpp
   void MapWidget::onTracksReset()
   {
       QJsonArray tracks;
       const int n = m_trackModel->rowCount();
       for (int row = 0; row < n; ++row) {
           const QModelIndex idx = m_trackModel->index(row, 0);

           const QString sessionId =
               m_trackModel->data(idx, TrackMapModel::SessionIdRole).toString();
           const QVariantList pointsVar =
               m_trackModel->data(idx, TrackMapModel::TrackPointsRole).toList();
           const QColor color =
               m_trackModel->data(idx, TrackMapModel::TrackColorRole).value<QColor>();

           QJsonArray jsonPoints;
           for (const QVariant &ptVar : pointsVar) {
               const QVariantMap pt = ptVar.toMap();
               QJsonObject jsonPt;
               jsonPt.insert(QStringLiteral("lat"), pt.value(QStringLiteral("lat")).toDouble());
               jsonPt.insert(QStringLiteral("lon"), pt.value(QStringLiteral("lon")).toDouble());
               jsonPt.insert(QStringLiteral("t"),   pt.value(QStringLiteral("t")).toDouble());
               jsonPoints.append(jsonPt);
           }

           QJsonObject track;
           track.insert(QStringLiteral("sessionId"), sessionId);
           track.insert(QStringLiteral("points"), jsonPoints);
           track.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
           tracks.append(track);
       }
       m_bridge->pushTracks(tracks);
   }
   ```

   The color is serialized as `#AARRGGBB` hex using `QColor::name(QColor::HexArgb)` so JS can parse it. The JS side in Phase 4 will need to handle this format.

4. **Implement `onCursorDotsReset`:** Serialize `MapCursorDotModel` data into a `QJsonArray`:

   ```cpp
   void MapWidget::onCursorDotsReset()
   {
       QJsonArray dots;
       const int n = m_cursorDotModel->rowCount();
       for (int row = 0; row < n; ++row) {
           const QModelIndex idx = m_cursorDotModel->index(row, 0);

           QJsonObject dot;
           dot.insert(QStringLiteral("sessionId"),
                      m_cursorDotModel->data(idx, MapCursorDotModel::SessionIdRole).toString());
           dot.insert(QStringLiteral("lat"),
                      m_cursorDotModel->data(idx, MapCursorDotModel::LatRole).toDouble());
           dot.insert(QStringLiteral("lon"),
                      m_cursorDotModel->data(idx, MapCursorDotModel::LonRole).toDouble());
           dot.insert(QStringLiteral("color"),
                      m_cursorDotModel->data(idx, MapCursorDotModel::ColorRole).value<QColor>()
                          .name(QColor::HexArgb));
           dots.append(dot);
       }
       m_bridge->pushCursorDots(dots);
   }
   ```

5. **Implement `onBoundsChanged`:** Read the plain-double bounds from `TrackMapModel` (provided by Phase 2) and push to the bridge:

   ```cpp
   void MapWidget::onBoundsChanged()
   {
       if (!m_trackModel->hasData())
           return;
       m_bridge->pushFitBounds(m_trackModel->boundsSouth(),
                               m_trackModel->boundsWest(),
                               m_trackModel->boundsNorth(),
                               m_trackModel->boundsEast());
   }
   ```

   The `hasData()` guard prevents sending a (0,0)-(0,0) bounding box when no tracks are loaded.

6. **Remove all QML-related code.** None of the following should remain:
   - `QQuickWidget` creation (old line 42)
   - `setResizeMode` (old line 43)
   - `addImportPath` (old line 46)
   - `setContextProperty` calls (old lines 49-52)
   - `setSource` (old line 54)
   - QML error checking block (old lines 56-61)

**Acceptance Criteria:**
- [ ] No references to `QQuickWidget`, `QQmlContext`, `QQmlEngine`, or `setSource` in the file
- [ ] `#include <QWebEngineView>` and `#include <QWebChannel>` are present
- [ ] `#include "MapBridge.h"` is present
- [ ] Constructor creates `MapBridge` and passes `m_cursorProxy` and `m_preferencesBridge`
- [ ] Constructor creates `QWebChannel`, registers `m_bridge` as `"bridge"`
- [ ] Constructor creates `QWebEngineView`, sets web channel on its page, loads `map.html` via `QUrl::fromLocalFile`
- [ ] `modelReset` signal from `TrackMapModel` is connected to `onTracksReset`
- [ ] `modelReset` signal from `MapCursorDotModel` is connected to `onCursorDotsReset`
- [ ] `boundsChanged` signal from `TrackMapModel` is connected to `onBoundsChanged`
- [ ] Preference change signals are connected to lambdas that call `m_bridge->pushPreference`
- [ ] `onTracksReset` serializes all model rows into a `QJsonArray` of `{sessionId, points, color}` and calls `m_bridge->pushTracks`
- [ ] `onCursorDotsReset` serializes all dot model rows into a `QJsonArray` of `{sessionId, lat, lon, color}` and calls `m_bridge->pushCursorDots`
- [ ] `onBoundsChanged` reads the plain-double bounds from `TrackMapModel` and calls `m_bridge->pushFitBounds`
- [ ] Color values are serialized as `#AARRGGBB` hex strings using `QColor::name(QColor::HexArgb)`
- [ ] File compiles and links without errors

**Complexity:** L

---

## Testing Requirements

### Unit Tests

No unit test framework is currently active in this project for map dock classes. However, the following properties of `MapBridge` are unit-testable if a harness is added in the future:

- Calling `onMapHover("sess1", 1234.0)` on `MapBridge` results in `MapCursorProxy::setMapHover("sess1", 1234.0)` being called.
- Calling `onMapHoverClear()` on `MapBridge` results in `MapCursorProxy::clearMapHover()` being called.
- Calling `onMapTypeChanged(2)` on `MapBridge` results in `MapPreferencesBridge::setMapTypeIndex(2)` being called.
- Calling `pushTracks(jsonArray)` emits the `tracksChanged` signal with the same `QJsonArray`.

### Integration Tests

- **Build test:** The project must compile and link successfully with the new `MapBridge` files and the rewritten `MapWidget`. Run `cmake --build build` and verify no errors.
- **Signal wiring test:** Load a session with GNSS data. Verify (via `qDebug` statements or a debugger) that:
  - `onTracksReset` is called when `TrackMapModel::rebuild()` fires
  - `onCursorDotsReset` is called when cursor position changes
  - `onBoundsChanged` is called when tracks are loaded

### Manual Verification

After this phase, the map dock will display a web page (the Phase 1 placeholder `map.html` content), not a functional Google Maps view. Full visual testing requires Phase 4. However, the following can be verified:

1. **Build succeeds:** `cmake --build build` completes without errors.
2. **Application launches:** The application starts without crashing.
3. **Map dock opens:** Opening the map dock shows the placeholder HTML content (or a blank page) in the `QWebEngineView` rather than the old QML-based map.
4. **No crashes on session load:** Loading a session with GNSS data does not crash. The serialization slots fire but the JS side does nothing (the placeholder HTML has no QWebChannel listener).
5. **Other docks unaffected:** Plot docks, video dock, and legend continue to function normally.

## Notes for Implementer

### Gotchas

- **`modelReset` is emitted by `endResetModel()`.** Both `TrackMapModel::rebuild()` and `MapCursorDotModel::rebuild()` use `beginResetModel()`/`endResetModel()`, which automatically emits the `modelReset` signal inherited from `QAbstractItemModel`. Do not attempt to emit a custom signal -- connect directly to `modelReset`.

- **`QWebEngineView` requires `#include <QWebEngineView>` in the .cpp file**, not just a forward declaration. The header can use a forward declaration (`class QWebEngineView;`) because it only holds a pointer, but the .cpp must include the full header to call methods like `page()` and `load()`.

- **`QWebChannel::registerObject` uses a string key.** The key `"bridge"` must match what the JS side uses to access the object. Phase 4's JavaScript will do `new QWebChannel(qt.webChannelTransport, function(channel) { var bridge = channel.objects.bridge; ... })`. The string must be exactly `"bridge"`.

- **`QColor::name(QColor::HexArgb)` produces `#AARRGGBB` format.** This is not CSS-standard (CSS uses `rgba()` or `#RRGGBBAA`). The JS side in Phase 4 will need to parse this format or convert it. Document this in the Phase 4 notes.

- **`QUrl::fromLocalFile` path must be correct at runtime.** On Windows, `QCoreApplication::applicationDirPath()` returns the directory containing the `.exe` (e.g., `C:/build/install/`). The `map.html` file must be at `resources/map.html` relative to that directory. If using `cmake --build build --target install`, verify the install rules from Phase 1 place the file correctly. During development (running from the build directory), you may need to ensure the file is present. A common pattern is to also copy it to the build output directory during the build step.

- **Initial data push timing.** When `MapWidget` is constructed, the models fire their initial `rebuild()` (see `TrackMapModel` constructor line 43 and `MapCursorDotModel` constructor line 270). However, the `QWebEngineView` has not yet loaded the page and the JS QWebChannel is not yet initialized. The initial data push will be emitted as signals but JS will not receive them. Phase 4 must handle this by having JS request initial data from the bridge once the channel is established (e.g., an invokable `requestInitialData()` slot). **For this phase**, do not worry about this timing issue -- just wire the connections. Phase 4 will address the initialization handshake.

- **Thread safety.** `QWebEngineView` requires all interaction on the GUI thread, which is where `MapWidget` lives. No special threading considerations are needed.

- **The `map.html` path construction uses forward slashes.** `QUrl::fromLocalFile` handles platform differences, but the path string itself should use `/` (Qt handles cross-platform path separators).

### Decisions Made

- **`MapBridge` takes `MapCursorProxy*` and `MapPreferencesBridge*` in its constructor** rather than `SessionModel*`, `CursorModel*`, etc. This keeps `MapBridge` thin -- it delegates to the existing proxy/bridge objects rather than reimplementing their logic. The bridge does not need direct model access for the JS-to-C++ direction.

- **`push*` methods are separate from signal emission** so that `MapWidget` has a clear imperative call pattern. This avoids the anti-pattern of one object directly emitting another object's signals.

- **Color serialization uses `HexArgb` format** (`#AARRGGBB`) because `QColor::name()` without arguments drops the alpha channel, and track colors include alpha (from `trackOpacity`). The JS side will need to handle this format.

- **Preference change forwarding uses individual `connect` calls with lambdas** rather than a single `onPreferenceChanged` slot. This keeps the preference key strings in `MapWidget` rather than spreading them across `MapBridge`, and allows `MapBridge` to remain unaware of specific preference keys. The key strings passed to JS (`"map/lineThickness"`, `"map/markerSize"`, `"map/trackOpacity"`, `"map/type"`) follow the `PreferenceKeys` naming convention but are hardcoded strings because they form a contract with the JS side.

- **Data serialization lives in `MapWidget`, not `MapBridge`.** `MapWidget` owns the models and knows how to iterate them. `MapBridge` is a pass-through that does not depend on `TrackMapModel` or `MapCursorDotModel` headers, keeping it lightweight.

- **No `requestInitialData` slot is added in this phase.** The JS initialization handshake is a Phase 4 concern. This phase only wires the reactive (signal-driven) data flow.

### Open Questions

- **Development-time `map.html` location:** When running the application directly from the build directory (without `cmake --install`), the `map.html` file may not be in the expected `resources/` subdirectory. The implementer may need to add a CMake `add_custom_command` or post-build copy step to place the configured `map.html` next to the built binary. This is a Phase 1 concern but may surface during Phase 3 testing. If the file is not found, the `QWebEngineView` will show a "file not found" error, which is non-fatal.

## Definition of Done

This phase is complete when:
1. All tasks have passing acceptance criteria
2. `MapBridge.h` and `MapBridge.cpp` exist and compile correctly
3. `MapBridge` is registered in `src/CMakeLists.txt`
4. `MapWidget.h` and `MapWidget.cpp` are fully rewritten to use `QWebEngineView` + `QWebChannel` + `MapBridge`
5. No references to `QQuickWidget`, `QQmlContext`, `QQmlEngine`, or QML loading remain in `MapWidget`
6. Model data is serialized to `QJsonArray` and pushed through `MapBridge` signals
7. JS-to-C++ invocable slots (`onMapHover`, `onMapHoverClear`, `onMapTypeChanged`) delegate to existing proxy/bridge objects
8. The project builds and links successfully on all target platforms
9. The application launches without crashing and the map dock shows the placeholder HTML
10. No TODOs or placeholder code remains
