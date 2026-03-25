#include "WingsuitPerformanceWidget.h"
#include "sessionmodel.h"
#include "sessiondata.h"

#include <QButtonGroup>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace FlySight {

// FAI default altitudes (metres AGL)
static constexpr double kFaiTopAlt    = 2500.0;
static constexpr double kFaiBottomAlt = 1500.0;

// ---------------------------------------------------------------------------
// SpinBox that commits on arrow-button clicks (not just Enter / focus-out)
// ---------------------------------------------------------------------------

class StepCommitSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;

    int  lastStepCount() const { return m_lastSteps; }
    void clearStep()           { m_lastSteps = 0; }
    void deselectText()        { lineEdit()->deselect(); }

    void stepBy(int steps) override
    {
        m_lastSteps = steps;
        QDoubleSpinBox::stepBy(steps);
        deselectText();
        emit editingFinished();
    }

private:
    int m_lastSteps = 0;
};

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

WingsuitPerformanceWidget::WingsuitPerformanceWidget(QWidget* parent)
    : AnalysisMethodWidget(parent)
{
    buildLayout();
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void WingsuitPerformanceWidget::buildLayout()
{
    auto* mainLayout = new QVBoxLayout(this);

    // --- Parameter section ---
    auto* paramForm = new QFormLayout;
    m_topAltSpin = new StepCommitSpinBox;
    m_topAltSpin->setRange(0.0, 99999.0);
    m_topAltSpin->setDecimals(0);
    m_topAltSpin->setSingleStep(10.0);
    paramForm->addRow(tr("Top of window (m):"), m_topAltSpin);

    m_bottomAltSpin = new StepCommitSpinBox;
    m_bottomAltSpin->setRange(0.0, 99999.0);
    m_bottomAltSpin->setDecimals(0);
    m_bottomAltSpin->setSingleStep(10.0);
    paramForm->addRow(tr("Bottom of window (m):"), m_bottomAltSpin);

    mainLayout->addLayout(paramForm);

    // --- Restore FAI Defaults button ---
    m_restoreButton = new QPushButton(tr("Restore FAI Defaults"));
    mainLayout->addWidget(m_restoreButton);

    // --- Separator ---
    auto* separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // --- Results grid ---
    auto* resultsGrid = new QGridLayout;

    m_timeRadio     = new QRadioButton(tr("Time"));
    m_distanceRadio = new QRadioButton(tr("Distance"));
    m_speedRadio    = new QRadioButton(tr("Speed"));

    auto* taskGroup = new QButtonGroup(this);
    taskGroup->addButton(m_timeRadio);
    taskGroup->addButton(m_distanceRadio);
    taskGroup->addButton(m_speedRadio);

    m_timeResult     = new QLabel(QStringLiteral("--"));
    m_distanceResult = new QLabel(QStringLiteral("--"));
    m_speedResult    = new QLabel(QStringLiteral("--"));

    m_timeResult->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_distanceResult->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_speedResult->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    resultsGrid->addWidget(m_timeRadio,     0, 0);
    resultsGrid->addWidget(m_timeResult,     0, 1);
    resultsGrid->addWidget(m_distanceRadio, 1, 0);
    resultsGrid->addWidget(m_distanceResult, 1, 1);
    resultsGrid->addWidget(m_speedRadio,    2, 0);
    resultsGrid->addWidget(m_speedResult,    2, 1);

    mainLayout->addLayout(resultsGrid);

    // --- SEP accuracy ---
    auto* sepSeparator = new QFrame;
    sepSeparator->setFrameShape(QFrame::HLine);
    sepSeparator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sepSeparator);

    m_sepResult = new QLabel(QStringLiteral("Maximum SEP: --"));
    mainLayout->addWidget(m_sepResult);

    // --- Stretch ---
    mainLayout->addStretch();

    // --- Connections ---
    connect(m_topAltSpin, &QDoubleSpinBox::editingFinished,
            this, &WingsuitPerformanceWidget::onTopAltChanged);
    connect(m_bottomAltSpin, &QDoubleSpinBox::editingFinished,
            this, &WingsuitPerformanceWidget::onBottomAltChanged);

    connect(m_timeRadio, &QRadioButton::toggled,
            this, &WingsuitPerformanceWidget::onTaskChanged);
    connect(m_distanceRadio, &QRadioButton::toggled,
            this, &WingsuitPerformanceWidget::onTaskChanged);
    connect(m_speedRadio, &QRadioButton::toggled,
            this, &WingsuitPerformanceWidget::onTaskChanged);

    connect(m_restoreButton, &QPushButton::clicked,
            this, &WingsuitPerformanceWidget::onRestoreFaiDefaults);
}

// ---------------------------------------------------------------------------
// Session binding
// ---------------------------------------------------------------------------

void WingsuitPerformanceWidget::setFocusedSession(SessionModel* model, const QString& sessionId)
{
    // Disconnect previous dependency tracking
    if (m_dependencyConnection)
        QObject::disconnect(m_dependencyConnection);

    m_model = model;
    m_sessionId = sessionId;

    // Connect new dependency tracking
    if (m_model && !m_sessionId.isEmpty()) {
        m_dependencyConnection = connect(
            m_model, &SessionModel::dependencyChanged,
            this, &WingsuitPerformanceWidget::onDependencyChanged);
    }

    refreshAll();
}

// ---------------------------------------------------------------------------
// Parameter keys
// ---------------------------------------------------------------------------

QStringList WingsuitPerformanceWidget::parameterKeys() const
{
    return {
        QLatin1String(SessionKeys::WspVersion),
        QLatin1String(SessionKeys::WspTopAlt),
        QLatin1String(SessionKeys::WspBottomAlt),
        QLatin1String(SessionKeys::WspTask)
    };
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void WingsuitPerformanceWidget::refreshAll()
{
    if (!m_model || m_sessionId.isEmpty()) {
        // Clear to empty state
        blockEditorSignals(true);
        m_topAltSpin->setValue(0.0);
        m_bottomAltSpin->setValue(0.0);
        m_timeRadio->setChecked(false);
        m_distanceRadio->setChecked(false);
        m_speedRadio->setChecked(false);
        m_timeResult->setText(QStringLiteral("--"));
        m_distanceResult->setText(QStringLiteral("--"));
        m_speedResult->setText(QStringLiteral("--"));
        m_sepResult->setText(QStringLiteral("Maximum SEP: --"));
        m_sepResult->setStyleSheet(QString());
        m_restoreButton->setEnabled(false);
        blockEditorSignals(false);
        return;
    }

    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded()) {
        // Unloaded session -- show empty state without crashing
        blockEditorSignals(true);
        m_topAltSpin->setValue(0.0);
        m_bottomAltSpin->setValue(0.0);
        m_timeRadio->setChecked(false);
        m_distanceRadio->setChecked(false);
        m_speedRadio->setChecked(false);
        m_timeResult->setText(QStringLiteral("--"));
        m_distanceResult->setText(QStringLiteral("--"));
        m_speedResult->setText(QStringLiteral("--"));
        m_sepResult->setText(QStringLiteral("Maximum SEP: --"));
        m_sepResult->setStyleSheet(QString());
        m_restoreButton->setEnabled(false);
        blockEditorSignals(false);
        return;
    }

    refreshParameters();
    refreshResults();
    refreshRestoreButton();
}

void WingsuitPerformanceWidget::refreshParameters()
{
    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded())
        return;

    const SessionData& session = *sr.session;

    blockEditorSignals(true);

    // Top altitude
    QVariant topAlt = session.getAttribute(QLatin1String(SessionKeys::WspTopAlt));
    m_topAltSpin->setValue(topAlt.isValid() ? topAlt.toDouble() : kFaiTopAlt);
    static_cast<StepCommitSpinBox*>(m_topAltSpin)->deselectText();

    // Bold if user-set
    QFont topFont = m_topAltSpin->font();
    topFont.setBold(session.hasAttribute(QLatin1String(SessionKeys::WspTopAlt)));
    m_topAltSpin->setFont(topFont);

    // Bottom altitude
    QVariant bottomAlt = session.getAttribute(QLatin1String(SessionKeys::WspBottomAlt));
    m_bottomAltSpin->setValue(bottomAlt.isValid() ? bottomAlt.toDouble() : kFaiBottomAlt);
    static_cast<StepCommitSpinBox*>(m_bottomAltSpin)->deselectText();

    QFont bottomFont = m_bottomAltSpin->font();
    bottomFont.setBold(session.hasAttribute(QLatin1String(SessionKeys::WspBottomAlt)));
    m_bottomAltSpin->setFont(bottomFont);

    // Task
    QVariant taskVal = session.getAttribute(QLatin1String(SessionKeys::WspTask));
    QString task = taskVal.isValid() ? taskVal.toString() : QStringLiteral("Time");

    if (task == QLatin1String("Time"))
        m_timeRadio->setChecked(true);
    else if (task == QLatin1String("Distance"))
        m_distanceRadio->setChecked(true);
    else if (task == QLatin1String("Speed"))
        m_speedRadio->setChecked(true);

    blockEditorSignals(false);
}

void WingsuitPerformanceWidget::refreshResults()
{
    int row = m_model->getSessionRow(m_sessionId);
    if (row < 0)
        return;

    const SessionRow& sr = m_model->rowAt(row);
    if (!sr.isLoaded())
        return;

    const SessionData& session = *sr.session;

    // Time result
    QVariant timeVal = session.getAttribute(QLatin1String(SessionKeys::WspTimeResult));
    if (timeVal.isValid() && !timeVal.isNull())
        m_timeResult->setText(QString::number(timeVal.toDouble(), 'f', 1) + QStringLiteral(" s"));
    else
        m_timeResult->setText(QStringLiteral("--"));

    // Distance result
    QVariant distVal = session.getAttribute(QLatin1String(SessionKeys::WspDistResult));
    if (distVal.isValid() && !distVal.isNull())
        m_distanceResult->setText(QString::number(qRound(distVal.toDouble())) + QStringLiteral(" m"));
    else
        m_distanceResult->setText(QStringLiteral("--"));

    // Speed result (stored m/s, display km/h)
    QVariant speedVal = session.getAttribute(QLatin1String(SessionKeys::WspSpeedResult));
    if (speedVal.isValid() && !speedVal.isNull())
        m_speedResult->setText(QString::number(speedVal.toDouble() * 3.6, 'f', 1) + QStringLiteral(" km/h"));
    else
        m_speedResult->setText(QStringLiteral("--"));

    // SEP result
    QVariant sepVal = session.getAttribute(QLatin1String(SessionKeys::WspSepResult));
    if (sepVal.isValid() && !sepVal.isNull()) {
        double sep = sepVal.toDouble();
        m_sepResult->setText(QStringLiteral("Maximum SEP: ") + QString::number(sep, 'f', 1) + QStringLiteral(" m"));
        if (sep >= 10.0)
            m_sepResult->setStyleSheet(QStringLiteral("color: red;"));
        else
            m_sepResult->setStyleSheet(QString());
    } else {
        m_sepResult->setText(QStringLiteral("Maximum SEP: --"));
        m_sepResult->setStyleSheet(QString());
    }
}

void WingsuitPerformanceWidget::refreshRestoreButton()
{
    m_restoreButton->setEnabled(!isAtFaiDefaults());
}

bool WingsuitPerformanceWidget::isAtFaiDefaults() const
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

    // At FAI defaults when neither altitude is stored as a user attribute
    bool hasTop = session.hasAttribute(QLatin1String(SessionKeys::WspTopAlt));
    bool hasBottom = session.hasAttribute(QLatin1String(SessionKeys::WspBottomAlt));

    return !hasTop && !hasBottom;
}

// ---------------------------------------------------------------------------
// Signal blocking
// ---------------------------------------------------------------------------

void WingsuitPerformanceWidget::blockEditorSignals(bool block)
{
    m_topAltSpin->blockSignals(block);
    m_bottomAltSpin->blockSignals(block);
    m_timeRadio->blockSignals(block);
    m_distanceRadio->blockSignals(block);
    m_speedRadio->blockSignals(block);
}

// ---------------------------------------------------------------------------
// Write-through
// ---------------------------------------------------------------------------

void WingsuitPerformanceWidget::writeAttribute(const QString& key, const QVariant& value)
{
    if (!m_model || m_sessionId.isEmpty())
        return;
    m_model->updateAttribute(m_sessionId, key, value);
}

void WingsuitPerformanceWidget::onTopAltChanged()
{
    auto* top = static_cast<StepCommitSpinBox*>(m_topAltSpin);

    writeAttribute(QLatin1String(SessionKeys::WspTopAlt), top->value());

    // Arrow click: shift bottom by the same amount to move the window as a whole
    int steps = top->lastStepCount();
    if (steps != 0) {
        top->clearStep();
        double delta = steps * top->singleStep();
        blockEditorSignals(true);
        m_bottomAltSpin->setValue(m_bottomAltSpin->value() + delta);
        blockEditorSignals(false);
        writeAttribute(QLatin1String(SessionKeys::WspBottomAlt), m_bottomAltSpin->value());
    }
}

void WingsuitPerformanceWidget::onBottomAltChanged()
{
    auto* bot = static_cast<StepCommitSpinBox*>(m_bottomAltSpin);

    writeAttribute(QLatin1String(SessionKeys::WspBottomAlt), bot->value());

    // Arrow click: shift top by the same amount to move the window as a whole
    int steps = bot->lastStepCount();
    if (steps != 0) {
        bot->clearStep();
        double delta = steps * bot->singleStep();
        blockEditorSignals(true);
        m_topAltSpin->setValue(m_topAltSpin->value() + delta);
        blockEditorSignals(false);
        writeAttribute(QLatin1String(SessionKeys::WspTopAlt), m_topAltSpin->value());
    }
}

void WingsuitPerformanceWidget::onTaskChanged()
{
    // toggled fires twice (uncheck + check); only act on the checked button
    QString task;
    if (m_timeRadio->isChecked())
        task = QStringLiteral("Time");
    else if (m_distanceRadio->isChecked())
        task = QStringLiteral("Distance");
    else if (m_speedRadio->isChecked())
        task = QStringLiteral("Speed");
    else
        return; // no button checked (during clear)

    writeAttribute(QLatin1String(SessionKeys::WspTask), task);
}

void WingsuitPerformanceWidget::onRestoreFaiDefaults()
{
    if (!m_model || m_sessionId.isEmpty())
        return;

    m_model->removeAttribute(m_sessionId, QLatin1String(SessionKeys::WspTopAlt));
    m_model->removeAttribute(m_sessionId, QLatin1String(SessionKeys::WspBottomAlt));
}

// ---------------------------------------------------------------------------
// Dependency tracking
// ---------------------------------------------------------------------------

void WingsuitPerformanceWidget::onDependencyChanged(const QString& sessionId,
                                                     const DependencyKey& /*key*/)
{
    if (sessionId == m_sessionId)
        refreshAll();
}

} // namespace FlySight
