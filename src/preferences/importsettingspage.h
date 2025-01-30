#ifndef IMPORTSETTINGSPAGE_H
#define IMPORTSETTINGSPAGE_H

#include <QWidget>
#include <QRadioButton>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QSettings>

class ImportSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit ImportSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    QRadioButton *automaticRadioButton;
    QRadioButton *fixedRadioButton;
    QLineEdit *fixedElevationLineEdit;

    QGroupBox* createGroundReferenceGroup();
};

#endif // IMPORTSETTINGSPAGE_H
