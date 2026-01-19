#ifndef LEGENDSETTINGSPAGE_H
#define LEGENDSETTINGSPAGE_H

#include <QWidget>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace FlySight {

class LegendSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit LegendSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveSettings();
    void resetToDefaults();

private:
    QSpinBox *m_textSizeSpinBox;
    QPushButton *m_resetButton;

    QGroupBox* createTextSettingsGroup();
    QWidget* createResetSection();

    void loadSettings();
};

} // namespace FlySight

#endif // LEGENDSETTINGSPAGE_H
