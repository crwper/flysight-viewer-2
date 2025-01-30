#include <QFileDialog>
#include <QLabel>
#include "generalsettingspage.h"
#include "preferencesmanager.h"

namespace FlySight {

GeneralSettingsPage::GeneralSettingsPage(QWidget *parent)
    : QWidget(parent) {
    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(createUnitsGroup());
    layout->addWidget(createLogbookFolderGroup());
    layout->addStretch();

    connect(unitsComboBox, &QComboBox::currentTextChanged, this, &GeneralSettingsPage::saveSettings);
    connect(browseButton, &QPushButton::clicked, this, &GeneralSettingsPage::browseLogbookFolder);
}

QGroupBox* GeneralSettingsPage::createUnitsGroup() {
    QGroupBox *unitsGroup = new QGroupBox("Measurement units", this);
    QHBoxLayout *unitsLayout = new QHBoxLayout(unitsGroup);

    unitsComboBox = new QComboBox(this);
    unitsComboBox->addItems({"Metric", "Imperial"});

    // Initialize from settings
    QString units = PreferencesManager::instance().getValue("general/units").toString();
    unitsComboBox->setCurrentText(units);

    unitsLayout->addWidget(unitsComboBox);
    unitsLayout->addStretch();

    return unitsGroup;
}

QGroupBox* GeneralSettingsPage::createLogbookFolderGroup() {
    QGroupBox *logbookGroup = new QGroupBox("Logbook folder", this);
    QHBoxLayout *logbookLayout = new QHBoxLayout(logbookGroup);

    logbookFolderLineEdit = new QLineEdit(this);
    logbookFolderLineEdit->setReadOnly(true);

    browseButton = new QPushButton(this);
    browseButton->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));

    // Initialize from settings
    QString folder = PreferencesManager::instance().getValue("general/logbookFolder").toString();
    logbookFolderLineEdit->setText(folder);

    logbookLayout->addWidget(logbookFolderLineEdit);
    logbookLayout->addWidget(browseButton);

    return logbookGroup;
}

void GeneralSettingsPage::saveSettings() {
    PreferencesManager &prefs = PreferencesManager::instance();
    prefs.setValue("general/units", unitsComboBox->currentText());
    prefs.setValue("general/logbookFolder", logbookFolderLineEdit->text());
}

void GeneralSettingsPage::browseLogbookFolder() {
    QString folder = QFileDialog::getExistingDirectory(this, "Select Logbook Folder");
    if (!folder.isEmpty()) {
        logbookFolderLineEdit->setText(folder);
        saveSettings();
    }
}

} // namespace FlySight
