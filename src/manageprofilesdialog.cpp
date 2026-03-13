#include "manageprofilesdialog.h"
#include "profile.h"
#include "profilemanager.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QVBoxLayout>

#include <algorithm>

namespace FlySight {

ManageProfilesDialog::ManageProfilesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Manage Profiles"));
    resize(450, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Profile list
    m_profileList = new QListWidget(this);
    m_profileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_profileList->setDragDropMode(QAbstractItemView::NoDragDrop);
    mainLayout->addWidget(m_profileList);

    // First button row: Rename, Move Up, Move Down, Delete
    QWidget *buttonRow1 = new QWidget(this);
    QHBoxLayout *buttonLayout1 = new QHBoxLayout(buttonRow1);
    buttonLayout1->setContentsMargins(0, 0, 0, 0);

    m_renameButton   = new QPushButton(tr("Rename..."), this);
    m_moveUpButton   = new QPushButton(tr("Move Up"), this);
    m_moveDownButton = new QPushButton(tr("Move Down"), this);
    m_deleteButton   = new QPushButton(tr("Delete"), this);

    buttonLayout1->addWidget(m_renameButton);
    buttonLayout1->addWidget(m_moveUpButton);
    buttonLayout1->addWidget(m_moveDownButton);
    buttonLayout1->addWidget(m_deleteButton);
    buttonLayout1->addStretch();

    mainLayout->addWidget(buttonRow1);

    // Second button row: Import, Export, stretch, Restore Defaults
    QWidget *buttonRow2 = new QWidget(this);
    QHBoxLayout *buttonLayout2 = new QHBoxLayout(buttonRow2);
    buttonLayout2->setContentsMargins(0, 0, 0, 0);

    m_importButton          = new QPushButton(tr("Import..."), this);
    m_exportButton          = new QPushButton(tr("Export..."), this);
    m_restoreDefaultsButton = new QPushButton(tr("Restore Defaults..."), this);

    buttonLayout2->addWidget(m_importButton);
    buttonLayout2->addWidget(m_exportButton);
    buttonLayout2->addStretch();
    buttonLayout2->addWidget(m_restoreDefaultsButton);

    mainLayout->addWidget(buttonRow2);

    // Close button
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    // Connect signals
    connect(m_renameButton,   &QPushButton::clicked, this, &ManageProfilesDialog::onRename);
    connect(m_moveUpButton,   &QPushButton::clicked, this, &ManageProfilesDialog::onMoveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &ManageProfilesDialog::onMoveDown);
    connect(m_deleteButton,   &QPushButton::clicked, this, &ManageProfilesDialog::onDelete);
    connect(m_importButton,   &QPushButton::clicked, this, &ManageProfilesDialog::onImport);
    connect(m_exportButton,   &QPushButton::clicked, this, &ManageProfilesDialog::onExport);
    connect(m_restoreDefaultsButton, &QPushButton::clicked, this, &ManageProfilesDialog::onRestoreDefaults);

    connect(m_profileList, &QListWidget::currentRowChanged,
            this, &ManageProfilesDialog::updateButtonStates);

    populateList();
}

void ManageProfilesDialog::populateList()
{
    m_profileList->blockSignals(true);
    m_profileList->clear();

    // Get all profiles and the user-defined order
    const QVector<Profile> allProfiles = ProfileManager::instance().listProfiles();
    const QStringList orderedIds = ProfileManager::instance().profileOrder();

    // Build a map from id to profile for quick lookup
    QHash<QString, Profile> profileMap;
    for (const Profile &p : allProfiles)
        profileMap.insert(p.id, p);

    // Collect profiles in order: first those in orderedIds, then remaining alphabetically
    QVector<Profile> ordered;
    QSet<QString> added;

    for (const QString &id : orderedIds) {
        if (profileMap.contains(id)) {
            ordered.append(profileMap.value(id));
            added.insert(id);
        }
    }

    // Remaining profiles sorted alphabetically by displayName
    QVector<Profile> remaining;
    for (const Profile &p : allProfiles) {
        if (!added.contains(p.id))
            remaining.append(p);
    }
    std::sort(remaining.begin(), remaining.end(),
              [](const Profile &a, const Profile &b) {
                  return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
              });

    ordered.append(remaining);

    // Populate list widget
    for (const Profile &p : ordered) {
        QListWidgetItem *item = new QListWidgetItem(p.displayName);
        item->setData(Qt::UserRole, p.id);
        m_profileList->addItem(item);
    }

    m_profileList->blockSignals(false);
    updateButtonStates();
}

void ManageProfilesDialog::updateButtonStates()
{
    int row = m_profileList->currentRow();
    int count = m_profileList->count();
    bool hasSelection = (row >= 0);

    m_renameButton->setEnabled(hasSelection);
    m_moveUpButton->setEnabled(row > 0);
    m_moveDownButton->setEnabled(hasSelection && row < count - 1);
    m_deleteButton->setEnabled(hasSelection);
    m_exportButton->setEnabled(hasSelection);
}

void ManageProfilesDialog::saveOrder()
{
    QStringList ids;
    for (int i = 0; i < m_profileList->count(); ++i)
        ids.append(m_profileList->item(i)->data(Qt::UserRole).toString());

    ProfileManager::instance().setProfileOrder(ids);
}

void ManageProfilesDialog::onRename()
{
    QListWidgetItem *item = m_profileList->currentItem();
    if (!item)
        return;

    const QString oldId = item->data(Qt::UserRole).toString();
    auto loaded = ProfileManager::instance().loadProfile(oldId);
    if (!loaded)
        return;

    Profile profile = *loaded;

    bool ok = false;
    QString newName = QInputDialog::getText(this, tr("Rename Profile"),
                                            tr("Profile name:"),
                                            QLineEdit::Normal,
                                            profile.displayName, &ok);
    if (!ok || newName.trimmed().isEmpty())
        return;

    newName = newName.trimmed();

    // Update display name and save as new profile (new ID since filename changes)
    profile.displayName = newName;
    profile.id.clear();

    if (!ProfileManager::instance().saveProfile(profile))
        return;

    // Delete old profile
    ProfileManager::instance().deleteProfile(oldId);

    // Update order list: replace old ID with new ID
    QStringList order = ProfileManager::instance().profileOrder();
    int orderIdx = order.indexOf(oldId);
    if (orderIdx >= 0)
        order[orderIdx] = profile.id;
    else
        order.append(profile.id);
    ProfileManager::instance().setProfileOrder(order);

    populateList();
}

void ManageProfilesDialog::onMoveUp()
{
    int row = m_profileList->currentRow();
    if (row <= 0)
        return;

    m_profileList->blockSignals(true);
    QListWidgetItem *item = m_profileList->takeItem(row);
    m_profileList->insertItem(row - 1, item);
    m_profileList->blockSignals(false);

    m_profileList->setCurrentRow(row - 1);
    saveOrder();
    updateButtonStates();
}

void ManageProfilesDialog::onMoveDown()
{
    int row = m_profileList->currentRow();
    if (row < 0 || row >= m_profileList->count() - 1)
        return;

    m_profileList->blockSignals(true);
    QListWidgetItem *item = m_profileList->takeItem(row);
    m_profileList->insertItem(row + 1, item);
    m_profileList->blockSignals(false);

    m_profileList->setCurrentRow(row + 1);
    saveOrder();
    updateButtonStates();
}

void ManageProfilesDialog::onDelete()
{
    QListWidgetItem *item = m_profileList->currentItem();
    if (!item)
        return;

    const QString id = item->data(Qt::UserRole).toString();
    const QString name = item->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Delete Profile"),
        tr("Are you sure you want to delete the profile \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    ProfileManager::instance().deleteProfile(id);
    populateList();
}

void ManageProfilesDialog::onImport()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Import Profiles"),
        QString(),
        tr("FlySight Profiles (*.fvprofile)"));

    if (files.isEmpty())
        return;

    for (const QString &filePath : files) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        const QByteArray data = file.readAll();
        file.close();

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        Profile profile = profileFromJson(doc.object(), QString());
        ProfileManager::instance().saveProfile(profile);
    }

    populateList();
}

void ManageProfilesDialog::onExport()
{
    QListWidgetItem *item = m_profileList->currentItem();
    if (!item)
        return;

    const QString id = item->data(Qt::UserRole).toString();
    auto loaded = ProfileManager::instance().loadProfile(id);
    if (!loaded)
        return;

    const Profile &profile = *loaded;

    QString suggestedName = profile.displayName;
    if (!suggestedName.endsWith(QStringLiteral(".fvprofile")))
        suggestedName += QStringLiteral(".fvprofile");

    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export Profile"),
        suggestedName,
        tr("FlySight Profiles (*.fvprofile)"));

    if (filePath.isEmpty())
        return;

    const QJsonObject obj = profileToJson(profile);
    const QJsonDocument doc(obj);

    QSaveFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly))
        return;

    saveFile.write(doc.toJson(QJsonDocument::Indented));
    saveFile.commit();
}

void ManageProfilesDialog::onRestoreDefaults()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Restore Default Profiles"),
        tr("This will restore the built-in default profiles, overwriting any "
           "existing profiles with the same filenames. Continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes)
        return;

    ProfileManager::instance().copyDefaultProfiles(true);
    populateList();
}

} // namespace FlySight
