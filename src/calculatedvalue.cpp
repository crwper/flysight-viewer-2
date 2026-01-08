#include "calculatedvalue.h"
#include "sessiondata.h"
#include <type_traits>

namespace FlySight {

template<typename Key, typename Value>
QMap<Key, QVector<typename CalculatedValue<Key, Value>::Method>>
    CalculatedValue<Key, Value>::s_methods;

template<typename Key, typename Value>
void CalculatedValue<Key, Value>::registerCalculation(const Key &key,
                                                      const QList<DependencyKey>& deps,
                                                      CalculationFunction func)
{
    s_methods[key].append(Method{deps, std::move(func)});
}

template<typename Key, typename Value>
bool CalculatedValue<Key, Value>::hasCalculation(const Key &key) const
{
    return s_methods.contains(key);
}

template<typename Key, typename Value>
std::optional<Value> CalculatedValue<Key, Value>::getValue(SessionData &session,
                                                           const Key &key) const
{
    // If the value is already computed and cached
    if (m_cache.contains(key)) {
        return m_cache.value(key);
    }

    // Check for circular dependencies
    if (m_activeCalculations.contains(key)) {
        qWarning() << "Circular dependency detected for key:" << key;
        return std::nullopt;
    }

    // Check if a calculation method is registered
    if (!s_methods.contains(key)) {
        return std::nullopt;
    }

    // Perform the calculation
    m_activeCalculations.insert(key);

    const auto& recipes = s_methods[key];
    auto depsSatisfied  = [&](const QList<DependencyKey>& deps) -> bool {
        for (const auto& d : deps) {
            if (d.type == DependencyKey::Type::Attribute) {
                if (!session.getAttribute(d.attributeKey).isValid())
                    return false;
            } else {
                if (session.getMeasurement(d.measurementKey.first,
                                           d.measurementKey.second).isEmpty())
                    return false;
            }
        }
        return true;
    };

    std::optional<Value> result;

    for (const auto& r : recipes) {
        if (!depsSatisfied(r.deps))
            continue;

        // Try the recipe
        result = r.func(session);
        if (result.has_value()) {
            // Build concrete reverse‑deps graph
            DependencyKey thisKey = toDependencyKey(key);
            session.addDependencies(thisKey, r.deps);
            break;
        }
    }

    m_activeCalculations.remove(key);

    if (result.has_value()) {
        m_cache.insert(key, *result);
    }

    return result;
}

template<typename Key, typename Value>
void CalculatedValue<Key, Value>::setValue(const Key &key, const Value& data)
{
    m_cache.insert(key, data);
}

template<typename Key, typename Value>
void CalculatedValue<Key, Value>::invalidate(const Key& key)
{
    m_cache.remove(key);
}

// Explicit template instantiations
using MeasurementKey = QPair<QString, QString>;
template class CalculatedValue<MeasurementKey, QVector<double>>;
template class CalculatedValue<QString, QVariant>;

} // namespace FlySight
