#include "LogbookDockFeature.h"
#include "ui/docks/AppContext.h"
#include "LogbookView.h"
#include "sessionmodel.h"

namespace FlySight {

LogbookDockFeature::LogbookDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Logbook"));

    // Create LogbookView
    m_logbookView = new LogbookView(ctx.sessionModel, m_dock);
    m_dock->setWidget(m_logbookView);

    // Forward LogbookView signals to feature signals
    connect(m_logbookView, &LogbookView::showSelectedRequested,
            this, &LogbookDockFeature::showSelectedRequested);
    connect(m_logbookView, &LogbookView::hideSelectedRequested,
            this, &LogbookDockFeature::hideSelectedRequested);
    connect(m_logbookView, &LogbookView::hideOthersRequested,
            this, &LogbookDockFeature::hideOthersRequested);
    connect(m_logbookView, &LogbookView::deleteRequested,
            this, &LogbookDockFeature::deleteRequested);
    connect(m_logbookView, &LogbookView::focusSessionRequested,
            this, &LogbookDockFeature::focusSessionRequested);

    // Forward focused session changes to the model
    connect(m_logbookView, &LogbookView::currentSessionChanged,
            ctx.sessionModel, &SessionModel::setFocusedSessionId);

    // Connect progress signals to progress bar updates
    connect(ctx.sessionModel, &SessionModel::saveProgressChanged,
            m_logbookView, &LogbookView::onSaveProgressChanged);

    // Connect loader progress and cancel signals
    connect(ctx.sessionModel, &SessionModel::loadProgressChanged,
            m_logbookView, &LogbookView::onLoadProgressChanged);
    connect(m_logbookView, &LogbookView::cancelLoaderRequested,
            ctx.sessionModel, &SessionModel::cancelLoader);

    // Connect column worker progress and cancel signals
    connect(ctx.sessionModel, &SessionModel::columnWorkerProgressChanged,
            m_logbookView, &LogbookView::onColumnWorkerProgressChanged);
    connect(m_logbookView, &LogbookView::cancelColumnWorkerRequested,
            ctx.sessionModel, &SessionModel::cancelColumnWorker);

    // Connect bulk edit progress and cancel signals
    connect(ctx.sessionModel, &SessionModel::bulkEditProgressChanged,
            m_logbookView, &LogbookView::onBulkEditProgressChanged);
    connect(m_logbookView, &LogbookView::cancelBulkEditRequested,
            ctx.sessionModel, &SessionModel::cancelBulkEdit);
}

QString LogbookDockFeature::id() const
{
    return QStringLiteral("Logbook");
}

QString LogbookDockFeature::title() const
{
    return QStringLiteral("Logbook");
}

KDDockWidgets::QtWidgets::DockWidget* LogbookDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location LogbookDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnRight;
}

} // namespace FlySight
