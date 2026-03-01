#include "selecttool.h"
#include "ui/docks/plot/PlotWidget.h"
#include "../graphinfo.h"

namespace FlySight {


SelectTool::SelectTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_selectionRect(new QCPItemRect(ctx.plot))
    , m_selecting(false)
{
    m_selectionRect->setVisible(false);
    m_selectionRect->setClipToAxisRect(true);
    m_selectionRect->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    m_selectionRect->setBrush(QColor(100, 100, 255, 50));
}

bool SelectTool::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_selecting = true;
        m_startPixel = event->pos();
        m_selectionRect->setVisible(true);

        // Initialize it to zero size
        QPointF startCoord = pixelToPlotCoords(m_startPixel);
        m_selectionRect->topLeft->setCoords(startCoord.x(), startCoord.y());
        m_selectionRect->bottomRight->setCoords(startCoord.x(), startCoord.y());

        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return true; // we consumed it
    }
    return false;
}

bool SelectTool::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting)
    {
        QPointF startCoord   = pixelToPlotCoords(m_startPixel);
        QPointF currentCoord = pixelToPlotCoords(event->pos());

        double xLeft  = qMin(startCoord.x(), currentCoord.x());
        double xRight = qMax(startCoord.x(), currentCoord.x());
        double yLow   = qMin(startCoord.y(), currentCoord.y());
        double yHigh  = qMax(startCoord.y(), currentCoord.y());

        m_selectionRect->setVisible(true);
        m_selectionRect->topLeft->setCoords(xLeft,  yHigh);
        m_selectionRect->bottomRight->setCoords(xRight, yLow);

        m_plot->replot(QCustomPlot::rpQueuedReplot);
        return true;
    }
    return false;
}

bool SelectTool::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_selecting && event->button() == Qt::LeftButton)
    {
        m_selecting = false;

        // 1) Hide rectangle
        m_selectionRect->setVisible(false);
        m_plot->replot();

        // 2) Calculate final bounding box
        QPointF startCoord = pixelToPlotCoords(m_startPixel);
        QPointF endCoord   = pixelToPlotCoords(event->pos());
        double xMin = qMin(startCoord.x(), endCoord.x());
        double xMax = qMax(startCoord.x(), endCoord.x());
        double yMin = qMin(startCoord.y(), endCoord.y());
        double yMax = qMax(startCoord.y(), endCoord.y());

        // 3) Figure out which sessions lie in the box
        QList<QString> selectedSessions = sessionsInRect(xMin, xMax, yMin, yMax);

        // 4) Select the sessions in the plot widget
        m_widget->handleSessionsSelected(selectedSessions);

        // After setting exit times, revert to the primary tool
        m_widget->revertToPrimaryTool();

        return true;
    }
    return false;
}

// Determine which sessions have points in the specified rectangular region
QList<QString> SelectTool::sessionsInRect(double xMin_main, double xMax_main, double yMin_main, double yMax_main) const
{
    QList<QString> resultSessions;
    QSet<QString> foundSessionIds;

    // Convert the Y boundaries from main axis to pixels (since we know the rectangle in main axes coords)
    double yMin_pixel = m_plot->yAxis->coordToPixel(yMin_main);
    double yMax_pixel = m_plot->yAxis->coordToPixel(yMax_main);

    // Iterate through each (graph, info) pair in the map
    for (auto infoIt = m_graphMap->cbegin(); infoIt != m_graphMap->cend(); ++infoIt)
    {
        QCPGraph* graph = infoIt.key();
        const GraphInfo& info = infoIt.value();

        QString sessionId = info.sessionId;
        if (sessionId.isEmpty())
            continue;

        // If this session is already found, skip checking
        if (foundSessionIds.contains(sessionId))
            continue;

        // Convert the main-axis rectangle to this graph's Y-axis coordinates
        QCPAxis *graphYAxis = graph->valueAxis();
        double yMin_graph = graphYAxis->pixelToCoord(yMin_pixel);
        double yMax_graph = graphYAxis->pixelToCoord(yMax_pixel);

        // Retrieve data
        QSharedPointer<QCPGraphDataContainer> dataContainer = graph->data();
        if (dataContainer->size() == 0)
            continue; // No data to check

        // If only one point, just check if it's inside the rectangle
        if (dataContainer->size() == 1) {
            auto it = dataContainer->constBegin();
            double x = it->key;
            double y = it->value;
            if (x >= xMin_main && x <= xMax_main && y >= yMin_graph && y <= yMax_graph) {
                foundSessionIds.insert(sessionId);
            }
            continue;
        }

        // Multiple points: check line segments
        bool intersected = false;
        auto it = dataContainer->constBegin();
        auto prev = it;
        ++it;
        for (; it != dataContainer->constEnd(); ++it) {
            double x1 = prev->key;
            double y1 = prev->value;
            double x2 = it->key;
            double y2 = it->value;

            // Check if the line segment (x1,y1)-(x2,y2) intersects with the rectangle (xMin_main,xMax_main,yMin_graph,yMax_graph)
            if (lineSegmentIntersectsRect(x1, y1, x2, y2,
                                          xMin_main, xMax_main,
                                          yMin_graph, yMax_graph)) {
                foundSessionIds.insert(sessionId);
                intersected = true;
                break;
            }

            prev = it;
        }

        // If already found intersection, no need to continue
        if (intersected)
            continue;
    }

    resultSessions = foundSessionIds.values();
    return resultSessions;
}

bool SelectTool::lineSegmentIntersectsRect(
    double x1, double y1, double x2, double y2,
    double xMin, double xMax, double yMin, double yMax
    ) const
{
    // Check if either endpoint is inside the rectangle
    if (pointInRect(x1, y1, xMin, xMax, yMin, yMax) || pointInRect(x2, y2, xMin, xMax, yMin, yMax))
        return true;

    // Check intersection with rectangle edges
    // Rectangle edges:
    // Left edge:   x = xMin, y in [yMin, yMax]
    // Right edge:  x = xMax, y in [yMin, yMax]
    // Bottom edge: y = yMin, x in [xMin, xMax]
    // Top edge:    y = yMax, x in [xMin, xMax]

    // Check intersection with left and right edges (vertical lines)
    if (intersectSegmentWithVerticalLine(x1, y1, x2, y2, xMin, yMin, yMax)) return true;
    if (intersectSegmentWithVerticalLine(x1, y1, x2, y2, xMax, yMin, yMax)) return true;

    // Check intersection with top and bottom edges (horizontal lines)
    if (intersectSegmentWithHorizontalLine(x1, y1, x2, y2, yMin, xMin, xMax)) return true;
    if (intersectSegmentWithHorizontalLine(x1, y1, x2, y2, yMax, xMin, xMax)) return true;

    return false;
}

// Check if a point is inside the rectangle
bool SelectTool::pointInRect(
    double x, double y,
    double xMin, double xMax,
    double yMin, double yMax
    ) const {
    return (x >= xMin && x <= xMax && y >= yMin && y <= yMax);
}

// Check intersection with a vertical line: x = lineX, and y in [yMin, yMax]
bool SelectTool::intersectSegmentWithVerticalLine(
    double x1, double y1, double x2, double y2,
    double lineX, double yMin, double yMax
    ) const
{
    // If segment is vertical and x1 != lineX or if lineX not between min and max x of segment, might skip
    if ((x1 < lineX && x2 < lineX) || (x1 > lineX && x2 > lineX))
        return false; // Entire segment is on one side of lineX

    double dx = (x2 - x1);
    if (dx == 0.0) {
        // Vertical segment
        if (x1 == lineX) {
            // They overlap in the vertical direction if the vertical ranges intersect
            double ymin_seg = qMin(y1, y2);
            double ymax_seg = qMax(y1, y2);
            return !(ymax_seg < yMin || ymin_seg > yMax);
        }
        return false;
    }

    double t = (lineX - x1) / dx;
    if (t < 0.0 || t > 1.0)
        return false;

    double yInt = y1 + t * (y2 - y1);
    return (yInt >= yMin && yInt <= yMax);
}

// Check intersection with a horizontal line: y = lineY, and x in [xMin, xMax]
bool SelectTool::intersectSegmentWithHorizontalLine(
    double x1, double y1, double x2, double y2,
    double lineY, double xMin, double xMax
    ) const
{
    // If entire segment above or below lineY
    if ((y1 < lineY && y2 < lineY) || (y1 > lineY && y2 > lineY))
        return false;

    double dy = (y2 - y1);
    if (dy == 0.0) {
        // Horizontal segment
        if (y1 == lineY) {
            // Check horizontal overlap
            double xmin_seg = qMin(x1, x2);
            double xmax_seg = qMax(x1, x2);
            return !(xmax_seg < xMin || xmin_seg > xMax);
        }
        return false;
    }

    double t = (lineY - y1) / dy;
    if (t < 0.0 || t > 1.0)
        return false;

    double xInt = x1 + t * (x2 - x1);
    return (xInt >= xMin && xInt <= xMax);
}

// Helper to convert pixels to plot coordinates
QPointF SelectTool::pixelToPlotCoords(const QPoint &pixel) const
{
    double x = m_plot->xAxis->pixelToCoord(pixel.x());
    double y = m_plot->yAxis->pixelToCoord(pixel.y());
    return QPointF(x, y);
}

} // namespace FlySight
