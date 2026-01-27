#ifndef DOCKREGISTRY_H
#define DOCKREGISTRY_H

#include <QList>

namespace FlySight {

class DockFeature;
struct AppContext;

/**
 * Factory for creating all dock features.
 *
 * This is NOT a singleton. It creates DockFeature instances that
 * are then owned by MainWindow.
 *
 * Usage:
 *   AppContext ctx { m_sessionModel, m_plotModel, ... };
 *   QList<DockFeature*> features = DockRegistry::createAll(ctx, this);
 */
class DockRegistry
{
public:
    /**
     * Create all dock features.
     * @param ctx The application context with shared services
     * @param parent The parent QObject (typically MainWindow) that will own the features
     * @return List of created DockFeature instances (caller owns via parent)
     */
    static QList<DockFeature*> createAll(const AppContext& ctx, QObject* parent);

private:
    // Prevent instantiation - this is a static factory
    DockRegistry() = delete;
};

} // namespace FlySight

#endif // DOCKREGISTRY_H
