// dependencyManager.cpp
#include "dependencymanager.h"
#include <QQueue>

namespace FlySight {

void DependencyManager::registerDependencies(
    const DependencyKey& thisKey,
    const QList<DependencyKey>& dependsOn)
{
    for (const DependencyKey& d : dependsOn) {
        // "thisKey" is the newly registered key as a DependencyKey
        m_reverseDeps[d].insert(thisKey);
    }
}

void DependencyManager::invalidateKeyAndDependents(
    const DependencyKey& changedKey,
    CalculatedValue<QString, QVariant>& attributeCache,
    CalculatedValue<QPair<QString, QString>, QVector<double>>& measurementCache)
{
    // We'll do a simple BFS or DFS
    QQueue<DependencyKey> queue;
    QSet<DependencyKey> visited;

    queue.enqueue(changedKey);
    visited.insert(changedKey);

    while (!queue.isEmpty()) {
        DependencyKey current = queue.dequeue();

        // For all keys that directly depend on 'current'
        const auto &depSet = m_reverseDeps[current];
        for (const DependencyKey& depKey : depSet) {
            if (!visited.contains(depKey)) {
                visited.insert(depKey);
                queue.enqueue(depKey);
            }
        }

        // Once we have current, we actually invalidate it in the caches:
        if (current.type == DependencyKey::Type::Attribute) {
            // remove from the attribute calculated cache
            attributeCache.invalidate(current.attributeKey);
        } else {
            // remove from the measurement calculated cache
            measurementCache.invalidate(current.measurementKey);
        }
    }
}

} // namespace FlySight
