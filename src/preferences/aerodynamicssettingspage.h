#ifndef AERODYNAMICSSETTINGSPAGE_H
#define AERODYNAMICSSETTINGSPAGE_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QGroupBox>

namespace FlySight {

class AerodynamicsSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit AerodynamicsSettingsPage(QWidget *parent = nullptr);

public slots:
    void saveSettings();

private:
    QDoubleSpinBox *m_massSpinBox;
    QDoubleSpinBox *m_areaSpinBox;

    QGroupBox* createBodyGroup();
};

} // namespace FlySight

#endif // AERODYNAMICSSETTINGSPAGE_H
