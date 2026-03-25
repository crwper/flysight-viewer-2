#ifndef WINGSUITPERFORMANCEWIDGET_H
#define WINGSUITPERFORMANCEWIDGET_H

#include "AnalysisMethodWidget.h"
#include <QMetaObject>

class QDoubleSpinBox;
class QRadioButton;
class QLabel;
class QPushButton;

namespace FlySight {

struct DependencyKey;

/**
 * Method-specific widget for Wingsuit Performance analysis.
 *
 * Provides altitude parameter editing (top/bottom of window), task selection
 * via radio buttons, a results grid showing Time/Distance/Speed, and a
 * Restore FAI Defaults button.
 *
 * All parameter edits write immediately to the session via
 * SessionModel::updateAttribute() with no local buffering.
 */
class WingsuitPerformanceWidget : public AnalysisMethodWidget
{
    Q_OBJECT
public:
    explicit WingsuitPerformanceWidget(QWidget* parent = nullptr);

    void setFocusedSession(SessionModel* model, const QString& sessionId) override;
    QStringList parameterKeys() const override;

private slots:
    void onTopAltChanged();
    void onBottomAltChanged();
    void onTaskChanged();
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

    SessionModel* m_model = nullptr;
    QString m_sessionId;

    // Parameter editors
    QDoubleSpinBox* m_topAltSpin    = nullptr;
    QDoubleSpinBox* m_bottomAltSpin = nullptr;

    // Task radio buttons
    QRadioButton* m_timeRadio     = nullptr;
    QRadioButton* m_distanceRadio = nullptr;
    QRadioButton* m_speedRadio    = nullptr;

    // Result labels
    QLabel* m_timeResult     = nullptr;
    QLabel* m_distanceResult = nullptr;
    QLabel* m_speedResult    = nullptr;
    QLabel* m_sepResult      = nullptr;

    // Restore FAI Defaults button
    QPushButton* m_restoreButton = nullptr;

    // Connection for dependency tracking
    QMetaObject::Connection m_dependencyConnection;
};

} // namespace FlySight

#endif // WINGSUITPERFORMANCEWIDGET_H
