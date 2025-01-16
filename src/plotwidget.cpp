#include "plotwidget.h"
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPixmap>
#include <QBitmap>
#include <QPainter>
#include <QDebug>

#include "plottool/plottool.h"
#include "plottool/pantool.h"
#include "plottool/zoomtool.h"
#include "plottool/selecttool.h"

namespace FlySight {

PlotWidget::PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
    , isCursorOverPlot(false)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // Initial plot setup
    setupPlot();

    // Setup crosshairs
    setupCrosshairs();

    // Construct plot context
    PlotContext ctx;
    ctx.widget = this;
    ctx.plot = customPlot;
    ctx.graphMap = &m_graphInfoMap;

    // Construct each tool
    m_panTool = std::make_unique<PanTool>(ctx);
    m_zoomTool = std::make_unique<ZoomTool>(ctx);
    m_selectTool = std::make_unique<SelectTool>(ctx);

    // Set default
    m_currentTool = m_panTool.get();

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

bool PlotWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != customPlot || !m_currentTool)
        return QWidget::eventFilter(obj, event);

    switch (event->type())
    {
    case QEvent::MouseMove:
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        // 1) Update crosshairs (in the PlotWidget)
        handleCrosshairMouseMove(mouseEvent);

        // 2) Let the current tool also handle mouse move
        //    (e.g. zoom rectangle, selection rectangle, or panning)
        bool toolConsumed = m_currentTool->mouseMoveEvent(mouseEvent);

        // Return whether the tool says "I fully consumed this"
        // or we might always return false if we want QCustomPlot
        // to see the event as well.
        return toolConsumed;
    }

    case QEvent::MouseButtonPress:
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        // Just delegate to tool (crosshair typically doesn't change on press,
        // but if you did want logic for crosshairs on press, you could put it here too).
        bool toolConsumed = m_currentTool->mousePressEvent(mouseEvent);
        return toolConsumed;
    }

    case QEvent::MouseButtonRelease:
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        // Likewise, let the tool finalize whatever it's doing
        bool toolConsumed = m_currentTool->mouseReleaseEvent(mouseEvent);
        return toolConsumed;
    }

    case QEvent::Leave:
    {
        // We want to hide crosshairs and reset state if the user moves the mouse off the plot
        handleCrosshairLeave(event);

        // The tool might want to know about it (rare, but you could call m_currentTool->leaveEvent)
        // or just do nothing if you prefer.
        m_currentTool->leaveEvent(event);

        // Usually returning false here so QCP can also handle it
        // (though QCP might not do much for Leave).
        return false;
    }

    default:
        // For all other events, fallback
        return QWidget::eventFilter(obj, event);
    }
}

void PlotWidget::handleCrosshairMouseMove(QMouseEvent *mouseEvent)
{
    // 1) Determine if we are over the plot area
    bool currentlyOverPlot = isCursorOverPlotArea(mouseEvent->pos());

    // 2) If we just entered the plot area
    if (currentlyOverPlot && !isCursorOverPlot)
    {
        isCursorOverPlot = true;
        enableCrosshairs(true);
    }
    // 3) If we just left the plot area
    else if (!currentlyOverPlot && isCursorOverPlot)
    {
        isCursorOverPlot = false;
        enableCrosshairs(false);
    }

    // 4) If cursor is within plot, update crosshairs
    if (isCursorOverPlot)
        updateCrosshairs(mouseEvent->pos());
}

void PlotWidget::handleCrosshairLeave(QEvent *event)
{
    Q_UNUSED(event);

    // If we were tracking the cursor inside the plot, disable crosshairs, restore cursor
    if (isCursorOverPlot)
    {
        isCursorOverPlot = false;
        enableCrosshairs(false);
    }
}

void PlotWidget::setupCrosshairs()
{
    // Create horizontal crosshair
    crosshairH = new QCPItemLine(customPlot);
    crosshairH->setPen(QPen(Qt::gray, 1));
    crosshairH->setVisible(false);

    // Create vertical crosshair
    crosshairV = new QCPItemLine(customPlot);
    crosshairV->setPen(QPen(Qt::gray, 1));
    crosshairV->setVisible(false);

    // Optionally do an initial replot
    customPlot->replot();
}

void PlotWidget::enableCrosshairs(bool enable)
{
    crosshairH->setVisible(enable);
    crosshairV->setVisible(enable);

    if (enable) {
        customPlot->setCursor(transparentCursor);
    } else {
        customPlot->setCursor(originalCursor);
    }

    // If you want an immediate update:
    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotWidget::updateCrosshairs(const QPoint &pos)
{
    // Convert pixel position to plot coordinates
    double x = customPlot->xAxis->pixelToCoord(pos.x());
    double y = customPlot->yAxis->pixelToCoord(pos.y());

    // Move horizontal crosshair
    crosshairH->start->setCoords(customPlot->xAxis->range().lower, y);
    crosshairH->end->setCoords(customPlot->xAxis->range().upper,  y);

    // Move vertical crosshair
    crosshairV->start->setCoords(x, customPlot->yAxis->range().lower);
    crosshairV->end->setCoords(x, customPlot->yAxis->range().upper);

    // Redraw
    customPlot->replot(QCustomPlot::rpQueuedReplot);
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

void PlotWidget::updatePlot()
{
    // Clear existing plots and graphs
    customPlot->clearPlottables();
    m_graphInfoMap.clear();

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

                    // Build the GraphInfo struct
                    GraphInfo info;
                    info.sessionId = session.getAttribute(SessionKeys::SessionId).toString();
                    info.sensorId = sensorID;
                    info.measurementId = measurementID;
                    info.defaultPen = graph->pen();

                    // Store in the single map
                    m_graphInfoMap.insert(graph, info);

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

    if (isCursorOverPlot) {
        QPoint cursorPos = customPlot->mapFromGlobal(QCursor::pos());
        updateCrosshairs(cursorPos);
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
    // Iterate through each (graph, info) pair in the map
    for (auto it = m_graphInfoMap.cbegin(); it != m_graphInfoMap.cend(); ++it)
    {
        QCPGraph* graph = it.key();
        const GraphInfo& info = it.value();

        if (sessionId.isEmpty() || info.sessionId == sessionId)
        {
            // Assign to highlighted layer
            graph->setLayer("highlighted");

            // Restore the default pen
            graph->setPen(info.defaultPen);
        }
        else
        {
            // Assign back to main layer
            graph->setLayer("main");

            // Modify the default pen color
            QPen pen = info.defaultPen;
            int h, s, l;
            pen.color().getHsl(&h, &s, &l);

            // Transform to lighten the color
            QColor lighterColor = QColor::fromHsl(h, s, (l + 255 * 15) / 16);
            pen.setColor(lighterColor);

            graph->setPen(pen);
        }
    }

    // Replot to apply the changes
    customPlot->replot();
}

void PlotWidget::handleSessionsSelected(const QList<QString> &sessionIds)
{
    // e.g., do something in your widget (highlight them, or further process).
    qDebug() << "Selected sessions:" << sessionIds;
    emit sessionsSelected(sessionIds); // If you want to re-emit from PlotWidget
}

void PlotWidget::setCurrentTool(Tool tool)
{
    switch(tool)
    {
    case Tool::Pan:
        m_currentTool = m_panTool.get();
        break;
    case Tool::Zoom:
        m_currentTool = m_zoomTool.get();
        break;
    case Tool::Select:
        m_currentTool = m_selectTool.get();
        break;
    }
}

} // namespace FlySight
