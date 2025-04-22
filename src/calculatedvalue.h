#ifndef CALCULATEDVALUE_H
#define CALCULATEDVALUE_H

#include <QMap>
#include <QSet>
#include <QVector>
#include <QString>
#include <functional>
#include <QDebug>
#include <QVariant>
#include "dependencykey.h"

namespace FlySight {

class SessionData;

template<typename Key, typename Value>
class CalculatedValue
{
public:
    using CalculationFunction = std::function<std::optional<Value>(SessionData&)>;

    struct Method {
        QList<DependencyKey>      deps;
        CalculationFunction       func;
    };

    // Registers a calculation function for a given key.
    static void registerCalculation(const Key &key,
                                    const QList<DependencyKey>&deps,
                                    CalculationFunction func);

    // Checks if there's a registered calculation function for the given key.
    bool hasCalculation(const Key &key) const;

    // Retrieves the value associated with the given key, computing it if necessary.
    std::optional<Value> getValue(SessionData &session, const Key &key) const;

    // Invalidate a calculated value
    void invalidate(const Key& key);

private:
    mutable QMap<Key, Value> m_cache;
    mutable QSet<Key> m_activeCalculations;
    static QMap<Key, QVector<Method>> s_methods;
};

} // namespace FlySight

#endif // CALCULATEDVALUE_H
