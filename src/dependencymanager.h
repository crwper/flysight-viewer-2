#ifndef DEPENDENCYMANAGER_H
#define DEPENDENCYMANAGER_H

#include "dependencykey.h"
#include "calculatedvalue.h"
#include <QMap>

namespace FlySight {

class DependencyManager {
public:
    static void registerDependencies(
        const DependencyKey& thisKey,
        const QList<DependencyKey>& dependsOn);

    static void invalidateKeyAndDependents(
        const DependencyKey& changedKey,
        CalculatedValue<QString, QVariant>& attributeCache,
        CalculatedValue<QPair<QString, QString>, QVector<double>>& measurementCache);

private:
    static QMap<DependencyKey, QSet<DependencyKey>> s_reverseDeps;
};

} // namespace FlySight

#endif // DEPENDENCYMANAGER_H
