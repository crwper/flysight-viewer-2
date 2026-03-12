#ifndef PLOTREGISTRY_H
#define PLOTREGISTRY_H

#include <QString>
#include <QColor>
#include <QVector>

namespace FlySight {

// Role of a measurement in the plotting system
enum class PlotRole {
    Dependent,    // Normal y-axis measurement (elevation, speed, etc.)
    Independent   // x-axis variable (time, distance, etc.)
};

// Plot values
struct PlotValue {
    QString category;          // Category name
    QString plotName;          // Display name of the plot
    QString plotUnits;         // Units for the y-axis
    QColor defaultColor;       // Default color for the plot
    QString sensorID;          // Sensor name (e.g., "GNSS")
    QString measurementID;     // Measurement name (e.g., "hMSL")
    QString measurementType;   // Unit conversion category (e.g., "speed", "altitude")
    PlotRole role = PlotRole::Dependent;
};

class PlotRegistry {
public:
    static PlotRegistry& instance();

    /// register one plot (called by C++ or PluginHost)
    void registerPlot(const PlotValue& pv);

    /// returns all plots (built-in + plugins).  Called by MainWindow.
    QVector<PlotValue> allPlots() const;

    /// returns only dependent (y-axis) plots
    QVector<PlotValue> dependentPlots() const;

    /// returns only independent (x-axis) plots
    QVector<PlotValue> independentPlots() const;

private:
    QVector<PlotValue> m_plots;
};

} // namespace FlySight

#endif // PLOTREGISTRY_H
