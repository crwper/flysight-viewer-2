#ifndef MARKERSSETTINGSPAGE_H
#define MARKERSSETTINGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QMap>
#include <QColor>

namespace FlySight {

class MarkersSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit MarkersSettingsPage(QWidget *parent = nullptr);

private slots:
    void onColorButtonClicked();
    void resetAllToDefaults();

private:
    QTreeWidget *m_markerTree;
    QPushButton *m_resetButton;

    // Maps attributeKey -> color button for quick access
    QMap<QString, QPushButton*> m_colorButtons;

    // Maps attributeKey -> default color for reset functionality
    QMap<QString, QColor> m_defaultColors;

    QGroupBox* createMarkerColorsGroup();
    QWidget* createResetSection();

    void populateMarkerTree();
    void updateColorButtonStyle(QPushButton *button, const QColor &color);
    void saveMarkerColor(const QString &attributeKey, const QColor &color);
    QColor loadMarkerColor(const QString &attributeKey, const QColor &defaultColor);

    static QString markerColorKey(const QString &attributeKey);
};

} // namespace FlySight

#endif // MARKERSSETTINGSPAGE_H
