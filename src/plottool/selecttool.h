#ifndef SELECTTOOL_H
#define SELECTTOOL_H

#include <QMap>
#include <QPoint>
#include "plottool.h"
#include "../plotwidget.h"
#include "../qcustomplot/qcustomplot.h"

namespace FlySight {

class PlotWidget;

class SelectTool : public PlotTool
{
public:
    explicit SelectTool(const PlotWidget::PlotContext &ctx);
    ~SelectTool() override = default;

    bool mousePressEvent(QMouseEvent *event) override;
    bool mouseMoveEvent(QMouseEvent *event) override;
    bool mouseReleaseEvent(QMouseEvent *event) override;

    // This is a primary tool
    bool isPrimary() override { return false; }

private:
    PlotWidget* m_widget;
    QCustomPlot* m_plot;
    QMap<QCPGraph*, PlotWidget::GraphInfo>* m_graphMap;

    QCPItemRect* m_selectionRect;
    bool m_selecting;
    QPoint m_startPixel;

    QList<QString> sessionsInRect(double xMin, double xMax, double yMin, double yMax) const;
    bool lineSegmentIntersectsRect(double x1, double y1, double x2, double y2,
                                   double xMin, double xMax, double yMin, double yMax) const;
    bool pointInRect(double x, double y, double xMin, double xMax, double yMin, double yMax) const;
    bool intersectSegmentWithVerticalLine(double x1, double y1, double x2, double y2,
                                          double lineX, double yMin, double yMax) const;
    bool intersectSegmentWithHorizontalLine(double x1, double y1, double x2, double y2,
                                            double lineY, double xMin, double xMax) const;

    // Helper to convert pixel -> plot coords
    QPointF pixelToPlotCoords(const QPoint &pixel) const;
};

} // namespace FlySight

#endif // SELECTTOOL_H
