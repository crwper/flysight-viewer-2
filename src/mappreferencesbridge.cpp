#include "mappreferencesbridge.h"
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
    m_markerSize = prefs.getValue(PreferenceKeys::MapMarkerSize).toInt();
    m_trackOpacity = prefs.getValue(PreferenceKeys::MapTrackOpacity).toDouble();
}

void MapPreferencesBridge::onPreferenceChanged(const QString &key, const QVariant &value)
{
    if (key == PreferenceKeys::MapLineThickness) {
        m_lineThickness = value.toDouble();
        emit lineThicknessChanged();
    }
    else if (key == PreferenceKeys::MapMarkerSize) {
        m_markerSize = value.toInt();
        emit markerSizeChanged();
    }
    else if (key == PreferenceKeys::MapTrackOpacity) {
        m_trackOpacity = value.toDouble();
        emit trackOpacityChanged();
    }
}

} // namespace FlySight
