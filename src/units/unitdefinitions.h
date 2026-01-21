#ifndef UNITDEFINITIONS_H
#define UNITDEFINITIONS_H

#include <QString>
#include <QStringList>
#include <QMap>

namespace FlySight {

/**
 * @brief Holds conversion parameters for a single unit.
 *
 * The conversion formula is: displayValue = (rawValue * scale) + offset
 */
struct UnitSpec {
    QString label;      // Display label (e.g., "m/s", "mph")
    double scale;       // Multiplication factor
    double offset;      // Addition after scaling (for temperature)
    int precision;      // Decimal places for display
};

/**
 * @brief Holds all unit systems for a measurement type.
 */
struct MeasurementTypeInfo {
    QString siBaseUnit;                  // SI unit name (e.g., "m/s")
    QMap<QString, UnitSpec> systems;     // System name -> UnitSpec
};

/**
 * @brief Namespace containing measurement type names as constants.
 */
namespace MeasurementTypes {
    inline const QString Distance = QStringLiteral("distance");
    inline const QString Altitude = QStringLiteral("altitude");
    inline const QString Speed = QStringLiteral("speed");
    inline const QString VerticalSpeed = QStringLiteral("vertical_speed");
    inline const QString Acceleration = QStringLiteral("acceleration");
    inline const QString Temperature = QStringLiteral("temperature");
    inline const QString Pressure = QStringLiteral("pressure");
    inline const QString Rotation = QStringLiteral("rotation");
    inline const QString Angle = QStringLiteral("angle");
    inline const QString MagneticField = QStringLiteral("magnetic_field");
    inline const QString Voltage = QStringLiteral("voltage");
    inline const QString Percentage = QStringLiteral("percentage");
    inline const QString Time = QStringLiteral("time");
    inline const QString Count = QStringLiteral("count");
}

/**
 * @brief Namespace containing unit system names as constants.
 */
namespace UnitSystems {
    inline const QString Metric = QStringLiteral("Metric");
    inline const QString Imperial = QStringLiteral("Imperial");
}

/**
 * @brief Returns the registry of all measurement types with their conversion specifications.
 *
 * This function provides lazy initialization of the measurement type definitions,
 * returning a reference to a static map that is initialized once on first call.
 */
inline const QMap<QString, MeasurementTypeInfo>& getMeasurementTypeRegistry() {
    static const QMap<QString, MeasurementTypeInfo> registry = {
        // Distance: m -> m (metric), ft (imperial)
        {MeasurementTypes::Distance, {
            QStringLiteral("m"),
            {
                {UnitSystems::Metric, {QStringLiteral("m"), 1.0, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("ft"), 3.28084, 0.0, 1}}
            }
        }},

        // Altitude: m -> m (metric), ft (imperial)
        {MeasurementTypes::Altitude, {
            QStringLiteral("m"),
            {
                {UnitSystems::Metric, {QStringLiteral("m"), 1.0, 0.0, 0}},
                {UnitSystems::Imperial, {QStringLiteral("ft"), 3.28084, 0.0, 0}}
            }
        }},

        // Speed: m/s -> m/s (metric), mph (imperial)
        {MeasurementTypes::Speed, {
            QStringLiteral("m/s"),
            {
                {UnitSystems::Metric, {QStringLiteral("km/h"), 3.6, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("mph"), 2.23694, 0.0, 1}}
            }
        }},

        // Vertical Speed: m/s -> m/s (metric), mph (imperial)
        {MeasurementTypes::VerticalSpeed, {
            QStringLiteral("m/s"),
            {
                {UnitSystems::Metric, {QStringLiteral("km/h"), 3.6, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("mph"), 2.23694, 0.0, 1}}
            }
        }},

        // Acceleration: m/s^2 -> g (both systems)
        // 1 g = 9.80665 m/s^2, so scale = 1/9.80665 = 0.101972
        {MeasurementTypes::Acceleration, {
            QStringLiteral("m/s^2"),
            {
                {UnitSystems::Metric, {QStringLiteral("g"), 0.101972, 0.0, 2}},
                {UnitSystems::Imperial, {QStringLiteral("g"), 0.101972, 0.0, 2}}
            }
        }},

        // Temperature: C -> C (metric), F (imperial)
        // F = C * 9/5 + 32 = C * 1.8 + 32
        {MeasurementTypes::Temperature, {
            QStringLiteral("C"),
            {
                {UnitSystems::Metric, {QStringLiteral("°C"), 1.0, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("°F"), 1.8, 32.0, 1}}
            }
        }},

        // Pressure: Pa -> Pa (metric), inHg (imperial)
        // 1 inHg = 3386.39 Pa, so scale = 1/3386.39 = 0.000295300
        {MeasurementTypes::Pressure, {
            QStringLiteral("Pa"),
            {
                {UnitSystems::Metric, {QStringLiteral("kPa"), 0.001, 0.0, 0}},
                {UnitSystems::Imperial, {QStringLiteral("inHg"), 0.000295300, 0.0, 2}}
            }
        }},

        // Rotation: deg/s -> deg/s (both systems)
        {MeasurementTypes::Rotation, {
            QStringLiteral("deg/s"),
            {
                {UnitSystems::Metric, {QStringLiteral("deg/s"), 1.0, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("deg/s"), 1.0, 0.0, 1}}
            }
        }},

        // Angle: deg -> deg (both systems)
        {MeasurementTypes::Angle, {
            QStringLiteral("deg"),
            {
                {UnitSystems::Metric, {QStringLiteral("deg"), 1.0, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("deg"), 1.0, 0.0, 1}}
            }
        }},

        // Magnetic Field: T -> gauss (both systems)
        // 1 T = 10000 gauss
        {MeasurementTypes::MagneticField, {
            QStringLiteral("T"),
            {
                {UnitSystems::Metric, {QStringLiteral("gauss"), 10000.0, 0.0, 4}},
                {UnitSystems::Imperial, {QStringLiteral("gauss"), 10000.0, 0.0, 4}}
            }
        }},

        // Voltage: V -> V (both systems)
        {MeasurementTypes::Voltage, {
            QStringLiteral("V"),
            {
                {UnitSystems::Metric, {QStringLiteral("V"), 1.0, 0.0, 2}},
                {UnitSystems::Imperial, {QStringLiteral("V"), 1.0, 0.0, 2}}
            }
        }},

        // Percentage: % -> % (both systems)
        {MeasurementTypes::Percentage, {
            QStringLiteral("%"),
            {
                {UnitSystems::Metric, {QStringLiteral("%"), 1.0, 0.0, 1}},
                {UnitSystems::Imperial, {QStringLiteral("%"), 1.0, 0.0, 1}}
            }
        }},

        // Time: s -> s (both systems)
        {MeasurementTypes::Time, {
            QStringLiteral("s"),
            {
                {UnitSystems::Metric, {QStringLiteral("s"), 1.0, 0.0, 2}},
                {UnitSystems::Imperial, {QStringLiteral("s"), 1.0, 0.0, 2}}
            }
        }},

        // Count: unitless (both systems)
        {MeasurementTypes::Count, {
            QString(),
            {
                {UnitSystems::Metric, {QString(), 1.0, 0.0, 0}},
                {UnitSystems::Imperial, {QString(), 1.0, 0.0, 0}}
            }
        }}
    };

    return registry;
}

/**
 * @brief Returns a list of all available unit system names.
 */
inline QStringList getAvailableSystems() {
    return {UnitSystems::Metric, UnitSystems::Imperial};
}

} // namespace FlySight

#endif // UNITDEFINITIONS_H
