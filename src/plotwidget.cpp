#include "plotwidget.h"
#include "mainwindow.h"

PlotWidget::PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
    , currentSensorID("")
    , currentMeasurementID("")
    , currentPlotName("")
    , currentPlotUnits("")
    , currentColor(Qt::black) // Default color
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // Initial plot setup
    setupPlot();

    // Connect to model changes
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);
}

void PlotWidget::setPlotValue(const QModelIndex &selectedIndex)
{
    if (!selectedIndex.isValid()) {
        qWarning() << "Invalid QModelIndex received.";
        return;
    }

    // Retrieve data from the model
    QVariant sensorIDVar = plotModel->data(selectedIndex, MainWindow::SensorIDRole);
    QVariant measurementIDVar = plotModel->data(selectedIndex, MainWindow::MeasurementIDRole);
    QVariant colorVar = plotModel->data(selectedIndex, MainWindow::DefaultColorRole);
    QVariant plotUnitsVar = plotModel->data(selectedIndex, MainWindow::PlotUnitsRole);
    QVariant plotNameVar = plotModel->data(selectedIndex, Qt::DisplayRole);

    // Validate and assign data
    if (sensorIDVar.isValid() && measurementIDVar.isValid()) {
        currentSensorID = sensorIDVar.toString();
        currentMeasurementID = measurementIDVar.toString();
    } else {
        qWarning() << "SensorID or MeasurementID is invalid.";
        return;
    }

    if (colorVar.canConvert<QColor>()) {
        currentColor = colorVar.value<QColor>();
    } else {
        qWarning() << "Color data is invalid.";
        currentColor = Qt::black; // Fallback to default
    }

    currentPlotName = plotNameVar.toString();
    currentPlotUnits = plotUnitsVar.toString();

    updatePlot();
}

void PlotWidget::updatePlot()
{
    if (currentSensorID.isEmpty() || currentMeasurementID.isEmpty()) {
        // No plot value selected
        customPlot->clearPlottables();
        customPlot->replot();
        return;
    }

    // Clear existing plots
    customPlot->clearPlottables();

    // Retrieve data from the SessionModel based on currentSensorID and currentMeasurementID
    QVector<double> xData, yData;
    /*
    if (!model->getData(currentSensorID, currentMeasurementID, xData, yData)) {
        qWarning() << "No data available for sensor:" << currentSensorID << "measurement:" << currentMeasurementID;
        return;
    }
    */

    // Create a new graph
    QCPGraph *graph = customPlot->addGraph();
    graph->setName(currentPlotName);
    graph->setPen(QPen(currentColor, 2));

    // Set the data
    graph->setData(xData, yData);

    // Set axis labels with units
    // Assuming x-axis is time; adjust as needed
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setLabel(currentPlotName + " (" + currentPlotUnits + ")");

    // Rescale axes to fit the data
    customPlot->rescaleAxes();

    // Replot to display the updated graph
    customPlot->replot();
}

void PlotWidget::setupPlot()
{
    // Basic plot setup
    customPlot->legend->setVisible(true);
    customPlot->xAxis->setLabel("Time (s)");
    customPlot->yAxis->setLabel("Value");

    // Enable interactions if needed
    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
}
