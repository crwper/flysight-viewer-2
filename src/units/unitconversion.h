#ifndef UNITCONVERSION_H
#define UNITCONVERSION_H

#include <QHash>
#include <QString>
#include <QVector>
#include <QDebug>

namespace FlySight {

/**
 * @brief Specification for converting a unit to SI.
 *
 * Conversion formula: SI_value = raw_value * scale + offset
 */
struct ConversionSpec {
    double scale;      // raw * scale + offset = SI
    double offset;     // for affine transforms (unused for current units)
    QString siUnit;    // resulting SI unit (for debugging/logging)
};

/**
 * @brief Static class providing unit text to SI conversion lookup and batch conversion.
 *
 * This class uses unit text strings from file headers (e.g., "g", "gauss", "(m/s)")
 * to drive SI normalization during import. Rather than hardcoding sensor/field names,
 * the lookup table maps unit text to conversion factors.
 */
class UnitConversion {
public:
    /**
     * @brief Get the conversion specification for a unit string.
     *
     * @param unitText The unit text from the file header (e.g., "g", "gauss", "(m/s)")
     * @return ConversionSpec with scale, offset, and resulting SI unit.
     *         Returns identity conversion (scale=1, offset=0) for unknown units.
     */
    static ConversionSpec getConversion(const QString& unitText) {
        const auto& table = lookup();
        auto it = table.constFind(unitText);
        if (it != table.constEnd()) {
            return it.value();
        }

        // Unknown unit - log warning and return identity conversion
        if (!unitText.isEmpty()) {
            qWarning() << "UnitConversion: Unknown unit text:" << unitText << "- using identity conversion";
        }
        return {1.0, 0.0, unitText};
    }

    /**
     * @brief Check if a unit requires conversion (not already in SI).
     *
     * This provides a fast path to skip conversion for already-SI units.
     *
     * @param unitText The unit text from the file header
     * @return true if scale != 1.0 or offset != 0.0
     */
    static bool requiresConversion(const QString& unitText) {
        ConversionSpec spec = getConversion(unitText);
        return (spec.scale != 1.0) || (spec.offset != 0.0);
    }

    /**
     * @brief Convert an entire vector of values to SI in-place.
     *
     * This is an optimized batch operation for converting all values in a measurement column.
     *
     * @param values The vector of raw values to convert (modified in-place)
     * @param unitText The unit text from the file header
     */
    static void toSI(QVector<double>& values, const QString& unitText) {
        ConversionSpec spec = getConversion(unitText);

        // Fast path: if no conversion needed, return immediately
        if (spec.scale == 1.0 && spec.offset == 0.0) {
            return;
        }

        // Apply conversion: SI = raw * scale + offset
        for (int i = 0; i < values.size(); ++i) {
            values[i] = values[i] * spec.scale + spec.offset;
        }
    }

private:
    /**
     * @brief Static lookup table mapping unit text strings to conversion specs.
     *
     * This table is initialized once on first access using a lambda.
     */
    static const QHash<QString, ConversionSpec>& lookup() {
        static QHash<QString, ConversionSpec> table = []() {
            QHash<QString, ConversionSpec> t;

            // === Units already in SI (scale=1, offset=0) ===
            t["m"]      = {1.0, 0.0, "m"};
            t["m/s"]    = {1.0, 0.0, "m/s"};
            t["Pa"]     = {1.0, 0.0, "Pa"};
            t["s"]      = {1.0, 0.0, "s"};
            t["deg"]    = {1.0, 0.0, "deg"};
            t["deg/s"]  = {1.0, 0.0, "deg/s"};
            t["V"]      = {1.0, 0.0, "V"};
            t["%"]      = {1.0, 0.0, "%"};
            t[""]       = {1.0, 0.0, ""};           // dimensionless (time, numSV, week)
            t["deg C"]  = {1.0, 0.0, "degC"};       // keep Celsius, don't convert to Kelvin

            // === Units requiring conversion ===
            t["g"]      = {9.80665, 0.0, "m/s^2"};  // acceleration: g -> m/s^2
            t["gauss"]  = {0.0001, 0.0, "T"};       // magnetic field: gauss -> Tesla

            // === Aliases for FS1 parenthesized format ===
            t["(m)"]    = t["m"];
            t["(m/s)"]  = t["m/s"];
            t["(deg)"]  = t["deg"];

            // === Aliases for common variations ===
            t["volt"]    = t["V"];
            t["percent"] = t["%"];

            return t;
        }();
        return table;
    }
};

} // namespace FlySight

#endif // UNITCONVERSION_H
