#include "calculatedvalue.h"

namespace FlySight {

template<typename Key, typename Value>
QMap<Key, typename CalculatedValue<Key, Value>::CalculationFunction> CalculatedValue<Key, Value>::s_calculations;

template<typename Key, typename Value>
void CalculatedValue<Key, Value>::registerCalculation(const Key &key, CalculationFunction func)
{
    s_calculations[key] = func;
}

template<typename Key, typename Value>
bool CalculatedValue<Key, Value>::hasCalculation(const Key &key) const
{
    return s_calculations.contains(key);
}

template<typename Key, typename Value>
std::optional<Value> CalculatedValue<Key, Value>::getValue(SessionData &session, const Key &key) const
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

    // Check if a calculation function is registered
    if (!s_calculations.contains(key)) {
        qWarning() << "No calculation registered for key:" << key;
        return std::nullopt;
    }

    // Perform the calculation
    m_activeCalculations.insert(key);
    std::optional<Value> result = s_calculations[key](session);
    m_activeCalculations.remove(key);

    if (result.has_value()) {
        m_cache.insert(key, result.value());
    } else {
        qWarning() << "Calculation failed for key:" << key;
    }

    return result;
}

// Explicit template instantiations
using MeasurementKey = QPair<QString, QString>;
template class CalculatedValue<MeasurementKey, QVector<double>>;
template class CalculatedValue<QString, QString>;

} // namespace FlySight
