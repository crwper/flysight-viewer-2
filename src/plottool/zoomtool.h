#ifndef ZOOMTOOL_H
#define ZOOMTOOL_H

#include "plottool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

namespace FlySight {

class ZoomTool : public PlotTool
{
public:
    explicit ZoomTool(const PlotWidget::PlotContext &ctx)
        : m_plot(ctx.plot)
        , m_zoomRect(new QCPItemRect(ctx.plot))
        , m_zooming(false)
    {
        m_zoomRect->setVisible(false);
        m_zoomRect->setPen(QPen(Qt::green, 1, Qt::DashLine));
        m_zoomRect->setBrush(QColor(0, 255, 0, 50));
    }

    bool mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && isCursorOverPlotArea(event->pos()))
        {
            m_zooming = true;
            m_startPixel = event->pos();
            m_zoomRect->setVisible(true);
            initRectAt(m_startPixel);
            return true; // we consumed it
        }
        return false;
    }

    bool mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_zooming)
        {
            updateRect(m_startPixel, event->pos());
            return true;
        }
        return false;
    }

    bool mouseReleaseEvent(QMouseEvent *event) override
    {
        if (m_zooming && event->button() == Qt::LeftButton)
        {
            m_zooming = false;
            finalizeRect(m_startPixel, event->pos());
            m_zoomRect->setVisible(false);
            m_plot->replot();
            return true;
        }
        return false;
    }

    // This is a primary tool
    bool isPrimary() override { return true; }

private:
    QCustomPlot*  m_plot;
    QCPItemRect*  m_zoomRect;
    bool          m_zooming;
    QPoint        m_startPixel;

    void initRectAt(const QPoint &startPixel)
    {
        // Initialize the rectangle, same logic you had in the widget
        QPointF startCoord = pixelToPlotCoords(startPixel);
        double x = startCoord.x();
        double yLow  = m_plot->yAxis->range().lower;
        double yHigh = m_plot->yAxis->range().upper;

        m_zoomRect->topLeft->setCoords(x, yHigh);
        m_zoomRect->bottomRight->setCoords(x, yLow);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }

    void updateRect(const QPoint &start, const QPoint &current)
    {
        QPointF startCoord   = pixelToPlotCoords(start);
        QPointF currentCoord = pixelToPlotCoords(current);

        double xLeft  = qMin(startCoord.x(), currentCoord.x());
        double xRight = qMax(startCoord.x(), currentCoord.x());
        double yLow   = m_plot->yAxis->range().lower;
        double yHigh  = m_plot->yAxis->range().upper;

        m_zoomRect->setVisible(true);
        m_zoomRect->topLeft->setCoords(xLeft,  yHigh);
        m_zoomRect->bottomRight->setCoords(xRight, yLow);

        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }

    void finalizeRect(const QPoint &start, const QPoint &end)
    {
        QPointF startCoord = pixelToPlotCoords(start);
        QPointF endCoord   = pixelToPlotCoords(end);

        double xMin = qMin(startCoord.x(), endCoord.x());
        double xMax = qMax(startCoord.x(), endCoord.x());
        if (qAbs(xMax - xMin) > 1e-9)
        {
            m_plot->xAxis->setRange(xMin, xMax);
            m_plot->replot();
        }
    }

    // If you don't have direct access to your PlotWidget's pixelToPlotCoords,
    // you'd replicate that logic or store a pointer to PlotWidget.
    QPointF pixelToPlotCoords(const QPoint &pixel) const
    {
        double x = m_plot->xAxis->pixelToCoord(pixel.x());
        double y = m_plot->yAxis->pixelToCoord(pixel.y());
        return QPointF(x, y);
    }

    bool isCursorOverPlotArea(const QPoint &pos) const
    {
        return m_plot->axisRect()->rect().contains(pos);
    }
};

} // namespace FlySight

#endif // ZOOMTOOL_H
