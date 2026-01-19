#ifndef PLOTSSETTINGSPAGE_H
#define PLOTSSETTINGSPAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QColor>
#include <QMap>

namespace FlySight {

class PlotsSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit PlotsSettingsPage(QWidget *parent = nullptr);

private slots:
    void saveGlobalSettings();
    void chooseCrosshairColor();
    void resetAllToDefaults();

private:
    // Global settings widgets
    QDoubleSpinBox *lineThicknessSpinBox;
    QSpinBox *textSizeSpinBox;
    QPushButton *crosshairColorButton;
    QDoubleSpinBox *crosshairThicknessSpinBox;
    QSpinBox *yAxisPaddingSpinBox;

    // Per-plot settings
    QTreeWidget *plotsTreeWidget;

    // Track color buttons for per-plot settings
    // Key: "sensorID/measurementID"
    QMap<QString, QPushButton*> plotColorButtons;
    QMap<QString, QComboBox*> plotYAxisModeComboBoxes;
    QMap<QString, QDoubleSpinBox*> plotYAxisMinSpinBoxes;
    QMap<QString, QDoubleSpinBox*> plotYAxisMaxSpinBoxes;

    // Current crosshair color (stored separately for button display)
    QColor currentCrosshairColor;

    // UI creation helpers
    QGroupBox* createGlobalSettingsGroup();
    QGroupBox* createPerPlotSettingsGroup();
    QWidget* createResetSection();

    // Helper methods
    void updateColorButtonStyle(QPushButton *button, const QColor &color);
    void loadGlobalSettings();
    void loadPerPlotSettings();
    QString getPlotKey(const QString &sensorID, const QString &measurementID) const;

    // Per-plot setting handlers
    void onPlotColorButtonClicked();
    void onYAxisModeChanged(const QString &plotKey, int index);
    void onYAxisMinChanged(const QString &plotKey, double value);
    void onYAxisMaxChanged(const QString &plotKey, double value);
    void updateYAxisWidgetsEnabled(const QString &plotKey, bool manual);
};

} // namespace FlySight

#endif // PLOTSSETTINGSPAGE_H
