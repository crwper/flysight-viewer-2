#ifndef LOGBOOKDOCKFEATURE_H
#define LOGBOOKDOCKFEATURE_H

#include "ui/docks/DockFeature.h"

namespace FlySight {

class LogbookView;
struct AppContext;

/**
 * DockFeature for the logbook view.
 * Displays session list with context menu actions.
 */
class LogbookDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit LogbookDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

    LogbookView* logbookView() const { return m_logbookView; }

signals:
    // Re-expose LogbookView signals for MainWindow to connect
    void showSelectedRequested();
    void hideSelectedRequested();
    void hideOthersRequested();
    void deleteRequested();

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    LogbookView* m_logbookView = nullptr;
};

} // namespace FlySight

#endif // LOGBOOKDOCKFEATURE_H
