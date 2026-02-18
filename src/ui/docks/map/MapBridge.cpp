#include "MapBridge.h"
#include "MapCursorProxy.h"
#include "MapPreferencesBridge.h"

namespace FlySight {

MapBridge::MapBridge(MapCursorProxy *cursorProxy,
                     MapPreferencesBridge *preferencesBridge,
                     QObject *parent)
    : QObject(parent)
    , m_cursorProxy(cursorProxy)
    , m_preferencesBridge(preferencesBridge)
{
}

void MapBridge::onMapHover(const QString &sessionId, double utcSeconds)
{
    if (m_cursorProxy)
        m_cursorProxy->setMapHover(sessionId, utcSeconds);
}

void MapBridge::onMapHoverClear()
{
    if (m_cursorProxy)
        m_cursorProxy->clearMapHover();
}

void MapBridge::onMapTypeChanged(int index)
{
    if (m_preferencesBridge)
        m_preferencesBridge->setMapTypeIndex(index);
}

void MapBridge::requestInitialData()
{
    emit initialDataRequested();
}

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

} // namespace FlySight
