#ifndef LEGENDDOCKFEATURE_H
#define LEGENDDOCKFEATURE_H

#include "ui/docks/DockFeature.h"

namespace FlySight {

class LegendWidget;
class LegendPresenter;
struct AppContext;

/**
 * DockFeature for the legend view.
 * Displays current cursor values with LegendPresenter driving updates.
 */
class LegendDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit LegendDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    LegendWidget* m_legendWidget = nullptr;
    LegendPresenter* m_presenter = nullptr;
};

} // namespace FlySight

#endif // LEGENDDOCKFEATURE_H
