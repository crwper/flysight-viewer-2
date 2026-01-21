#ifndef UNITCONVERTER_H
#define UNITCONVERTER_H

#include <QObject>
#include <QString>
#include <QStringList>

namespace FlySight {

/**
 * @brief Singleton class that performs unit conversions based on the current unit system.
 *
 * This class provides methods to convert SI-stored values to display units,
 * get unit labels, formatting precision, and formatted strings.
 * It emits a signal when the unit system changes.
 */
class UnitConverter : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Get the singleton instance of UnitConverter.
     * @return Reference to the singleton instance.
     */
    static UnitConverter& instance() {
        static UnitConverter instance;
        return instance;
    }

    /**
     * @brief Convert a value from SI units to display units.
     * @param value The value in SI units.
     * @param measurementType The measurement type (e.g., "speed", "altitude").
     * @return The converted value in display units.
     *
     * If measurementType is unknown or empty, returns the value unchanged.
     */
    double convert(double value, const QString& measurementType) const;

    /**
     * @brief Get the unit label for the current system.
     * @param measurementType The measurement type.
     * @return The unit label (e.g., "m/s", "mph"), or empty string if not found.
     */
    QString getUnitLabel(const QString& measurementType) const;

    /**
     * @brief Get the decimal precision for the current system.
     * @param measurementType The measurement type.
     * @return The number of decimal places, or -1 if not found.
     */
    int getPrecision(const QString& measurementType) const;

    /**
     * @brief Format a value with conversion and unit label.
     * @param value The value in SI units.
     * @param measurementType The measurement type.
     * @return Formatted string (e.g., "45.5 mph"), or "--" for NaN values.
     */
    QString format(double value, const QString& measurementType) const;

    /**
     * @brief Get the currently active unit system name.
     * @return The current system name (e.g., "Metric", "Imperial").
     */
    QString currentSystem() const;

    /**
     * @brief Set the active unit system.
     * @param systemName The system name to activate.
     *
     * This updates the preference and emits systemChanged if the value changes.
     */
    void setSystem(const QString& systemName);

    /**
     * @brief Get the list of available unit system names.
     * @return List containing all registered system names.
     */
    QStringList availableSystems() const;

signals:
    /**
     * @brief Emitted when the unit system changes.
     * @param systemName The new system name.
     */
    void systemChanged(const QString& systemName);

private:
    UnitConverter();
    ~UnitConverter() override = default;

    QString m_currentSystem;

    Q_DISABLE_COPY(UnitConverter)
};

} // namespace FlySight

#endif // UNITCONVERTER_H
