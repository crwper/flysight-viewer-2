#ifndef MAPPREFERENCESBRIDGE_H
#define MAPPREFERENCESBRIDGE_H

#include <QObject>
#include <QColor>

namespace FlySight {

/**
 * @brief Bridge class to expose map preferences to QML.
 *
 * This class listens to PreferencesManager changes and notifies QML
 * when map-related preferences change.
 */
class MapPreferencesBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double lineThickness READ lineThickness NOTIFY lineThicknessChanged)
    Q_PROPERTY(int markerSize READ markerSize NOTIFY markerSizeChanged)
    Q_PROPERTY(double trackOpacity READ trackOpacity NOTIFY trackOpacityChanged)
    Q_PROPERTY(int mapTypeIndex READ mapTypeIndex WRITE setMapTypeIndex NOTIFY mapTypeIndexChanged)

public:
    explicit MapPreferencesBridge(QObject *parent = nullptr);

    double lineThickness() const { return m_lineThickness; }
    int markerSize() const { return m_markerSize; }
    double trackOpacity() const { return m_trackOpacity; }
    int mapTypeIndex() const { return m_mapTypeIndex; }
    void setMapTypeIndex(int index);

signals:
    void lineThicknessChanged();
    void markerSizeChanged();
    void trackOpacityChanged();
    void mapTypeIndexChanged();

private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    void loadAllPreferences();

    double m_lineThickness = 3.0;
    int m_markerSize = 10;
    double m_trackOpacity = 0.85;
    int m_mapTypeIndex = 0;
};

} // namespace FlySight

#endif // MAPPREFERENCESBRIDGE_H
