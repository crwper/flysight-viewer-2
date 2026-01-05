#include "crosshairmanager.h"
#include "graphinfo.h"
#include "plotwidget.h"

#include <QMouseEvent>
#include <QDebug>

namespace FlySight {

CrosshairManager::CrosshairManager(QCustomPlot *plot,
                                   SessionModel *model,
                                   QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                                   QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_model(model)
    , m_graphInfoMap(graphInfoMap)
    , m_enabled(true)
    , m_crosshairH(nullptr)
    , m_crosshairV(nullptr)
    , m_isCursorOverPlot(false)
{
    // Create a fully transparent cursor
    QPixmap pix(16,16);
    pix.fill(Qt::transparent);
    m_transparentCursor = QCursor(pix);

    // Store the original cursor for restoring later
    if (m_plot) {
        m_originalCursor = m_plot->cursor();
    }
}

void CrosshairManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    if (!m_enabled) {
        // hide crosshair lines
        setCrosshairVisible(false);
        // hide all tracers
        hideAllTracers();
        // restore normal cursor if we had changed it
        if (m_plot)
            m_plot->setCursor(m_originalCursor);
        m_isCursorOverPlot = false;

        // Also clear hovered session
        if (m_model)
            m_model->setHoveredSessionId(QString());

        // Clear traced IDs
        m_currentlyTracedSessionIds.clear();
    } else {
        // if enabling, we won't do anything until the next mouseMove
    }
}

void CrosshairManager::handleMouseMove(const QPoint &pixelPos)
{
    if (!m_enabled || !m_plot || !m_graphInfoMap)
        return;

    bool overPlot = isCursorOverPlotArea(pixelPos);
    if (overPlot && !m_isCursorOverPlot) {
        // We just entered the plot area
        m_isCursorOverPlot = true;
        ensureCrosshairCreated();
        setCrosshairVisible(true);
        m_plot->setCursor(m_transparentCursor);
    } else if (!overPlot && m_isCursorOverPlot) {
        // We just left the plot area
        m_isCursorOverPlot = false;
        setCrosshairVisible(false);
        hideAllTracers();
        m_plot->setCursor(m_originalCursor);

        // Clear hovered session
        if (m_model)
            m_model->setHoveredSessionId(QString());

        // Clear traced IDs
        m_currentlyTracedSessionIds.clear();
    }

    // If still over the plot, update crosshair and tracers
    if (m_isCursorOverPlot) {
        updateCrosshairLines(pixelPos);
        updateTracers(pixelPos);
    }
}

void CrosshairManager::handleMouseLeave()
{
    // Called if we get QEvent::Leave
    if (m_isCursorOverPlot) {
        m_isCursorOverPlot = false;
        setCrosshairVisible(false);
        hideAllTracers();
        if (m_plot)
            m_plot->setCursor(m_originalCursor);

        // Clear hovered session
        if (m_model)
            m_model->setHoveredSessionId(QString());

        // Clear traced IDs
        m_currentlyTracedSessionIds.clear();
    }
}

void CrosshairManager::setExternalCursor(const QString &sessionId, double xPlot)
{
    if (!m_enabled || !m_plot || !m_graphInfoMap)
        return;

    if (sessionId.isEmpty()) {
        clearExternalCursor();
        return;
    }

    // Option 2 (matching plot feel): show a vertical crosshair at xPlot (no horizontal line).
    ensureCrosshairCreated();
    if (m_crosshairH)
        m_crosshairH->setVisible(false);

    if (m_crosshairV) {
        const double yLower = m_plot->yAxis->range().lower;
        const double yUpper = m_plot->yAxis->range().upper;
        m_crosshairV->start->setCoords(xPlot, yLower);
        m_crosshairV->end->setCoords(xPlot, yUpper);
        m_crosshairV->setVisible(true);
    }

    // Tracers: only for graphs belonging to this sessionId.
    hideAllTracers();
    m_currentlyTracedSessionIds.clear();

    for (auto it = m_graphInfoMap->begin(); it != m_graphInfoMap->end(); ++it) {
        if (it.value().sessionId != sessionId)
            continue;

        QCPGraph* g = it.key();
        if (!g || !g->visible())
            continue;

        const double yPlot = PlotWidget::interpolateY(g, xPlot);
        if (std::isnan(yPlot))
            continue;

        QCPItemTracer *tr = getOrCreateTracer(g);
        tr->position->setAxes(m_plot->xAxis, g->valueAxis());
        tr->position->setCoords(xPlot, yPlot);
        tr->setPen(it.value().defaultPen);
        tr->setBrush(it.value().defaultPen.color());
        tr->setVisible(true);

        m_currentlyTracedSessionIds.insert(it.value().sessionId);
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void CrosshairManager::clearExternalCursor()
{
    if (!m_plot)
        return;

    hideAllTracers();
    m_currentlyTracedSessionIds.clear();

    if (m_crosshairH)
        m_crosshairH->setVisible(false);
    if (m_crosshairV)
        m_crosshairV->setVisible(false);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void CrosshairManager::updateIfOverPlotArea()
{
    if (!m_isCursorOverPlot || !m_plot)
        return;

    // We replicate the logic from handleMouseMove, except we re-use the global cursor pos
    QPoint pos = m_plot->mapFromGlobal(QCursor::pos());
    if (!isCursorOverPlotArea(pos)) {
        // The user must've left the area
        handleMouseMove(pos);
        return;
    }
    // Otherwise, just update crosshair lines and tracers
    updateCrosshairLines(pos);
    updateTracers(pos);
}

void CrosshairManager::ensureCrosshairCreated()
{
    if (!m_plot)
        return;

    if (!m_crosshairH) {
        m_crosshairH = new QCPItemLine(m_plot);
        m_crosshairH->setPen(QPen(Qt::gray, 1));
        m_crosshairH->setVisible(false);
    }
    if (!m_crosshairV) {
        m_crosshairV = new QCPItemLine(m_plot);
        m_crosshairV->setPen(QPen(Qt::gray, 1));
        m_crosshairV->setVisible(false);
    }
}

void CrosshairManager::setCrosshairVisible(bool visible)
{
    if (m_crosshairH) m_crosshairH->setVisible(visible);
    if (m_crosshairV) m_crosshairV->setVisible(visible);
    if (m_plot)
        m_plot->replot(QCustomPlot::rpQueuedReplot);
}

bool CrosshairManager::isCursorOverPlotArea(const QPoint &pixelPos) const
{
    if (!m_plot)
        return false;
    return m_plot->axisRect()->rect().contains(pixelPos);
}

void CrosshairManager::updateCrosshairLines(const QPoint &pixelPos)
{
    if (!m_crosshairH || !m_crosshairV || !m_plot)
        return;

    // convert pixel coords -> plot coords
    double x = m_plot->xAxis->pixelToCoord(pixelPos.x());
    double y = m_plot->yAxis->pixelToCoord(pixelPos.y());

    // horizontal line across entire x-range at y
    double xLower = m_plot->xAxis->range().lower;
    double xUpper = m_plot->xAxis->range().upper;
    m_crosshairH->start->setCoords(xLower, y);
    m_crosshairH->end->setCoords(xUpper, y);

    // vertical line across entire y-range at x
    double yLower = m_plot->yAxis->range().lower;
    double yUpper = m_plot->yAxis->range().upper;
    m_crosshairV->start->setCoords(x, yLower);
    m_crosshairV->end->setCoords(x, yUpper);
}

void CrosshairManager::updateTracers(const QPoint &pixelPos)
{
    // single-tracer if nearest graph is within threshold
    double bestDist = 999999.0;
    QCPGraph *closest = findClosestGraph(pixelPos, bestDist);
    double xPlot = m_plot->xAxis->pixelToCoord(pixelPos.x());

    // Start Tracer Update
    hideAllTracers();
    m_currentlyTracedSessionIds.clear();

    // We'll assume single-tracer if dist < threshold, else multi-tracer
    if (closest && bestDist < m_pixelThreshold) {
        const auto itClosest = m_graphInfoMap->find(closest);
        if (itClosest != m_graphInfoMap->end()) {
            if (m_model)
                m_model->setHoveredSessionId(itClosest.value().sessionId);
        }

        double xPlot = m_plot->xAxis->pixelToCoord(pixelPos.x());
        hideAllTracers(); // We'll re-show them
        for (auto it = m_graphInfoMap->begin(); it != m_graphInfoMap->end(); ++it) {
            if (it.value().sessionId != m_model->hoveredSessionId())
                continue;
            QCPGraph* g = it.key();
            if (!g || !g->visible())
                continue;
            double yPlot = PlotWidget::interpolateY(g, xPlot);
            if (std::isnan(yPlot))
                continue;
            QCPItemTracer *tr = getOrCreateTracer(g);
            tr->position->setAxes(m_plot->xAxis, g->valueAxis());
            tr->position->setCoords(xPlot, yPlot);
            tr->setPen(it.value().defaultPen);
            tr->setBrush(it.value().defaultPen.color());
            tr->setVisible(true);
            m_currentlyTracedSessionIds.insert(it.value().sessionId);
        }
    } else {
        if (m_model)
            m_model->setHoveredSessionId(QString());

        double xPlot = m_plot->xAxis->pixelToCoord(pixelPos.x());
        hideAllTracers(); // We'll re-show them
        for (auto it = m_graphInfoMap->begin(); it != m_graphInfoMap->end(); ++it) {
            QCPGraph* g = it.key();
            if (!g || !g->visible())
                continue;
            double yPlot = PlotWidget::interpolateY(g, xPlot);
            if (std::isnan(yPlot))
                continue;
            QCPItemTracer *tr = getOrCreateTracer(g);
            tr->position->setAxes(m_plot->xAxis, g->valueAxis());
            tr->position->setCoords(xPlot, yPlot);
            tr->setPen(it.value().defaultPen);
            tr->setBrush(it.value().defaultPen.color());
            tr->setVisible(true);
            m_currentlyTracedSessionIds.insert(it.value().sessionId);
        }
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

QCPItemTracer* CrosshairManager::getOrCreateTracer(QCPGraph *g)
{
    if (m_tracers.contains(g))
        return m_tracers[g];

    QCPItemTracer* tracer = new QCPItemTracer(m_plot);
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(6);
    // We'll set pen/brush each time we show it for a specific graph
    tracer->setInterpolating(false); // we do the interpolation ourselves
    tracer->position->setAxes(m_plot->xAxis, g->valueAxis());
    m_tracers.insert(g, tracer);
    return tracer;
}

void CrosshairManager::hideAllTracers()
{
    for (auto *tr : m_tracers.values()) {
        tr->setVisible(false);
    }
}

void CrosshairManager::hideAllExcept(QCPGraph* keep)
{
    for (auto it = m_tracers.begin(); it != m_tracers.end(); ++it) {
        if (it.key() == keep)
            continue;
        it.value()->setVisible(false);
    }
}

QCPGraph* CrosshairManager::findClosestGraph(const QPoint &pixelPos, double &distOut) const
{
    distOut = 999999.0;
    QCPGraph* closest = nullptr;
    if (!m_plot || !m_graphInfoMap)
        return nullptr;

    // For each graph in the map, call selectTest to measure distance in px
    for (auto it = m_graphInfoMap->begin(); it != m_graphInfoMap->end(); ++it) {
        QCPGraph* graph = it.key();
        if (!graph->visible())
            continue;
        double d = graph->selectTest(pixelPos, false);
        if (d >= 0 && d < distOut) {
            distOut = d;
            closest = graph;
        }
    }
    return closest;
}

QSet<QString> CrosshairManager::getTracedSessionIds() const
{
    return m_currentlyTracedSessionIds;
}

} // namespace FlySight
