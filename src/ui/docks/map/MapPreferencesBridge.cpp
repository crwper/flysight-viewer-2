#include "MapPreferencesBridge.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

MapPreferencesBridge::MapPreferencesBridge(QObject *parent)
    : QObject(parent)
{
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &MapPreferencesBridge::onPreferenceChanged);

    loadAllPreferences();
}

void MapPreferencesBridge::loadAllPreferences()
{
    auto &prefs = PreferencesManager::instance();

    m_lineThickness = prefs.getValue(PreferenceKeys::MapLineThickness).toDouble();
    m_largeDotSize = prefs.getValue(PreferenceKeys::MapLargeDotSize).toInt();
    m_smallDotSize = prefs.getValue(PreferenceKeys::MapSmallDotSize).toInt();
    m_trackOpacity = prefs.getValue(PreferenceKeys::MapTrackOpacity).toDouble();
    m_mapTypeIndex = prefs.getValue(PreferenceKeys::MapType).toInt();
}

void MapPreferencesBridge::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::MapLineThickness) {
        m_lineThickness = value.toDouble();
        emit lineThicknessChanged();
    }
    else if (key == PreferenceKeys::MapLargeDotSize) {
        m_largeDotSize = value.toInt();
        emit largeDotSizeChanged();
    }
    else if (key == PreferenceKeys::MapSmallDotSize) {
        m_smallDotSize = value.toInt();
        emit smallDotSizeChanged();
    }
    else if (key == PreferenceKeys::MapTrackOpacity) {
        m_trackOpacity = value.toDouble();
        emit trackOpacityChanged();
    }
    else if (key == PreferenceKeys::MapType) {
        m_mapTypeIndex = value.toInt();
        emit mapTypeIndexChanged();
    }
}

void MapPreferencesBridge::setMapTypeIndex(int index)
{
    if (m_mapTypeIndex == index)
        return;
    PreferencesManager::instance().setValue(PreferenceKeys::MapType, index);
}

} // namespace FlySight
