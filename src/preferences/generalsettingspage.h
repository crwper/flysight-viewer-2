#ifndef GENERALSETTINGSPAGE_H
#define GENERALSETTINGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QGroupBox>

namespace FlySight {

class GeneralSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit GeneralSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveSettings();
    void browseLogbookFolder();

private:
    QComboBox *unitsComboBox;
    QLineEdit *logbookFolderLineEdit;
    QPushButton *browseButton;

    QGroupBox* createUnitsGroup();
    QGroupBox* createLogbookFolderGroup();
};

} // namespace FlySight

#endif // GENERALSETTINGSPAGE_H
