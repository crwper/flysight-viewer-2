#ifndef MAPBRIDGE_H
#define MAPBRIDGE_H

#include <QObject>
#include <QJsonArray>
#include <QString>
#include <QVariant>

namespace FlySight {

class MapCursorProxy;
class MapPreferencesBridge;

/**
 * QWebChannel bridge object that serves as the sole communication channel
 * between C++ and the Google Maps JavaScript running inside a QWebEngineView.
 *
 * Signals (C++ -> JS):
 *   tracksChanged, cursorDotsChanged, fitBounds, preferenceChanged
 *
 * Invokable slots (JS -> C++):
 *   onMapHover, onMapHoverClear, onMapTypeChanged
 */
class MapBridge : public QObject
{
    Q_OBJECT
public:
    explicit MapBridge(MapCursorProxy *cursorProxy,
                       MapPreferencesBridge *preferencesBridge,
                       QObject *parent = nullptr);

    // Called by MapWidget to push data to JS via signals
    void pushTracks(const QJsonArray &tracks);
    void pushCursorDots(const QJsonArray &dots);
    void pushFitBounds(double south, double west, double north, double east);
    void pushPreference(const QString &key, const QVariant &value);

signals:
    // C++ -> JS signals (JS listens to these via QWebChannel)
    void tracksChanged(const QJsonArray &tracks);
    void cursorDotsChanged(const QJsonArray &dots);
    void fitBounds(double south, double west, double north, double east);
    void preferenceChanged(const QString &key, const QVariant &value);

    // Emitted when JS requests initial data after page load
    void initialDataRequested();

public slots:
    // JS -> C++ invokable slots (JS calls these via QWebChannel)
    Q_INVOKABLE void onMapHover(const QString &sessionId, double utcSeconds);
    Q_INVOKABLE void onMapHoverClear();
    Q_INVOKABLE void onMapTypeChanged(int index);
    Q_INVOKABLE void requestInitialData();

private:
    MapCursorProxy *m_cursorProxy = nullptr;
    MapPreferencesBridge *m_preferencesBridge = nullptr;
};

} // namespace FlySight

#endif // MAPBRIDGE_H
