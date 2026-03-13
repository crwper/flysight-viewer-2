#ifndef MANAGEPROFILESDIALOG_H
#define MANAGEPROFILESDIALOG_H

#include <QDialog>

class QListWidget;
class QPushButton;

namespace FlySight {

class ManageProfilesDialog : public QDialog {
    Q_OBJECT

public:
    explicit ManageProfilesDialog(QWidget *parent = nullptr);

private slots:
    void onRename();
    void onMoveUp();
    void onMoveDown();
    void onDelete();
    void onImport();
    void onExport();
    void onRestoreDefaults();
    void updateButtonStates();

private:
    QListWidget *m_profileList;
    QPushButton *m_renameButton;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QPushButton *m_deleteButton;
    QPushButton *m_importButton;
    QPushButton *m_exportButton;
    QPushButton *m_restoreDefaultsButton;

    void populateList();
    void saveOrder();
};

} // namespace FlySight

#endif // MANAGEPROFILESDIALOG_H
