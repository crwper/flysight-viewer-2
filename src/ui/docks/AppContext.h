#ifndef APPCONTEXT_H
#define APPCONTEXT_H

class QSettings;

namespace FlySight {

class SessionModel;
class PlotModel;
class MarkerModel;
class CursorModel;
class PlotRangeModel;
class PlotViewSettingsModel;

/**
 * Bundles all shared services that dock features may need.
 * This is a simple struct, not a service locator.
 * All pointers are non-owning; lifetime is managed by MainWindow.
 */
struct AppContext {
    SessionModel* sessionModel = nullptr;
    PlotModel* plotModel = nullptr;
    MarkerModel* markerModel = nullptr;
    CursorModel* cursorModel = nullptr;
    PlotRangeModel* rangeModel = nullptr;
    PlotViewSettingsModel* plotViewSettings = nullptr;
    QSettings* settings = nullptr;
};

} // namespace FlySight

#endif // APPCONTEXT_H
