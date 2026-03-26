#include "AnalysisDockFeature.h"
#include "AnalysisDockWidget.h"
#include "AnalysisMethodWidget.h"
#include "WingsuitPerformanceWidget.h"
#include "SpeedSkydivingWidget.h"
#include "ui/docks/AppContext.h"
#include "sessionmodel.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

// ---------------------------------------------------------------------------
// AnalysisDockFeature
// ---------------------------------------------------------------------------

AnalysisDockFeature::AnalysisDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Analysis"));

    // Create inner widget
    m_widget = new AnalysisDockWidget(m_dock);
    m_dock->setWidget(m_widget);

    // Store session model reference
    m_sessionModel = ctx.sessionModel;

    // Register method pages
    auto* wspWidget = new WingsuitPerformanceWidget();
    m_widget->addMethodPage(tr("Wingsuit Performance"), wspWidget);

    auto* spWidget = new SpeedSkydivingWidget();
    m_widget->addMethodPage(tr("Speed Skydiving"), spWidget);

    // Connect focused session tracking
    connect(m_sessionModel, &SessionModel::focusedSessionChanged,
            this, &AnalysisDockFeature::onFocusedSessionChanged);

    // Connect method selector changes
    connect(m_widget, &AnalysisDockWidget::methodChanged,
            this, &AnalysisDockFeature::onMethodChanged);

    // Restore persisted analysis method
    {
        auto& prefs = PreferencesManager::instance();
        QString savedMethod = prefs.getValue(PreferenceKeys::AnalysisMethod).toString();
        setCurrentMethodByName(savedMethod);
    }

    // Initialize state from the current focused session (if any)
    onFocusedSessionChanged(m_sessionModel->focusedSessionId());
}

QString AnalysisDockFeature::id() const
{
    return QStringLiteral("Analysis");
}

QString AnalysisDockFeature::title() const
{
    return QStringLiteral("Analysis");
}

KDDockWidgets::QtWidgets::DockWidget* AnalysisDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location AnalysisDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnRight;
}

AnalysisMethodWidget* AnalysisDockFeature::methodWidgetAt(int index) const
{
    if (!m_widget)
        return nullptr;

    return qobject_cast<AnalysisMethodWidget*>(m_widget->methodPage(index));
}

AnalysisMethodWidget* AnalysisDockFeature::currentMethodWidget() const
{
    if (!m_widget)
        return nullptr;
    return methodWidgetAt(m_widget->currentMethodIndex());
}

void AnalysisDockFeature::onFocusedSessionChanged(const QString& sessionId)
{
    m_focusedSessionId = sessionId;
    m_widget->setSessionActive(!sessionId.isEmpty());

    // Notify the current method page
    if (auto* method = currentMethodWidget())
        method->setFocusedSession(m_sessionModel, sessionId);
}

void AnalysisDockFeature::onMethodChanged(int index)
{
    // Notify the newly selected method page about the current session
    if (auto* method = methodWidgetAt(index))
        method->setFocusedSession(m_sessionModel, m_focusedSessionId);

    // Persist the method selection
    if (m_widget) {
        QString name = m_widget->methodName(index);
        if (!name.isEmpty())
            PreferencesManager::instance().setValue(PreferenceKeys::AnalysisMethod, name);
    }
}

QString AnalysisDockFeature::currentMethodName() const
{
    if (!m_widget)
        return QString();
    return m_widget->methodName(m_widget->currentMethodIndex());
}

void AnalysisDockFeature::setCurrentMethodByName(const QString& name)
{
    if (!m_widget)
        return;
    for (int i = 0; i < m_widget->methodCount(); ++i) {
        if (m_widget->methodName(i) == name) {
            m_widget->setCurrentMethodIndex(i);
            return;
        }
    }
}

} // namespace FlySight
