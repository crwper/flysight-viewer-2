#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "zoomsettingspage.h"
#include "preferencekeys.h"
#include "preferencesmanager.h"
#include "markerregistry.h"

namespace FlySight {

ZoomSettingsPage::ZoomSettingsPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createExtentModeGroup());
    layout->addWidget(createMarginGroup());
    layout->addStretch();

    // Wire up saves
    connect(m_allDataRadio, &QRadioButton::toggled, this, &ZoomSettingsPage::saveSettings);
    connect(m_markerRangeRadio, &QRadioButton::toggled, this, &ZoomSettingsPage::saveSettings);
    connect(m_startMarkerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ZoomSettingsPage::saveSettings);
    connect(m_endMarkerCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ZoomSettingsPage::saveSettings);
    connect(m_marginSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ZoomSettingsPage::saveSettings);

    // Enable/disable marker combos based on mode
    connect(m_allDataRadio, &QRadioButton::toggled, this, &ZoomSettingsPage::updateMarkerCombosEnabled);
}

QGroupBox* ZoomSettingsPage::createExtentModeGroup()
{
    QGroupBox *group = new QGroupBox(tr("Zoom to extent mode"), this);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);

    m_allDataRadio = new QRadioButton(tr("All data"), this);
    m_markerRangeRadio = new QRadioButton(tr("Marker range"), this);

    groupLayout->addWidget(m_allDataRadio);
    groupLayout->addWidget(m_markerRangeRadio);

    // Marker combo boxes
    QHBoxLayout *startLayout = new QHBoxLayout();
    startLayout->addSpacing(20);
    startLayout->addWidget(new QLabel(tr("Start marker:"), this));
    m_startMarkerCombo = new QComboBox(this);
    startLayout->addWidget(m_startMarkerCombo, 1);
    groupLayout->addLayout(startLayout);

    QHBoxLayout *endLayout = new QHBoxLayout();
    endLayout->addSpacing(20);
    endLayout->addWidget(new QLabel(tr("End marker:"), this));
    m_endMarkerCombo = new QComboBox(this);
    endLayout->addWidget(m_endMarkerCombo, 1);
    groupLayout->addLayout(endLayout);

    // Populate combos from MarkerRegistry
    populateMarkerCombos();

    // Initialize from preferences
    PreferencesManager &prefs = PreferencesManager::instance();
    QString mode = prefs.getValue(PreferenceKeys::ZoomExtentMode).toString();
    if (mode == "allData") {
        m_allDataRadio->setChecked(true);
    } else {
        m_markerRangeRadio->setChecked(true);
    }

    // Select saved markers
    QString startKey = prefs.getValue(PreferenceKeys::ZoomExtentStartMarker).toString();
    QString endKey = prefs.getValue(PreferenceKeys::ZoomExtentEndMarker).toString();

    int startIdx = m_startMarkerCombo->findData(startKey);
    if (startIdx >= 0) m_startMarkerCombo->setCurrentIndex(startIdx);

    int endIdx = m_endMarkerCombo->findData(endKey);
    if (endIdx >= 0) m_endMarkerCombo->setCurrentIndex(endIdx);

    updateMarkerCombosEnabled();

    return group;
}

QGroupBox* ZoomSettingsPage::createMarginGroup()
{
    QGroupBox *group = new QGroupBox(tr("Margin"), this);
    QHBoxLayout *groupLayout = new QHBoxLayout(group);

    QLabel *label = new QLabel(tr("Margin"), this);
    m_marginSpinBox = new QDoubleSpinBox(this);
    m_marginSpinBox->setRange(0.0, 25.0);
    m_marginSpinBox->setSingleStep(1.0);
    m_marginSpinBox->setDecimals(1);
    m_marginSpinBox->setSuffix(tr("%"));

    groupLayout->addWidget(label);
    groupLayout->addWidget(m_marginSpinBox);

    // Initialize from preferences
    PreferencesManager &prefs = PreferencesManager::instance();
    m_marginSpinBox->setValue(prefs.getValue(PreferenceKeys::ZoomExtentMarginPct).toDouble());

    return group;
}

void ZoomSettingsPage::populateMarkerCombos()
{
    const QVector<MarkerDefinition> markers = MarkerRegistry::instance()->allMarkers();
    for (const MarkerDefinition &def : markers) {
        QString label = def.displayName;
        if (!def.category.isEmpty()) {
            label = QStringLiteral("%1 / %2").arg(def.category, def.displayName);
        }
        m_startMarkerCombo->addItem(label, def.attributeKey);
        m_endMarkerCombo->addItem(label, def.attributeKey);
    }
}

void ZoomSettingsPage::updateMarkerCombosEnabled()
{
    bool enabled = m_markerRangeRadio->isChecked();
    m_startMarkerCombo->setEnabled(enabled);
    m_endMarkerCombo->setEnabled(enabled);
}

void ZoomSettingsPage::saveSettings()
{
    PreferencesManager &prefs = PreferencesManager::instance();

    prefs.setValue(PreferenceKeys::ZoomExtentMode,
                   m_allDataRadio->isChecked() ? QStringLiteral("allData") : QStringLiteral("markerRange"));
    prefs.setValue(PreferenceKeys::ZoomExtentStartMarker,
                   m_startMarkerCombo->currentData().toString());
    prefs.setValue(PreferenceKeys::ZoomExtentEndMarker,
                   m_endMarkerCombo->currentData().toString());
    prefs.setValue(PreferenceKeys::ZoomExtentMarginPct, m_marginSpinBox->value());
}

} // namespace FlySight
