#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent = nullptr);

public slots:
    void setPlotValue(const QModelIndex &selectedIndex);

private slots:
    void updatePlot();

private:
    QCustomPlot *customPlot;
    SessionModel *model;
    QStandardItemModel *plotModel;

    // Store current plot settings
    QString currentSensorID;
    QString currentMeasurementID;
    QString currentPlotName;
    QString currentPlotUnits;
    QColor currentColor;

    void setupPlot();
};

#endif // PLOTWIDGET_H
