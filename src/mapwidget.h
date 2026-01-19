#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>

namespace FlySight {

class SessionModel;
class TrackMapModel;
class MapCursorDotModel;
class MapCursorProxy;
class MapPreferencesBridge;
class CursorModel;
class PlotRangeModel;

/**
 * QWidget wrapper around a QML Map (Qt Location) that overlays
 * raw GNSS tracks for all visible sessions.
 */
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(SessionModel *sessionModel,
                       CursorModel *cursorModel,
                       PlotRangeModel *rangeModel,
                       QWidget *parent = nullptr);

private:
    TrackMapModel *m_trackModel = nullptr;
    MapCursorDotModel *m_cursorDotModel = nullptr;
    MapCursorProxy *m_cursorProxy = nullptr;
    MapPreferencesBridge *m_preferencesBridge = nullptr;
    CursorModel *m_cursorModel = nullptr;
};

} // namespace FlySight

#endif // MAPWIDGET_H
