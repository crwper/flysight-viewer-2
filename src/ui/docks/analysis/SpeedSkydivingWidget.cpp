#include "SpeedSkydivingWidget.h"
#include "StepCommitSpinBox.h"
#include "sessionmodel.h"
#include "sessiondata.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace FlySight {

// FAI default parameters in feet
static constexpr double kFaiPerfWindowHeightFt = 7400.0;
static constexpr double kFaiValWindowHeightFt  = 3300.0;
static constexpr double kFaiBreakoffAltFt      = 5600.0;

static constexpr double kFtPerM = 3.28084;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SpeedSkydivingWidget::SpeedSkydivingWidget(QWidget* parent)
    : AnalysisMethodWidget(parent)
{
    buildLayout();
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void SpeedSkydivingWidget::buildLayout()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Parameter section ---
    auto* paramForm = new QFormLayout;

    m_perfWindowHeightSpin = new StepCommitSpinBox;
    m_perfWindowHeightSpin->setRange(0.0, 99999.0);
    m_perfWindowHeightSpin->setDecimals(0);
    m_perfWindowHeightSpin->setSingleStep(100.0);
    paramForm->addRow(tr("Performance window height (ft):"), m_perfWindowHeightSpin);

    m_valWindowHeightSpin = new StepCommitSpinBox;
    m_valWindowHeightSpin->setRange(0.0, 99999.0);
    m_valWindowHeightSpin->setDecimals(0);
    m_valWindowHeightSpin->setSingleStep(100.0);
    paramForm->addRow(tr("Validation window height (ft):"), m_valWindowHeightSpin);

    m_breakoffAltSpin = new StepCommitSpinBox;
    m_breakoffAltSpin->setRange(0.0, 99999.0);
    m_breakoffAltSpin->setDecimals(0);
    m_breakoffAltSpin->setSingleStep(100.0);
    paramForm->addRow(tr("Breakoff altitude (ft):"), m_breakoffAltSpin);

    mainLayout->addLayout(paramForm);

    // --- Restore FAI Defaults button ---
    m_restoreButton = new QPushButton(tr("Restore FAI Defaults"));
    mainLayout->addWidget(m_restoreButton);

    // --- Separator ---
    auto* separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // --- Speed result ---
    m_speedResult = new QLabel(QStringLiteral("Speed: --"));
    mainLayout->addWidget(m_speedResult);

    // --- Speed accuracy ---
    m_speedAccResult = new QLabel(QStringLiteral("Speed Accuracy: --"));
    mainLayout->addWidget(m_speedAccResult);

    // --- Stretch ---
    mainLayout->addStretch();

    // --- Connections ---
    connect(m_perfWindowHeightSpin, &QDoubleSpinBox::editingFinished,
            this, &SpeedSkydivingWidget::onPerfWindowHeightChanged);
    connect(m_valWindowHeightSpin, &QDoubleSpinBox::editingFinished,
            this, &SpeedSkydivingWidget::onValWindowHeightChanged);
    connect(m_breakoffAltSpin, &QDoubleSpinBox::editingFinished,
            this, &SpeedSkydivingWidget::onBreakoffAltChanged);

    connect(m_restoreButton, &QPushButton::clicked,
            this, &SpeedSkydivingWidget::onRestoreFaiDefaults);
}

// ---------------------------------------------------------------------------
// Session binding
// ---------------------------------------------------------------------------

void SpeedSkydivingWidget::setFocusedSession(SessionModel* model, const QString& sessionId)
{
    if (m_dependencyConnection)
        QObject::disconnect(m_dependencyConnection);

    m_model = model;
    m_sessionId = sessionId;

    if (m_model && !m_sessionId.isEmpty()) {
        m_dependencyConnection = connect(
            m_model, &SessionModel::dependencyChanged,
            this, &SpeedSkydivingWidget::onDependencyChanged);
    }

    refreshAll();
}

// ---------------------------------------------------------------------------
// Parameter keys
// ---------------------------------------------------------------------------

QStringList SpeedSkydivingWidget::parameterKeys() const
{
    return {
        QLatin1String(SessionKeys::SpPerfWindowHeight),
        QLatin1String(SessionKeys::SpValWindowHeight),
        QLatin1String(SessionKeys::SpBreakoffAlt)
    };
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void SpeedSkydivingWidget::refreshAll()
{
    if (!m_model || m_sessionId.isEmpty()) {
        blockEditorSignals(true);
        m_perfWindowHeightSpin->setValue(0.0);
        m_valWindowHeightSpin->setValue(0.0);
        m_breakoffAltSpin->setValue(0.0);
        m_speedResult->setText(QStringLiteral("Speed: --"));
        m_speedAccResult->setText(QStringLiteral("Speed Accuracy: --"));
        m_speedAccResult->setStyleSheet(QString());
        m_restoreButton->setEnabled(false);
        blockEditorSignals(false);
        return;
    }

    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded()) {
        blockEditorSignals(true);
        m_perfWindowHeightSpin->setValue(0.0);
        m_valWindowHeightSpin->setValue(0.0);
        m_breakoffAltSpin->setValue(0.0);
        m_speedResult->setText(QStringLiteral("Speed: --"));
        m_speedAccResult->setText(QStringLiteral("Speed Accuracy: --"));
        m_speedAccResult->setStyleSheet(QString());
        m_restoreButton->setEnabled(false);
        blockEditorSignals(false);
        return;
    }

    refreshParameters();
    refreshResults();
    refreshRestoreButton();
}

void SpeedSkydivingWidget::refreshParameters()
{
    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded())
        return;

    const SessionData& session = *sr.session;

    blockEditorSignals(true);

    // Performance window height (stored in metres, display in feet)
    QVariant perfH = session.getAttribute(QLatin1String(SessionKeys::SpPerfWindowHeight));
    double perfHFt = perfH.isValid() ? perfH.toDouble() * kFtPerM : kFaiPerfWindowHeightFt;
    m_perfWindowHeightSpin->setValue(qRound(perfHFt));
    static_cast<StepCommitSpinBox*>(m_perfWindowHeightSpin)->deselectText();

    QFont perfFont = m_perfWindowHeightSpin->font();
    perfFont.setBold(session.hasAttribute(QLatin1String(SessionKeys::SpPerfWindowHeight)));
    m_perfWindowHeightSpin->setFont(perfFont);

    // Validation window height
    QVariant valH = session.getAttribute(QLatin1String(SessionKeys::SpValWindowHeight));
    double valHFt = valH.isValid() ? valH.toDouble() * kFtPerM : kFaiValWindowHeightFt;
    m_valWindowHeightSpin->setValue(qRound(valHFt));
    static_cast<StepCommitSpinBox*>(m_valWindowHeightSpin)->deselectText();

    QFont valFont = m_valWindowHeightSpin->font();
    valFont.setBold(session.hasAttribute(QLatin1String(SessionKeys::SpValWindowHeight)));
    m_valWindowHeightSpin->setFont(valFont);

    // Breakoff altitude
    QVariant breakoff = session.getAttribute(QLatin1String(SessionKeys::SpBreakoffAlt));
    double breakoffFt = breakoff.isValid() ? breakoff.toDouble() * kFtPerM : kFaiBreakoffAltFt;
    m_breakoffAltSpin->setValue(qRound(breakoffFt));
    static_cast<StepCommitSpinBox*>(m_breakoffAltSpin)->deselectText();

    QFont breakoffFont = m_breakoffAltSpin->font();
    breakoffFont.setBold(session.hasAttribute(QLatin1String(SessionKeys::SpBreakoffAlt)));
    m_breakoffAltSpin->setFont(breakoffFont);

    blockEditorSignals(false);
}

void SpeedSkydivingWidget::refreshResults()
{
    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded())
        return;

    const SessionData& session = *sr.session;

    // Speed result (stored m/s, display km/h, 2 decimal places)
    QVariant speedVal = session.getAttribute(QLatin1String(SessionKeys::SpSpeedResult));
    if (speedVal.isValid() && !speedVal.isNull())
        m_speedResult->setText(QStringLiteral("Speed: ")
                               + QString::number(speedVal.toDouble() * 3.6, 'f', 2)
                               + QStringLiteral(" km/h"));
    else
        m_speedResult->setText(QStringLiteral("Speed: --"));

    // Speed accuracy result (m/s, 2 decimal places)
    QVariant accVal = session.getAttribute(QLatin1String(SessionKeys::SpMaxSpeedAcc));
    if (accVal.isValid() && !accVal.isNull()) {
        double acc = accVal.toDouble();
        m_speedAccResult->setText(QStringLiteral("Speed Accuracy: ")
                                  + QString::number(acc, 'f', 2)
                                  + QStringLiteral(" m/s"));
        if (acc >= 3.0)
            m_speedAccResult->setStyleSheet(QStringLiteral("color: red;"));
        else
            m_speedAccResult->setStyleSheet(QString());
    } else {
        m_speedAccResult->setText(QStringLiteral("Speed Accuracy: --"));
        m_speedAccResult->setStyleSheet(QString());
    }
}

void SpeedSkydivingWidget::refreshRestoreButton()
{
    m_restoreButton->setEnabled(!isAtFaiDefaults());
}

bool SpeedSkydivingWidget::isAtFaiDefaults() const
{
    if (!m_model || m_sessionId.isEmpty())
        return true;

    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return true;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded())
        return true;

    const SessionData& session = *sr.session;

    bool hasPerfH    = session.hasAttribute(QLatin1String(SessionKeys::SpPerfWindowHeight));
    bool hasValH     = session.hasAttribute(QLatin1String(SessionKeys::SpValWindowHeight));
    bool hasBreakoff = session.hasAttribute(QLatin1String(SessionKeys::SpBreakoffAlt));

    return !hasPerfH && !hasValH && !hasBreakoff;
}

// ---------------------------------------------------------------------------
// Signal blocking
// ---------------------------------------------------------------------------

void SpeedSkydivingWidget::blockEditorSignals(bool block)
{
    m_perfWindowHeightSpin->blockSignals(block);
    m_valWindowHeightSpin->blockSignals(block);
    m_breakoffAltSpin->blockSignals(block);
}

// ---------------------------------------------------------------------------
// Write-through
// ---------------------------------------------------------------------------

void SpeedSkydivingWidget::writeAttribute(const QString& key, const QVariant& value)
{
    if (!m_model || m_sessionId.isEmpty())
        return;
    m_model->updateAttribute(m_sessionId, key, value);
}

void SpeedSkydivingWidget::onPerfWindowHeightChanged()
{
    // Convert displayed feet to metres for storage
    double metres = m_perfWindowHeightSpin->value() / kFtPerM;
    writeAttribute(QLatin1String(SessionKeys::SpPerfWindowHeight), metres);
}

void SpeedSkydivingWidget::onValWindowHeightChanged()
{
    double metres = m_valWindowHeightSpin->value() / kFtPerM;
    writeAttribute(QLatin1String(SessionKeys::SpValWindowHeight), metres);
}

void SpeedSkydivingWidget::onBreakoffAltChanged()
{
    double metres = m_breakoffAltSpin->value() / kFtPerM;
    writeAttribute(QLatin1String(SessionKeys::SpBreakoffAlt), metres);
}

void SpeedSkydivingWidget::onRestoreFaiDefaults()
{
    if (!m_model || m_sessionId.isEmpty())
        return;

    m_model->removeAttribute(m_sessionId, QLatin1String(SessionKeys::SpPerfWindowHeight));
    m_model->removeAttribute(m_sessionId, QLatin1String(SessionKeys::SpValWindowHeight));
    m_model->removeAttribute(m_sessionId, QLatin1String(SessionKeys::SpBreakoffAlt));
}

// ---------------------------------------------------------------------------
// Dependency tracking
// ---------------------------------------------------------------------------

void SpeedSkydivingWidget::onDependencyChanged(const QString& sessionId,
                                                const DependencyKey& /*key*/)
{
    if (sessionId == m_sessionId)
        refreshAll();
}

} // namespace FlySight
