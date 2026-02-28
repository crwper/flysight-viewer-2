#ifndef MARKERSELECTIONDOCKFEATURE_H
#define MARKERSELECTIONDOCKFEATURE_H

#include "ui/docks/DockFeature.h"
#include <QComboBox>
#include <QLabel>
#include <QTreeView>

namespace FlySight {

struct AppContext;
class PlotViewSettingsModel;
class MarkerModel;

/**
 * DockFeature for the marker selection tree view.
 * Displays available marker categories and individual markers,
 * plus a "Reference" dropdown for selecting the plot x-axis alignment marker.
 */
class MarkerSelectionDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit MarkerSelectionDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

private:
    void populateReferenceCombo();
    void onReferenceComboActivated(int index);
    void onReferenceMarkerKeyChanged(const QString &oldKey, const QString &newKey);

    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    QTreeView* m_treeView = nullptr;
    QComboBox* m_referenceCombo = nullptr;
    PlotViewSettingsModel* m_viewSettings = nullptr;
    MarkerModel* m_markerModel = nullptr;
};

} // namespace FlySight

#endif // MARKERSELECTIONDOCKFEATURE_H
