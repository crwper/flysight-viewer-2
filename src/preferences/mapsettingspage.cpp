#include "mapsettingspage.h"
#include "preferencesmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>

namespace FlySight {

// Preference key constants
static const char* kLineThicknessKey = "map/lineThickness";
static const char* kTrackOpacityKey = "map/trackOpacity";
static const char* kMarkerSizeKey = "map/markerSize";

// Default values
static constexpr double kDefaultLineThickness = 3.0;
static constexpr double kDefaultTrackOpacity = 0.85;
static constexpr int kDefaultMarkerSize = 10;

MapSettingsPage::MapSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    // Register preferences with defaults
    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.registerPreference(kLineThicknessKey, kDefaultLineThickness);
    prefs.registerPreference(kTrackOpacityKey, kDefaultTrackOpacity);
    prefs.registerPreference(kMarkerSizeKey, kDefaultMarkerSize);

    // Build UI
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    mainLayout->addWidget(createTrackAppearanceGroup());
    mainLayout->addWidget(createCursorMarkerGroup());
    mainLayout->addWidget(createResetSection());
    mainLayout->addStretch();

    // Load current values
    loadSettings();

    // Connect signals for immediate save on change
    connect(m_lineThicknessSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MapSettingsPage::saveSettings);
    connect(m_trackOpacitySlider, &QSlider::valueChanged,
            this, &MapSettingsPage::onOpacitySliderChanged);
    connect(m_markerSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MapSettingsPage::saveSettings);
    connect(m_resetButton, &QPushButton::clicked,
            this, &MapSettingsPage::resetToDefaults);
}

QGroupBox* MapSettingsPage::createTrackAppearanceGroup()
{
    QGroupBox *group = new QGroupBox(tr("Track Appearance"), this);
    QFormLayout *layout = new QFormLayout(group);

    // Line thickness spin box
    m_lineThicknessSpinBox = new QDoubleSpinBox(this);
    m_lineThicknessSpinBox->setRange(1.0, 10.0);
    m_lineThicknessSpinBox->setSingleStep(0.5);
    m_lineThicknessSpinBox->setDecimals(1);
    m_lineThicknessSpinBox->setSuffix(tr(" px"));
    m_lineThicknessSpinBox->setToolTip(tr("Thickness of track lines on the map (1-10 pixels)"));
    layout->addRow(tr("Line thickness:"), m_lineThicknessSpinBox);

    // Track opacity slider with label
    QWidget *opacityWidget = new QWidget(this);
    QHBoxLayout *opacityLayout = new QHBoxLayout(opacityWidget);
    opacityLayout->setContentsMargins(0, 0, 0, 0);

    m_trackOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_trackOpacitySlider->setRange(0, 100);
    m_trackOpacitySlider->setTickPosition(QSlider::TicksBelow);
    m_trackOpacitySlider->setTickInterval(10);
    m_trackOpacitySlider->setToolTip(tr("Track transparency (0% = invisible, 100% = fully opaque)"));

    m_trackOpacityLabel = new QLabel(this);
    m_trackOpacityLabel->setMinimumWidth(40);
    m_trackOpacityLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    opacityLayout->addWidget(m_trackOpacitySlider, 1);
    opacityLayout->addWidget(m_trackOpacityLabel);

    layout->addRow(tr("Track opacity:"), opacityWidget);

    return group;
}

QGroupBox* MapSettingsPage::createCursorMarkerGroup()
{
    QGroupBox *group = new QGroupBox(tr("Cursor Marker"), this);
    QFormLayout *layout = new QFormLayout(group);

    // Marker size spin box
    m_markerSizeSpinBox = new QSpinBox(this);
    m_markerSizeSpinBox->setRange(4, 30);
    m_markerSizeSpinBox->setSuffix(tr(" px"));
    m_markerSizeSpinBox->setToolTip(tr("Size of the cursor position marker on the map (4-30 pixels)"));
    layout->addRow(tr("Marker size:"), m_markerSizeSpinBox);

    return group;
}

QWidget* MapSettingsPage::createResetSection()
{
    QWidget *widget = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 10, 0, 0);

    m_resetButton = new QPushButton(tr("Reset to Defaults"), this);
    layout->addStretch();
    layout->addWidget(m_resetButton);

    return widget;
}

void MapSettingsPage::loadSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    // Block signals during load to prevent unnecessary saves
    m_lineThicknessSpinBox->blockSignals(true);
    m_trackOpacitySlider->blockSignals(true);
    m_markerSizeSpinBox->blockSignals(true);

    double lineThickness = prefs.getValue(kLineThicknessKey).toDouble();
    m_lineThicknessSpinBox->setValue(lineThickness);

    double opacity = prefs.getValue(kTrackOpacityKey).toDouble();
    int opacityPercent = qRound(opacity * 100.0);
    m_trackOpacitySlider->setValue(opacityPercent);
    m_trackOpacityLabel->setText(QString("%1%").arg(opacityPercent));

    int markerSize = prefs.getValue(kMarkerSizeKey).toInt();
    m_markerSizeSpinBox->setValue(markerSize);

    // Re-enable signals
    m_lineThicknessSpinBox->blockSignals(false);
    m_trackOpacitySlider->blockSignals(false);
    m_markerSizeSpinBox->blockSignals(false);
}

void MapSettingsPage::saveSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    prefs.setValue(kLineThicknessKey, m_lineThicknessSpinBox->value());
    prefs.setValue(kTrackOpacityKey, m_trackOpacitySlider->value() / 100.0);
    prefs.setValue(kMarkerSizeKey, m_markerSizeSpinBox->value());
}

void MapSettingsPage::onOpacitySliderChanged(int value)
{
    // Update the label in real-time
    m_trackOpacityLabel->setText(QString("%1%").arg(value));

    // Save settings
    saveSettings();
}

void MapSettingsPage::resetToDefaults()
{
    // Confirm before resetting
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("Reset to Defaults"),
        tr("Are you sure you want to reset all map settings to their default values?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    PreferencesManager &prefs = PreferencesManager::instance();

    // Set defaults in preferences
    prefs.setValue(kLineThicknessKey, kDefaultLineThickness);
    prefs.setValue(kTrackOpacityKey, kDefaultTrackOpacity);
    prefs.setValue(kMarkerSizeKey, kDefaultMarkerSize);

    // Reload UI from preferences
    loadSettings();
}

} // namespace FlySight
