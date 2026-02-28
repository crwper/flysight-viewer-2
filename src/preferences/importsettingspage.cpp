#include <QLabel>
#include <QSpacerItem>
#include "importsettingspage.h"
#include "preferencekeys.h"
#include "preferencesmanager.h"

namespace FlySight {

ImportSettingsPage::ImportSettingsPage(QWidget *parent)
    : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createGroundReferenceGroup());
    layout->addWidget(createDescentPauseGroup());
    layout->addStretch();

    connect(automaticRadioButton, &QRadioButton::toggled, this, &ImportSettingsPage::saveSettings);
    connect(fixedRadioButton, &QRadioButton::toggled, this, &ImportSettingsPage::saveSettings);
    connect(fixedElevationLineEdit, &QLineEdit::textChanged, this, &ImportSettingsPage::saveSettings);
    connect(descentPauseSpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &ImportSettingsPage::saveSettings);
}

QGroupBox* ImportSettingsPage::createGroundReferenceGroup() {
    QGroupBox *groundReferenceGroup = new QGroupBox(tr("Ground reference"), this);
    QVBoxLayout *groupLayout = new QVBoxLayout(groundReferenceGroup);

    automaticRadioButton = new QRadioButton(tr("Automatic"), this);
    fixedRadioButton = new QRadioButton(tr("Fixed"), this);

    QHBoxLayout *fixedLayout = new QHBoxLayout();
    fixedElevationLineEdit = new QLineEdit(this);
    QLabel *metersLabel = new QLabel(tr("m"), this);

    fixedLayout->addWidget(fixedRadioButton);
    fixedLayout->addWidget(fixedElevationLineEdit);
    fixedLayout->addWidget(metersLabel);

    groupLayout->addWidget(automaticRadioButton);
    groupLayout->addLayout(fixedLayout);

    // Initialize settings
    PreferencesManager &prefs = PreferencesManager::instance();
    QString groundRefMode = prefs.getValue(PreferenceKeys::ImportGroundReferenceMode).toString();
    double elevation = prefs.getValue(PreferenceKeys::ImportFixedElevation).toDouble();

    if (groundRefMode == "Automatic") {
        automaticRadioButton->setChecked(true);
    } else {
        fixedRadioButton->setChecked(true);
    }
    fixedElevationLineEdit->setText(QString::number(elevation));

    return groundReferenceGroup;
}

QGroupBox* ImportSettingsPage::createDescentPauseGroup() {
    QGroupBox *descentPauseGroup = new QGroupBox(tr("Descent detection"), this);
    QHBoxLayout *groupLayout = new QHBoxLayout(descentPauseGroup);

    QLabel *label = new QLabel(tr("Descent pause timeout"), this);
    descentPauseSpinBox = new QDoubleSpinBox(this);
    descentPauseSpinBox->setRange(1.0, 300.0);
    descentPauseSpinBox->setSingleStep(1.0);
    descentPauseSpinBox->setDecimals(1);
    QLabel *unitLabel = new QLabel(tr("s"), this);

    groupLayout->addWidget(label);
    groupLayout->addWidget(descentPauseSpinBox);
    groupLayout->addWidget(unitLabel);

    // Initialize from preferences
    PreferencesManager &prefs = PreferencesManager::instance();
    descentPauseSpinBox->setValue(prefs.getValue(PreferenceKeys::ImportDescentPauseSeconds).toDouble());

    return descentPauseGroup;
}

void ImportSettingsPage::saveSettings() {
    PreferencesManager &prefs = PreferencesManager::instance();

    if (automaticRadioButton->isChecked()) {
        prefs.setValue(PreferenceKeys::ImportGroundReferenceMode, "Automatic");
    } else {
        prefs.setValue(PreferenceKeys::ImportGroundReferenceMode, "Fixed");
    }
    prefs.setValue(PreferenceKeys::ImportFixedElevation, fixedElevationLineEdit->text().toDouble());
    prefs.setValue(PreferenceKeys::ImportDescentPauseSeconds, descentPauseSpinBox->value());
}

} // namespace FlySight
