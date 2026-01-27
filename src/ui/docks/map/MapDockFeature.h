#ifndef MAPDOCKFEATURE_H
#define MAPDOCKFEATURE_H

#include "ui/docks/DockFeature.h"

namespace FlySight {

class MapWidget;
struct AppContext;

/**
 * DockFeature for the map view.
 * Displays GNSS tracks on a Qt Location map.
 */
class MapDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit MapDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    MapWidget* m_mapWidget = nullptr;
};

} // namespace FlySight

#endif // MAPDOCKFEATURE_H
