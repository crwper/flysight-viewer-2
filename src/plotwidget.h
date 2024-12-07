#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    PlotWidget(SessionModel *model, QWidget *parent = nullptr);

public slots:
    void setPlotColor(const QColor &color);

private slots:
    void updatePlot();

private:
    QCustomPlot *customPlot;
    SessionModel *model;

    QColor currentColor;

    void setupPlot();
};

#endif // PLOTWIDGET_H
