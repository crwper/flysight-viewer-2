#ifndef DOCKFEATURE_H
#define DOCKFEATURE_H

#include <QObject>
#include <QString>
#include <kddockwidgets/DockWidget.h>
#include <kddockwidgets/KDDockWidgets.h>

namespace FlySight {

/**
 * Abstract base class for all dock features.
 *
 * Each DockFeature owns:
 * - A KDDockWidgets::QtWidgets::DockWidget
 * - The widget(s) displayed in that dock
 * - Any presenters or coordinators for that dock
 *
 * MainWindow interacts with docks only through this interface.
 */
class DockFeature : public QObject
{
    Q_OBJECT

public:
    /**
     * Construct a DockFeature.
     * @param parent The parent QObject (typically MainWindow)
     */
    explicit DockFeature(QObject* parent = nullptr) : QObject(parent) {}

    virtual ~DockFeature() = default;

    /**
     * Unique identifier for this dock (used for layout save/restore).
     * Must be unique across all docks and stable across app launches.
     * Examples: "Logbook", "Legend", "Plots", "Map", "Video"
     */
    virtual QString id() const = 0;

    /**
     * Human-readable title for the dock.
     * Displayed in the dock title bar and Window menu.
     */
    virtual QString title() const = 0;

    /**
     * The KDDockWidgets DockWidget managed by this feature.
     * Used by MainWindow for addDockWidget() and layout management.
     */
    virtual KDDockWidgets::QtWidgets::DockWidget* dock() const = 0;

    /**
     * Toggle action for showing/hiding this dock.
     * Used to populate the Window menu.
     * Default implementation returns dock()->toggleAction().
     */
    virtual QAction* toggleAction() const {
        auto* d = dock();
        return d ? d->toggleAction() : nullptr;
    }

    /**
     * Default location for this dock when first added.
     * Used by MainWindow when no saved layout exists.
     */
    virtual KDDockWidgets::Location defaultLocation() const = 0;
};

} // namespace FlySight

#endif // DOCKFEATURE_H
