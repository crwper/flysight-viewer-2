#include "plotwidget.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPixmap>
#include <QBitmap>
#include <QPainter>
#include <QDebug>

namespace FlySight {

PlotWidget::PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
    , isCursorOverPlot(false)
    , m_selecting(false)
    , m_zooming(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // Initial plot setup
    setupPlot();

    // Setup crosshairs
    setupCrosshairs();

    // Setup selection rectangle
    setupSelectionRectangle();

    // Setup zoom rectangle
    setupZoomRectangle();

    // Install event filter on customPlot to handle mouse events
    customPlot->installEventFilter(this);

    // Create a transparent cursor
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    transparentCursor = QCursor(pixmap);

    // Store the original cursor
    originalCursor = customPlot->cursor();

    // Connect to model changes
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);
    connect(plotModel, &QStandardItemModel::modelReset, this, &PlotWidget::updatePlot);

    // Connect xAxis range changes to the new slot
    connect(
        customPlot->xAxis,
        QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
        this,
        &PlotWidget::onXAxisRangeChanged
        );

    // Connect to hoveredSessionChanged signal
    connect(model, &SessionModel::hoveredSessionChanged, this, &PlotWidget::onHoveredSessionChanged);

    // Initial plot
    updatePlot();

    // Initialize tool
    setCurrentTool(Tool::Pan);
}

void PlotWidget::setupPlot()
{
    // Basic plot setup
    customPlot->xAxis->setLabel("Time from exit (s)");
    customPlot->yAxis->setVisible(false);

    // We'll keep iRangeDrag and iRangeZoom enabled, but handle the buttons ourselves in eventFilter.
    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);
    customPlot->setInteraction(QCP::iSelectPlottables, true);

    customPlot->axisRect()->setRangeDrag(Qt::Horizontal);
    customPlot->axisRect()->setRangeZoom(Qt::Horizontal);

    // Create a new layer for highlighted graphs
    customPlot->addLayer("highlighted", customPlot->layer("main"), QCustomPlot::limAbove);
}

void PlotWidget::setupCrosshairs()
{
    // Initialize horizontal crosshair
    crosshairH = new QCPItemLine(customPlot);
    crosshairH->setPen(QPen(Qt::gray, 1));
    crosshairH->setVisible(false); // Hidden by default

    // Initialize vertical crosshair
    crosshairV = new QCPItemLine(customPlot);
    crosshairV->setPen(QPen(Qt::gray, 1));
    crosshairV->setVisible(false); // Hidden by default

    customPlot->replot();
}

// Setup the selection rectangle
void PlotWidget::setupSelectionRectangle()
{
    m_selectionRect = new QCPItemRect(customPlot);
    m_selectionRect->setVisible(false);
    m_selectionRect->setPen(QPen(Qt::blue, 1, Qt::DashLine));
    m_selectionRect->setBrush(QColor(100, 100, 255, 50)); // semi-transparent
}

void PlotWidget::setupZoomRectangle()
{
    m_zoomRect = new QCPItemRect(customPlot);
    m_zoomRect->setVisible(false);
    m_zoomRect->setPen(QPen(Qt::green, 1, Qt::DashLine));
    m_zoomRect->setBrush(QColor(0, 255, 0, 50));
}

bool PlotWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == customPlot)
    {
        switch (event->type())
        {
        case QEvent::Wheel:
        {
            // Always let QCP handle wheel for zoom.
            return false;
        }

        case QEvent::MouseButtonPress:
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            // Middle button => always let QCP handle range drag:
            if (mouseEvent->button() == Qt::MiddleButton)
                return false;

            if (mouseEvent->button() == Qt::LeftButton)
            {
                if (currentTool == Tool::Pan)
                {
                    // Manually do a "fake" drag if we want left-button to pan.
                    // But simpler: just let QCP do a left-drag if iRangeDrag is on.
                    // So we return false => QCP sees left button press.
                    return false;
                }
                else if (currentTool == Tool::Zoom)
                {
                    if (isCursorOverPlotArea(mouseEvent->pos()))
                    {
                        m_zooming = true;
                        m_zoomStartPixel = mouseEvent->pos();
                        m_zoomRect->setVisible(true);
                        // Initialize rect
                        QPointF startCoord = pixelToPlotCoords(m_zoomStartPixel);
                        double x = startCoord.x();
                        double y = customPlot->yAxis->range().lower;
                        double y2 = customPlot->yAxis->range().upper;
                        // We'll only highlight horizontally for time zoom
                        m_zoomRect->topLeft->setCoords(x, y2);
                        m_zoomRect->bottomRight->setCoords(x, y);
                        customPlot->replot(QCustomPlot::rpQueuedReplot);
                    }
                    return true;
                }
                else if (currentTool == Tool::Select)
                {
                    if (isCursorOverPlotArea(mouseEvent->pos()))
                    {
                        m_selecting = true;
                        m_selectionStartPixel = mouseEvent->pos();
                        m_selectionRect->setVisible(true);
                        updateSelectionRect(m_selectionStartPixel);
                    }
                    return true;
                }
            }
            break;
        }

        case QEvent::MouseMove:
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint pos = mouseEvent->pos();

            bool currentlyOverPlot = isCursorOverPlotArea(pos);
            if (currentlyOverPlot && !isCursorOverPlot)
            {
                isCursorOverPlot = true;
                customPlot->setCursor(transparentCursor);
                crosshairH->setVisible(true);
                crosshairV->setVisible(true);
                customPlot->replot();
            }
            else if (!currentlyOverPlot && isCursorOverPlot)
            {
                isCursorOverPlot = false;
                customPlot->setCursor(originalCursor);
                crosshairH->setVisible(false);
                crosshairV->setVisible(false);
                customPlot->replot();
            }

            if (isCursorOverPlot)
                updateCrosshairs(pos);

            // Zoom dragging
            if (m_zooming && currentTool == Tool::Zoom)
            {
                QPointF startCoord = pixelToPlotCoords(m_zoomStartPixel);
                QPointF currentCoord = pixelToPlotCoords(pos);
                double xLeft = qMin(startCoord.x(), currentCoord.x());
                double xRight = qMax(startCoord.x(), currentCoord.x());
                double yLow = customPlot->yAxis->range().lower;
                double yHigh = customPlot->yAxis->range().upper;

                m_zoomRect->setVisible(true);
                m_zoomRect->topLeft->setCoords(xLeft, yHigh);
                m_zoomRect->bottomRight->setCoords(xRight, yLow);
                customPlot->replot(QCustomPlot::rpQueuedReplot);
            }

            // Selecting
            if (m_selecting && currentTool == Tool::Select)
            {
                updateSelectionRect(pos);
            }
            return false;
        }

        case QEvent::MouseButtonRelease:
        {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

            // Middle => let QCP handle
            if (mouseEvent->button() == Qt::MiddleButton)
                return false;

            if (mouseEvent->button() == Qt::LeftButton)
            {
                if (currentTool == Tool::Zoom && m_zooming)
                {
                    m_zooming = false;
                    m_zoomRect->setVisible(false);
                    customPlot->replot();

                    QPointF startCoord = pixelToPlotCoords(m_zoomStartPixel);
                    QPointF endCoord   = pixelToPlotCoords(mouseEvent->pos());
                    double xMin = qMin(startCoord.x(), endCoord.x());
                    double xMax = qMax(startCoord.x(), endCoord.x());
                    if (qAbs(xMax - xMin) > 1e-9) // do actual zoom
                    {
                        customPlot->xAxis->setRange(xMin, xMax);
                        customPlot->replot();
                    }
                    return true;
                }
                else if (currentTool == Tool::Select && m_selecting)
                {
                    m_selecting = false;
                    finalizeSelectionRect(mouseEvent->pos());
                    return true;
                }
            }
            break;
        }

        case QEvent::Leave:
        {
            if (isCursorOverPlot)
            {
                isCursorOverPlot = false;
                customPlot->setCursor(originalCursor);
                crosshairH->setVisible(false);
                crosshairV->setVisible(false);
                customPlot->replot();
            }
            break;
        }

        default:
            break;
        }
    }
    // Standard event processing
    return QWidget::eventFilter(obj, event);
}

bool PlotWidget::isCursorOverPlotArea(const QPoint &pos) const
{
    // Get the axis rect
    QCPAxisRect *axisRect = customPlot->axisRect();

    // Get the plotting area rectangle in pixel coordinates
    QRect plotAreaRect = axisRect->rect();

    // Check if the position is within the axis rect
    return plotAreaRect.contains(pos);
}

void PlotWidget::updateCrosshairs(const QPoint &pos)
{
    // Convert pixel position to plot coordinates
    double x = customPlot->xAxis->pixelToCoord(pos.x());
    double y = customPlot->yAxis->pixelToCoord(pos.y());

    // Set the positions of the crosshairs
    crosshairH->start->setCoords(customPlot->xAxis->range().lower, y);
    crosshairH->end->setCoords(customPlot->xAxis->range().upper, y);

    crosshairV->start->setCoords(x, customPlot->yAxis->range().lower);
    crosshairV->end->setCoords(x, customPlot->yAxis->range().upper);

    // Update plot without triggering a full replot
    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

// Update the selection rectangle as the mouse moves
void PlotWidget::updateSelectionRect(const QPoint &currentPos)
{
    QPointF startCoord = pixelToPlotCoords(m_selectionStartPixel);
    QPointF currentCoord = pixelToPlotCoords(currentPos);

    // Update the rectangle corners
    m_selectionRect->topLeft->setCoords(qMin(startCoord.x(), currentCoord.x()), qMax(startCoord.y(), currentCoord.y()));
    m_selectionRect->bottomRight->setCoords(qMax(startCoord.x(), currentCoord.x()), qMin(startCoord.y(), currentCoord.y()));

    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

// Finalize the selection and determine which sessions are selected
void PlotWidget::finalizeSelectionRect(const QPoint &endPos)
{
    // Get the final coordinates of the selection
    QPointF startCoord = pixelToPlotCoords(m_selectionStartPixel);
    QPointF endCoord = pixelToPlotCoords(endPos);

    double xMin = qMin(startCoord.x(), endCoord.x());
    double xMax = qMax(startCoord.x(), endCoord.x());
    double yMin = qMin(startCoord.y(), endCoord.y());
    double yMax = qMax(startCoord.y(), endCoord.y());

    // Hide the rectangle after selection is done
    m_selectionRect->setVisible(false);
    customPlot->replot();

    // Find sessions that intersect this region
    QList<QString> selectedSessions = sessionsInRect(xMin, xMax, yMin, yMax);

    // Emit signal with selected sessions
    emit sessionsSelected(selectedSessions);
}

// Helper to convert pixels to plot coordinates
QPointF PlotWidget::pixelToPlotCoords(const QPoint &pixel) const
{
    double x = customPlot->xAxis->pixelToCoord(pixel.x());
    double y = customPlot->yAxis->pixelToCoord(pixel.y());
    return QPointF(x, y);
}

// Determine which sessions have points in the specified rectangular region
QList<QString> PlotWidget::sessionsInRect(double xMin_main, double xMax_main, double yMin_main, double yMax_main) const
{
    QList<QString> resultSessions;
    QSet<QString> foundSessionIds;

    // Convert the Y boundaries from main axis to pixels (since we know the rectangle in main axes coords)
    double yMin_pixel = customPlot->yAxis->coordToPixel(yMin_main);
    double yMax_pixel = customPlot->yAxis->coordToPixel(yMax_main);

    for (auto graph : m_plottedGraphs) {
        QString sessionId = m_graphToSessionMap.value(graph, QString());
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

bool PlotWidget::lineSegmentIntersectsRect(
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
bool PlotWidget::pointInRect(
    double x, double y,
    double xMin, double xMax,
    double yMin, double yMax
    ) const {
    return (x >= xMin && x <= xMax && y >= yMin && y <= yMax);
}

// Check intersection with a vertical line: x = lineX, and y in [yMin, yMax]
bool PlotWidget::intersectSegmentWithVerticalLine(
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
bool PlotWidget::intersectSegmentWithHorizontalLine(
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

void PlotWidget::updatePlot()
{
    // Clear existing plots and graphs
    customPlot->clearPlottables();
    m_plottedGraphs.clear();
    m_graphToSessionMap.clear();
    m_graphDefaultPens.clear();

    // Remove existing custom y-axes
    QList<QCPAxis*> axesToRemove = m_plotValueAxes.values();
    m_plotValueAxes.clear();

    for(auto axis : axesToRemove){
        customPlot->axisRect()->removeAxis(axis);
    }

    // Retrieve all sessions
    const QVector<SessionData>& sessions = model->getAllSessions();

    // Iterate through all checked plot values
    for(int row = 0; row < plotModel->rowCount(); ++row){
        QStandardItem* categoryItem = plotModel->item(row);
        for(int col = 0; col < categoryItem->rowCount(); ++col){
            QStandardItem* plotItem = categoryItem->child(col);
            if(plotItem->checkState() == Qt::Checked){
                // Retrieve plot specifications
                QColor color = plotItem->data(MainWindow::DefaultColorRole).value<QColor>();
                QString sensorID = plotItem->data(MainWindow::SensorIDRole).toString();
                QString measurementID = plotItem->data(MainWindow::MeasurementIDRole).toString();
                QString plotName = plotItem->data(Qt::DisplayRole).toString();
                QString plotUnits = plotItem->data(MainWindow::PlotUnitsRole).toString();

                // Unique identifier for plot value
                QString plotValueID = sensorID + "/" + measurementID;

                // Create y-axis for this plot value if not already created
                if(!m_plotValueAxes.contains(plotValueID)){
                    // Create a new y-axis
                    QCPAxis *newYAxis = customPlot->axisRect()->addAxis(QCPAxis::atLeft);

                    // Set axis label
                    if (!plotUnits.isEmpty()) {
                        newYAxis->setLabel(plotName + " (" + plotUnits + ")");
                    } else {
                        newYAxis->setLabel(plotName);
                    }

                    // Set axis color
                    newYAxis->setLabelColor(color);
                    newYAxis->setTickLabelColor(color);
                    newYAxis->setBasePen(QPen(color));
                    newYAxis->setTickPen(QPen(color));
                    newYAxis->setSubTickPen(QPen(color));

                    // Add to the map
                    m_plotValueAxes.insert(plotValueID, newYAxis);
                }

                QCPAxis* assignedYAxis = m_plotValueAxes.value(plotValueID);

                // Iterate through all sessions to plot
                for(const auto& session : sessions){
                    // Check for session visibility
                    if (!session.isVisible()) {
                        continue;
                    }

                    // Get yData directly from session
                    QVector<double> yData = const_cast<SessionData&>(session).getMeasurement(sensorID, measurementID);

                    if(yData.isEmpty()){
                        qWarning() << "No data available for plot:" << plotName << "in session:" << session.getAttribute(SessionKeys::SessionId);
                        continue;
                    }

                    // Assume there is a "time" measurement for x-axis
                    QVector<double> xData = const_cast<SessionData&>(session).getMeasurement(sensorID, SessionKeys::TimeFromExit);

                    if(xData.isEmpty()){
                        qWarning() << "No 'time' data available for session:" << session.getAttribute(SessionKeys::SessionId);
                        continue;
                    }

                    if(xData.size() != yData.size()){
                        qWarning() << "Time and measurement data size mismatch for session:" << session.getAttribute(SessionKeys::SessionId);
                        continue;
                    }

                    // Create a new graph assigned to the specific y-axis
                    QCPGraph *graph = customPlot->addGraph(customPlot->xAxis, assignedYAxis);

                    // Assign the default color
                    graph->setPen(QPen(color));

                    // Set data
                    graph->setData(xData, yData);

                    // Set line style, scatter style, etc.
                    graph->setLineStyle(QCPGraph::lsLine);
                    graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

                    // Assign to default layer
                    graph->setLayer("main");

                    // Add to the list of plotted graphs
                    m_plottedGraphs.append(graph);

                    // Map graph to session ID
                    QString sessionId = session.getAttribute(SessionKeys::SessionId).toString();
                    m_graphToSessionMap.insert(graph, sessionId);

                    // Store the default pen for later use
                    m_graphDefaultPens.insert(graph, graph->pen());

                    qDebug() << "Plotted session:" << session.getAttribute(SessionKeys::SessionId) << "on plot:" << plotName;
                }
            }
        }
    }

    // Replot to display the updated graph
    onXAxisRangeChanged(customPlot->xAxis->range());
}

void PlotWidget::onXAxisRangeChanged(const QCPRange &newRange)
{
    if (m_updatingYAxis)
        return; // Prevent recursion

    m_updatingYAxis = true;

    for(auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it){
        QCPAxis* yAxis = it.value();

        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();

        // Iterate through all graphs and find those assigned to this yAxis
        for(int i = 0; i < customPlot->graphCount(); ++i){
            QCPGraph* graph = customPlot->graph(i);
            if(graph->valueAxis() != yAxis) continue;

            if(graph->valueAxis() != yAxis){
                continue;
            }

            // Efficiently find data within the new x range
            QCPDataContainer<QCPGraphData>::const_iterator itLower = graph->data()->findBegin(newRange.lower, false);
            QCPDataContainer<QCPGraphData>::const_iterator itUpper = graph->data()->findEnd(newRange.upper, false);

            for(auto it = itLower; it != itUpper; ++it){
                double y = it->value;
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
            }

            // Interpolate at newRange.lower
            double yLower = interpolateY(graph, newRange.lower);
            if (!std::isnan(yLower)) {
                yMin = std::min(yMin, yLower);
                yMax = std::max(yMax, yLower);
            }

            // Interpolate at newRange.upper
            double yUpper = interpolateY(graph, newRange.upper);
            if (!std::isnan(yUpper)) {
                yMin = std::min(yMin, yUpper);
                yMax = std::max(yMax, yUpper);
            }
        }

        if(yMin < yMax){
            // Add 5% padding to the y-axis range for better visualization
            double padding = (yMax - yMin) * 0.05;
            if(padding == 0){
                padding = 1.0; // Fallback padding
            }

            yAxis->setRange(yMin - padding, yMax + padding);
        }
    }

    customPlot->replot();

    m_updatingYAxis = false;
}

double PlotWidget::interpolateY(const QCPGraph* graph, double x) {
    auto itLower = graph->data()->findBegin(x, false);
    if (itLower == graph->data()->constBegin() || itLower == graph->data()->constEnd()) {
        return std::numeric_limits<double>::quiet_NaN(); // Cannot interpolate
    }

    auto itPrev = itLower;
    --itPrev;

    double x1 = itPrev->key;
    double y1 = itPrev->value;
    double x2 = itLower->key;
    double y2 = itLower->value;

    if (x2 == x1) {
        return std::numeric_limits<double>::quiet_NaN(); // Avoid division by zero
    }

    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

void PlotWidget::setXAxisRange(double min, double max)
{
    // Directly set the x-axis range
    customPlot->xAxis->setRange(min, max);
}

void PlotWidget::onHoveredSessionChanged(const QString& sessionId)
{
    // Iterate through all plotted graphs
    for(auto graph : m_plottedGraphs){
        QString graphSessionId = m_graphToSessionMap.value(graph, QString());

        if(graphSessionId == sessionId || sessionId.isEmpty()){
            // Assign to highlighted layer
            graph->setLayer("highlighted");

            if(m_graphDefaultPens.contains(graph)){
                QPen highlightPen = m_graphDefaultPens.value(graph);
                graph->setPen(highlightPen);
            }
        }
        else{
            // Assign back to main layer
            graph->setLayer("main");

            if(m_graphDefaultPens.contains(graph)){
                QPen highlightPen = m_graphDefaultPens.value(graph);

                int h, s, l;
                highlightPen.color().getHsl(&h, &s, &l);
                QColor color = QColor::fromHsl(h, s, (l + 255 * 15) / 16);

                highlightPen.setColor(color);
                graph->setPen(highlightPen);
            }
        }
    }

    // Replot to apply the changes
    customPlot->replot();
}

void PlotWidget::setCurrentTool(Tool tool)
{
    currentTool = tool;
    switch (currentTool) {
    case Tool::Pan:
        // Just let left mouse press pass to QCP for drag. Middle also drags, wheel => zoom
        break;
    case Tool::Zoom:
        // We'll do a custom left-drag rectangle => zoom
        break;
    case Tool::Select:
        // We'll do a custom left-drag rectangle => selection
        break;
    }
}

} // namespace FlySight
