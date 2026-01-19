#ifndef MAPSETTINGSPAGE_H
#define MAPSETTINGSPAGE_H

#include <QWidget>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

namespace FlySight {

class MapSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit MapSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveSettings();
    void onOpacitySliderChanged(int value);
    void resetToDefaults();

private:
    // Track Appearance controls
    QDoubleSpinBox *m_lineThicknessSpinBox;
    QSlider *m_trackOpacitySlider;
    QLabel *m_trackOpacityLabel;

    // Cursor Marker controls
    QSpinBox *m_markerSizeSpinBox;

    // Reset button
    QPushButton *m_resetButton;

    QGroupBox* createTrackAppearanceGroup();
    QGroupBox* createCursorMarkerGroup();
    QWidget* createResetSection();

    void loadSettings();
};

} // namespace FlySight

#endif // MAPSETTINGSPAGE_H
