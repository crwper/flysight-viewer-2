#ifndef CALCULATEDVALUEREGISTRY_H
#define CALCULATEDVALUEREGISTRY_H

namespace FlySight {

class CalculatedValueRegistry {
public:
    static CalculatedValueRegistry& instance();

    /// Registers all built-in calculated values (attributes and measurements).
    /// Call this once during application startup.
    void registerBuiltInCalculations();

private:
    CalculatedValueRegistry() = default;
    ~CalculatedValueRegistry() = default;
    CalculatedValueRegistry(const CalculatedValueRegistry&) = delete;
    CalculatedValueRegistry& operator=(const CalculatedValueRegistry&) = delete;
};

} // namespace FlySight

#endif // CALCULATEDVALUEREGISTRY_H
