#ifndef SPEEDSKYDIVINGWIDGET_H
#define SPEEDSKYDIVINGWIDGET_H

#include "AnalysisMethodWidget.h"
#include <QMetaObject>

class QDoubleSpinBox;
class QLabel;
class QPushButton;

namespace FlySight {

struct DependencyKey;

/**
 * Method-specific widget for Speed Skydiving analysis.
 *
 * Provides parameter editing (performance window height, validation window
 * height, breakoff altitude) displayed in feet, a results section showing
 * the scored speed in km/h, and speed accuracy in m/s.
 *
 * All parameter edits write immediately to the session via
 * SessionModel::updateAttribute() with no local buffering.
 */
class SpeedSkydivingWidget : public AnalysisMethodWidget
{
    Q_OBJECT
public:
    explicit SpeedSkydivingWidget(QWidget* parent = nullptr);

    void setFocusedSession(SessionModel* model, const QString& sessionId) override;
    QStringList parameterKeys() const override;

private slots:
    void onPerfWindowHeightChanged();
    void onValWindowHeightChanged();
    void onBreakoffAltChanged();
    void onRestoreFaiDefaults();
    void onDependencyChanged(const QString& sessionId, const DependencyKey& key);

private:
    void buildLayout();
    void refreshAll();
    void refreshParameters();
    void refreshResults();
    void refreshRestoreButton();
    void writeAttribute(const QString& key, const QVariant& value);
    void blockEditorSignals(bool block);
    bool isAtFaiDefaults() const;

    static constexpr double kFtPerM = 3.28084;

    SessionModel* m_model = nullptr;
    QString m_sessionId;

    // Parameter editors (display in feet, store in metres)
    QDoubleSpinBox* m_perfWindowHeightSpin = nullptr;
    QDoubleSpinBox* m_valWindowHeightSpin  = nullptr;
    QDoubleSpinBox* m_breakoffAltSpin      = nullptr;

    // Result labels
    QLabel* m_speedResult    = nullptr;
    QLabel* m_speedAccResult = nullptr;

    // Restore FAI Defaults button
    QPushButton* m_restoreButton = nullptr;

    // Connection for dependency tracking
    QMetaObject::Connection m_dependencyConnection;
};

} // namespace FlySight

#endif // SPEEDSKYDIVINGWIDGET_H
