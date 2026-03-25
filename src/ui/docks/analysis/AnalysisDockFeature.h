#ifndef ANALYSISDOCKFEATURE_H
#define ANALYSISDOCKFEATURE_H

#include "ui/docks/DockFeature.h"

namespace FlySight {

class AnalysisDockWidget;
class AnalysisMethodWidget;
class SessionModel;
struct AppContext;

/**
 * DockFeature for the Analysis dock.
 * Displays analysis method pages for the focused session.
 */
class AnalysisDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit AnalysisDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    AnalysisDockWidget* m_widget = nullptr;
    SessionModel* m_sessionModel = nullptr;
    QString m_focusedSessionId;

    /// Returns the method widget at the given index, or nullptr.
    AnalysisMethodWidget* methodWidgetAt(int index) const;

    /// Returns the currently active method widget, or nullptr.
    AnalysisMethodWidget* currentMethodWidget() const;

    void onFocusedSessionChanged(const QString& sessionId);
    void onMethodChanged(int index);
};

} // namespace FlySight

#endif // ANALYSISDOCKFEATURE_H
