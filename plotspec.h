#ifndef PLOTSPEC_H
#define PLOTSPEC_H

#include <QString>
#include <QColor>

struct PlotSpec {
    QString category;          // Category name
    QString plotName;          // Display name of the plot
    QString plotUnits;         // Units for the y-axis
    QColor defaultColor;       // Default color for the plot
    QString sensorID;          // Sensor name (e.g., "GNSS")
    QString measurementID;     // Measurement name (e.g., "hMSL")
};

#endif // PLOTSPEC_H
