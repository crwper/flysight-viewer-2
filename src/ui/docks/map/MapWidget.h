#ifndef MAPWIDGET_H
#define MAPWIDGET_H

#include <QWidget>

class QWebEngineView;
class QWebChannel;

namespace FlySight {

class SessionModel;
class TrackMapModel;
class MapCursorDotModel;
class MapCursorProxy;
class MapPreferencesBridge;
class MapBridge;
class CursorModel;
class PlotRangeModel;

/**
 * QWidget wrapper around a QWebEngineView that displays Google Maps
 * with overlaid GNSS tracks for all visible sessions.
 *
 * Communication between C++ and the Google Maps JavaScript passes
 * through a MapBridge object registered on a QWebChannel.
 */
class MapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MapWidget(SessionModel *sessionModel,
                       CursorModel *cursorModel,
                       PlotRangeModel *rangeModel,
                       QWidget *parent = nullptr);

private slots:
    void onTracksReset();
    void onCursorDotsReset();
    void onBoundsChanged();
    void pushAllData();

private:
    TrackMapModel *m_trackModel = nullptr;
    MapCursorDotModel *m_cursorDotModel = nullptr;
    MapCursorProxy *m_cursorProxy = nullptr;
    MapPreferencesBridge *m_preferencesBridge = nullptr;
    CursorModel *m_cursorModel = nullptr;

    QWebEngineView *m_webView = nullptr;
    QWebChannel *m_channel = nullptr;
    MapBridge *m_bridge = nullptr;
};

} // namespace FlySight

#endif // MAPWIDGET_H
