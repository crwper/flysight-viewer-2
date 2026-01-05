Yep — if you’re aiming for “100 dockables and MainWindow stays boring,” the scalable move is to **introduce a feature/module layer** between `MainWindow` and each dock. MainWindow becomes the **composition root** that creates shared services once, then instantiates a list of *dock features* that each know how to build themselves.

Below is a naming + folder convention that stays consistent as you add more docks, plus how the wiring works.

---

## The core idea: Dock Features (one per dock)

Create a “feature object” per dock that owns **everything dock-specific**:

* the `DockWidget` container
* the view widget (`XxxWidget`)
* presenters/controllers/adapters
* local presentation models
* QML registrations / context properties
* any dock-specific actions/menus/shortcuts
* save/restore of dock-specific UI state (if needed)

MainWindow then only does:

* build shared app services/models once
* ask “dock registry” to construct features
* add the resulting docks to the docking system
* optionally build menus from feature metadata

That means MainWindow doesn’t need to know what a legend requires vs a map requires vs a future “wind profile” requires.

---

## Naming conventions

Keep the names aligned across all docks:

### View layer

* `XxxWidget` — the actual QWidget/QQuickWidget content

### Presentation models

Use `Model` only for Qt model/view sources (or observable state objects):

* `XxxYyyModel` — `QAbstractItemModel` or `QObject` view-model-ish state

Examples:

* `TrackMapModel` ✅
* `MapCursorDotModel` ✅

### Interaction / glue

Prefer role-based names:

* `XxxPresenter` — observe models → update view widget
* `XxxController` — handle input policy → write into shared models
* `XxxAdapter` / `XxxBridge` / `XxxProxy` — translate frameworks/coordinates (QML↔C++, pixels↔time, etc.)

Examples:

* `LegendPresenter` ✅
* `MapCursorController` (instead of `Proxy`, if it’s mostly policy)

### The feature/module wrapper

This is the scalable piece:

* `XxxDockFeature` (or `XxxFeature`, but I’d include “Dock” to signal it owns a dock)

---

## Folder structure (feature-first, scalable)

Put each dock’s implementation in its own folder:

```
src/
  core/                      # domain + data sources (sessions, parsing, calculations)
  app/                       # shared app services/models created once
  ui/
    docks/
      DockFeature.h          # base interface (pure virtual / abstract QObject)
      DockRegistry.h         # list of dock factories or static registration

      legend/
        LegendDockFeature.{h,cpp}
        LegendWidget.{h,cpp}
        LegendPresenter.{h,cpp}
        (optional) LegendModels...  # if you add more view models later

      map/
        MapDockFeature.{h,cpp}
        MapWidget.{h,cpp}
        TrackMapModel.{h,cpp}
        MapCursorDotModel.{h,cpp}
        MapCursorController.{h,cpp}
        qml/
          MapDock.qml
          MapCursorOverlay.qml     # optional if it grows

      plot/
        PlotDockFeature.{h,cpp}
        PlotWidget.{h,cpp}
        CrosshairManager.{h,cpp}
        plottools/
          ...
  shared/
    interaction/
      CursorModel.{h,cpp}
    settings/
      PlotViewSettingsModel.{h,cpp}
```

Why this stays intuitive for devs:

* “Want to change the map dock?” → go to `ui/docks/map/`
* Everything needed to build the map dock is co-located.
* Shared state stays shared (`shared/interaction`, `shared/settings`, etc.).
* The “dock feature” is the place to look for wiring.

---

## What `DockFeature` should look like

Minimal interface that lets MainWindow treat all docks uniformly:

* Construct with an `AppContext` (see below)
* Returns a `DockWidget*` to add to the layout
* Exposes metadata so menus/toolbars can be built without special-casing

A typical interface (conceptual):

```cpp
struct AppContext {
  SessionModel* sessionModel;
  PlotModel* plotModel;
  CursorModel* cursorModel;
  PlotViewSettingsModel* plotViewSettings;
  // ...whatever else is truly “global”
};

class DockFeature : public QObject {
  Q_OBJECT
public:
  virtual QString id() const = 0;        // "legend", "map"
  virtual QString title() const = 0;     // "Legend", "Map"
  virtual KDDockWidgets::DockWidget* dock() const = 0;

  // Optional:
  virtual QAction* toggleAction() const = 0;  // show/hide action for menus
};
```

MainWindow never asks “what presenter do you need?” It only deals with `DockFeature`.

---

## AppContext: how you keep dependencies explicit (no globals)

Instead of MainWindow passing 4–8 different pointers to each widget/presenter manually, it passes **one** context object.

* Features pull what they need from `AppContext`.
* Dependencies are still explicit (and testable), but MainWindow doesn’t care about per-dock details.

You can keep `AppContext` small and stable by only including truly shared services.

---

## DockRegistry: how MainWindow stays boring

Add a registry that lists which docks exist, and how to build them.

Two common approaches:

### Option A: Simple explicit list (still clean)

`DockRegistry::createAll(AppContext&, parent)` returns a list of `unique_ptr<DockFeature>`.

MainWindow does:

* create `AppContext ctx{...}`
* `m_features = DockRegistry::createAll(ctx, this);`
* for each feature: add its dock, add its toggleAction to a menu, etc.

### Option B: Self-registration (plugin-like)

Each feature registers a factory in a global registry. This is more complex but scales if you want optional docks, build flags, etc.

---

## What changes for your existing docks

### Legend

`LegendDockFeature` owns:

* `DockWidget*`
* `LegendWidget*`
* `LegendPresenter*`

MainWindow no longer knows `LegendPresenter` exists.

### Map

`MapDockFeature` owns:

* `DockWidget*`
* `MapWidget*` (host)
* `TrackMapModel*`
* future cursor stuff (`MapCursorController`, `MapCursorDotModel`) and QML hookups

Again: MainWindow doesn’t know any of those types exist.

---

## Naming cheat sheet you can apply everywhere

If you stick to this, contributors will instantly know where to put things:

* **`XxxDockFeature`**: owns composition + wiring for dock “Xxx”
* **`XxxWidget`**: view
* **`Xxx...Model`**: Qt-facing presentation model / observable view-state
* **`Xxx...Presenter`**: model→view updates
* **`Xxx...Controller`**: input→model updates
* **`Xxx...Adapter/Bridge`**: cross-framework translation (QML↔C++, pixel↔time)

---

## Why this scales to 100 docks

MainWindow complexity becomes roughly **O(number of shared services)**, not O(number of docks).

Adding a new dock becomes:

1. create `ui/docks/newdock/NewDockFeature`
2. add it to `DockRegistry`
3. (optional) add QML, models, presenters inside the folder

No MainWindow edits beyond “registry includes it,” and even that can be avoided with self-registration if desired.

---

If you want, I can also suggest a consistent **CMake target layout** (one target per dock feature folder, linked into the app), which tends to make large Qt apps much easier to navigate and build incrementally.
