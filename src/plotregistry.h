#ifndef PLOTREGISTRY_H
#define PLOTREGISTRY_H

#include <QString>
#include <QColor>
#include <QVector>

namespace FlySight {

// Plot values
struct PlotValue {
    QString category;          // Category name
    QString plotName;          // Display name of the plot
    QString plotUnits;         // Units for the y-axis
    QColor defaultColor;       // Default color for the plot
    QString sensorID;          // Sensor name (e.g., "GNSS")
    QString measurementID;     // Measurement name (e.g., "hMSL")
};

class PlotRegistry {
public:
    static PlotRegistry& instance();

    /// register one plot (called by C++ or PluginHost)
    void registerPlot(const PlotValue& pv);

    /// returns all plots (built-in + plugins).  Called by MainWindow.
    QVector<PlotValue> allPlots() const;

private:
    QVector<PlotValue> m_plots;
};

} // namespace FlySight

#endif // PLOTREGISTRY_H
