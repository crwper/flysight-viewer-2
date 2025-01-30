#include <QLabel>
#include <QSpacerItem>
#include "importsettingspage.h"
#include "preferencesmanager.h"

namespace FlySight {

ImportSettingsPage::ImportSettingsPage(QWidget *parent)
    : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createGroundReferenceGroup());
    layout->addStretch();

    connect(automaticRadioButton, &QRadioButton::toggled, this, &ImportSettingsPage::saveSettings);
    connect(fixedRadioButton, &QRadioButton::toggled, this, &ImportSettingsPage::saveSettings);
    connect(fixedElevationLineEdit, &QLineEdit::textChanged, this, &ImportSettingsPage::saveSettings);
}

QGroupBox* ImportSettingsPage::createGroundReferenceGroup() {
    QGroupBox *groundReferenceGroup = new QGroupBox("Ground reference", this);
    QVBoxLayout *groupLayout = new QVBoxLayout(groundReferenceGroup);

    automaticRadioButton = new QRadioButton("Automatic", this);
    fixedRadioButton = new QRadioButton("Fixed", this);

    QHBoxLayout *fixedLayout = new QHBoxLayout();
    fixedElevationLineEdit = new QLineEdit(this);
    QLabel *metersLabel = new QLabel("m", this);

    fixedLayout->addWidget(fixedRadioButton);
    fixedLayout->addWidget(fixedElevationLineEdit);
    fixedLayout->addWidget(metersLabel);

    groupLayout->addWidget(automaticRadioButton);
    groupLayout->addLayout(fixedLayout);

    // Initialize settings
    PreferencesManager &prefs = PreferencesManager::instance();
    QString groundRefMode = prefs.getValue("import/groundReferenceMode").toString();
    double elevation = prefs.getValue("import/fixedElevation").toDouble();

    if (groundRefMode == "automatic") {
        automaticRadioButton->setChecked(true);
    } else {
        fixedRadioButton->setChecked(true);
    }
    fixedElevationLineEdit->setText(QString::number(elevation));

    return groundReferenceGroup;
}

void ImportSettingsPage::saveSettings() {
    PreferencesManager &prefs = PreferencesManager::instance();

    if (automaticRadioButton->isChecked()) {
        prefs.setValue("import/groundReferenceMode", "automatic");
    } else {
        prefs.setValue("import/groundReferenceMode", "fixed");
    }
    prefs.setValue("import/fixedElevation", fixedElevationLineEdit->text().toDouble());
}

} // namespace FlySight
