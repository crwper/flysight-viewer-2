#ifndef DEPENDENCYMANAGER_H
#define DEPENDENCYMANAGER_H

#include "dependencykey.h"
#include "calculatedvalue.h"
#include <QMap>
#include <QSet>

namespace FlySight {

class DependencyManager {
public:
    void registerDependencies(
        const DependencyKey& thisKey,
        const QList<DependencyKey>& dependsOn);

    void invalidateKeyAndDependents(
        const DependencyKey& changedKey,
        CalculatedValue<QString, QVariant>& attributeCache,
        CalculatedValue<QPair<QString, QString>, QVector<double>>& measurementCache);

private:
    QMap<DependencyKey, QSet<DependencyKey>> m_reverseDeps;
};

} // namespace FlySight

#endif // DEPENDENCYMANAGER_H
