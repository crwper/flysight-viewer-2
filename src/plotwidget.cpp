#include "plotwidget.h"

PlotWidget::PlotWidget(SessionModel *model, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , currentColor(Qt::blue)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // Initial plot setup
    setupPlot();

    // Connect to model changes
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);

    // Initial plot rendering
    updatePlot();
}

void PlotWidget::setPlotColor(const QColor &color)
{
    if (currentColor != color) {
        currentColor = color;
        updatePlot(); // Redraw with the new color
    }
}

void PlotWidget::updatePlot()
{
    // Clear existing plots
    customPlot->clearPlottables();

    // Iterate over model items and plot based on visibility and attributes
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex descriptionIndex = model->index(row, SessionModel::Description);
        bool visible = model->data(descriptionIndex, Qt::CheckStateRole).toInt() == Qt::Checked;
        if (!visible) {
            continue;
        }

        // Retrieve other attributes
        QString description = model->data(descriptionIndex, Qt::DisplayRole).toString();

        // Create a QCPCurve for each shape
        QCPCurve *curve = new QCPCurve(customPlot->xAxis, customPlot->yAxis);
        curve->setName(description);

        // Set the curve's pen to the selected color
        QPen pen(currentColor);
        pen.setWidth(2); // Optional: Set pen width
        curve->setPen(pen);

        // Define the shape based on description
        if (description == "Circle") {
            QVector<double> x, y;
            for (int i = 0; i < 100; ++i) {
                double a = i / 99. * 2 * 3.1415926535;
                x.append(-2.5 + 0.5 * cos(a));
                y.append(0.5 * sin(a));
            }
            curve->setData(x, y);
        } else if (description == "Square") {
            QVector<double> x = { -0.5, 0.5, 0.5, -0.5, -0.5 };
            QVector<double> y = { 0.5, 0.5, -0.5, -0.5, 0.5 };
            curve->setData(x, y);
        } else if (description == "Triangle") {
            QVector<double> x = { 2.5, 3, 2, 2.5 };
            QVector<double> y = { 0.5, -0.5, -0.5, 0.5 };
            curve->setData(x, y);
        }

        // Customize graph appearance based on other attributes if needed
    }

    customPlot->xAxis->setRange(-5, 5);
    customPlot->yAxis->setRange(-5, 5);
    customPlot->replot();
}

void PlotWidget::setupPlot()
{
    // Additional plot setup (axes labels, etc.)
    customPlot->xAxis->setLabel("X Axis");
    customPlot->yAxis->setLabel("Y Axis");
}
