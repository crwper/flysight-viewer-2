#include "unitconverter.h"
#include "unitdefinitions.h"
#include "../preferences/preferencesmanager.h"
#include "../preferences/preferencekeys.h"

#include <cmath>

namespace FlySight {

UnitConverter::UnitConverter()
{
    // Read initial system from preferences, default to "Metric" if not set
    QVariant prefValue = PreferencesManager::instance().getValue(PreferenceKeys::GeneralUnits);
    m_currentSystem = prefValue.toString();

    // Ensure we have a valid system
    if (m_currentSystem.isEmpty()) {
        m_currentSystem = UnitSystems::Metric;
    }
}

double UnitConverter::convert(double value, const QString& measurementType) const
{
    // Handle empty or missing measurement type gracefully
    if (measurementType.isEmpty()) {
        return value;
    }

    const QMap<QString, MeasurementTypeInfo>& registry = getMeasurementTypeRegistry();

    // Check if measurement type exists
    auto typeIt = registry.find(measurementType);
    if (typeIt == registry.end()) {
        return value; // Unknown type, return unchanged
    }

    // Check if current system exists for this type
    const MeasurementTypeInfo& typeInfo = typeIt.value();
    auto systemIt = typeInfo.systems.find(m_currentSystem);
    if (systemIt == typeInfo.systems.end()) {
        return value; // Unknown system, return unchanged
    }

    // Apply conversion: displayValue = (rawValue * scale) + offset
    const UnitSpec& spec = systemIt.value();
    return (value * spec.scale) + spec.offset;
}

QString UnitConverter::getUnitLabel(const QString& measurementType) const
{
    if (measurementType.isEmpty()) {
        return QString();
    }

    const QMap<QString, MeasurementTypeInfo>& registry = getMeasurementTypeRegistry();

    auto typeIt = registry.find(measurementType);
    if (typeIt == registry.end()) {
        return QString();
    }

    const MeasurementTypeInfo& typeInfo = typeIt.value();
    auto systemIt = typeInfo.systems.find(m_currentSystem);
    if (systemIt == typeInfo.systems.end()) {
        return QString();
    }

    return systemIt.value().label;
}

int UnitConverter::getPrecision(const QString& measurementType) const
{
    if (measurementType.isEmpty()) {
        return -1;
    }

    const QMap<QString, MeasurementTypeInfo>& registry = getMeasurementTypeRegistry();

    auto typeIt = registry.find(measurementType);
    if (typeIt == registry.end()) {
        return -1;
    }

    const MeasurementTypeInfo& typeInfo = typeIt.value();
    auto systemIt = typeInfo.systems.find(m_currentSystem);
    if (systemIt == typeInfo.systems.end()) {
        return -1;
    }

    return systemIt.value().precision;
}

QString UnitConverter::format(double value, const QString& measurementType) const
{
    // Handle NaN values
    if (std::isnan(value)) {
        return QStringLiteral("--");
    }

    double displayValue = convert(value, measurementType);
    int precision = getPrecision(measurementType);
    QString label = getUnitLabel(measurementType);

    // Use default precision if not found
    if (precision < 0) {
        precision = 2;
    }

    QString formattedValue = QString::number(displayValue, 'f', precision);

    if (label.isEmpty()) {
        return formattedValue;
    }

    return formattedValue + QStringLiteral(" ") + label;
}

QString UnitConverter::currentSystem() const
{
    return m_currentSystem;
}

void UnitConverter::setSystem(const QString& systemName)
{
    if (m_currentSystem != systemName) {
        m_currentSystem = systemName;

        // Update preference
        PreferencesManager::instance().setValue(PreferenceKeys::GeneralUnits, systemName);

        // Emit signal
        emit systemChanged(systemName);
    }
}

QStringList UnitConverter::availableSystems() const
{
    return getAvailableSystems();
}

} // namespace FlySight
