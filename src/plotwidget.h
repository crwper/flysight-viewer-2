// plotwidget.h

#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"
#include "calculatedvaluemanager.h"

namespace FlySight {

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    PlotWidget(SessionModel *model, QStandardItemModel *plotModel, CalculatedValueManager* calcManager, QWidget *parent = nullptr);

public slots:
    void updatePlot(); // Updated to remove parameters

private:
    QCustomPlot *customPlot;
    SessionModel *model;
    QStandardItemModel *plotModel;
    CalculatedValueManager* m_calculatedValueManager;

    // To keep track of plotted graphs to update or clear
    QList<QCPGraph*> m_plottedGraphs;

    // Map to associate plot values with their corresponding y-axes
    QMap<QString, QCPAxis*> m_plotValueAxes;

    void setupPlot();
};

} // namespace FlySight

#endif // PLOTWIDGET_H
