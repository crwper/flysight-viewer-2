// plotwidget.h

#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"

namespace FlySight {

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent = nullptr);

public slots:
    void updatePlot();
    void onXAxisRangeChanged(const QCPRange &newRange);
    void setXAxisRange(double min, double max);

private:
    QCustomPlot *customPlot;
    SessionModel *model;
    QStandardItemModel *plotModel;

    // To keep track of plotted graphs to update or clear
    QList<QCPGraph*> m_plottedGraphs;

    // Map to associate plot values with their corresponding y-axes
    QMap<QString, QCPAxis*> m_plotValueAxes;

    // Flag to prevent recursion
    bool m_updatingYAxis = false;

    void setupPlot();

    // Helper function to interpolate Y at a given X
    static double interpolateY(const QCPGraph* graph, double x);
};

} // namespace FlySight

#endif // PLOTWIDGET_H
