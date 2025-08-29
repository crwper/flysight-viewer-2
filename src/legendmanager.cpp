#include "legendmanager.h"
#include "sessionmodel.h"
#include "plotwidget.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

namespace FlySight {

LegendManager::LegendManager(QCustomPlot *plot, SessionModel *model,
                             QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                             QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_model(model)
    , m_graphInfoMap(graphInfoMap)
    , m_legendLayout(nullptr)
    , m_backgroundRect(nullptr)
    , m_visible(false)
    , m_mode(PointDataMode)
{
    createLegendStructure();
}

LegendManager::~LegendManager()
{
    clearLegendElements();
    if (m_backgroundRect) {
        m_plot->removeItem(m_backgroundRect);
    }
}

void LegendManager::createLegendStructure()
{
    if (!m_plot || !m_plot->axisRect())
        return;

    // Create background rectangle in a layer above the main plot but below the axes
    m_backgroundRect = new QCPItemRect(m_plot);
    m_backgroundRect->setPen(QPen(QColor(100, 100, 100), 1));
    m_backgroundRect->setBrush(QBrush(QColor(255, 255, 255)));

    // Important: set the clipping to axis rect so it doesn't extend beyond
    m_backgroundRect->setClipToAxisRect(true);
    m_backgroundRect->setClipAxisRect(m_plot->axisRect());
    m_backgroundRect->setVisible(false);

    // Create layout grid in the inset layout of axis rect
    QCPLayoutInset *insetLayout = m_plot->axisRect()->insetLayout();
    m_legendLayout = new QCPLayoutGrid();
    m_legendLayout->setMargins(QMargins(5, 5, 5, 5));
    m_legendLayout->setRowSpacing(2);
    m_legendLayout->setColumnSpacing(10);

    // Add to inset layout at top-right
    insetLayout->addElement(m_legendLayout, Qt::AlignTop | Qt::AlignRight);

    m_legendLayout->setVisible(false);
}

void LegendManager::setVisible(bool visible)
{
    m_visible = visible;

    if (m_legendLayout)
        m_legendLayout->setVisible(visible);

    if (visible) {
        updateLegendPosition();
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void LegendManager::setMode(Mode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        rebuildLegend();
    }
}

void LegendManager::rebuildLegend()
{
    clearLegendElements();
    collectVisibleSeries();

    if (m_visibleSeries.isEmpty()) {
        setVisible(false);
        return;
    }

    // Create header row
    m_headerElements.clear();

    if (m_mode == PointDataMode) {
        // Headers: Series | Value
        auto* seriesHeader = new QCPTextElement(m_plot, "Series", QFont("Arial", 9, QFont::Bold));
        auto* valueHeader = new QCPTextElement(m_plot, "Value", QFont("Arial", 9, QFont::Bold));
        m_legendLayout->addElement(0, 0, seriesHeader);
        m_legendLayout->addElement(0, 1, valueHeader);
        m_headerElements << seriesHeader << valueHeader;
    } else {
        // Headers: Series | Value | Change | Min | Avg | Max
        QStringList headers = {"Series", "Value", "Change", "Min", "Avg", "Max"};
        for (int i = 0; i < headers.size(); ++i) {
            auto* header = new QCPTextElement(m_plot, headers[i], QFont("Arial", 9, QFont::Bold));
            m_legendLayout->addElement(0, i, header);
            m_headerElements << header;
        }
    }

    // Create data rows for each series
    m_dataElements.clear();
    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        QVector<QCPTextElement*> row;
        int colCount = (m_mode == PointDataMode) ? 2 : 6;

        for (int j = 0; j < colCount; ++j) {
            auto* elem = new QCPTextElement(m_plot, "", QFont("Arial", 8));
            m_legendLayout->addElement(i + 1, j, elem);
            row << elem;
        }

        // Set series name and color
        row[0]->setText(m_visibleSeries[i].name);
        row[0]->setTextColor(m_visibleSeries[i].color);

        m_dataElements << row;
    }

    updateLegendPosition();
}

void LegendManager::collectVisibleSeries()
{
    m_visibleSeries.clear();

    if (!m_graphInfoMap)
        return;

    // Group by measurement type to avoid duplicates
    QMap<QString, SeriesInfo> seriesMap;

    for (auto it = m_graphInfoMap->constBegin(); it != m_graphInfoMap->constEnd(); ++it) {
        QCPGraph* graph = it.key();
        const GraphInfo& info = it.value();

        if (!graph || !graph->visible())
            continue;

        QString key = info.sensorId + "/" + info.measurementId;
        if (!seriesMap.contains(key)) {
            SeriesInfo si;
            si.name = info.measurementId;
            si.color = info.defaultPen.color();
            si.sensorId = info.sensorId;
            si.measurementId = info.measurementId;
            si.graph = graph; // Store first graph for this measurement type
            seriesMap.insert(key, si);
        }
    }

    m_visibleSeries = seriesMap.values().toVector();
}

void LegendManager::updatePointData(double xCoord, const QString& hoveredSessionId)
{
    if (!m_visible || m_mode != PointDataMode)
        return;

    clearDataRows();

    if (!hoveredSessionId.isEmpty()) {
        // Single track mode - show values for hovered session only
        for (int i = 0; i < m_visibleSeries.size(); ++i) {
            // Find the graph for this series and hovered session
            QCPGraph* targetGraph = nullptr;
            for (auto it = m_graphInfoMap->constBegin(); it != m_graphInfoMap->constEnd(); ++it) {
                const GraphInfo& info = it.value();
                if (info.sessionId == hoveredSessionId &&
                    info.sensorId == m_visibleSeries[i].sensorId &&
                    info.measurementId == m_visibleSeries[i].measurementId) {
                    targetGraph = it.key();
                    break;
                }
            }

            if (targetGraph) {
                double value = interpolateValueAtX(targetGraph, xCoord);
                setElementText(i + 1, 1, formatValue(value, m_visibleSeries[i].measurementId));
            } else {
                setElementText(i + 1, 1, "--");
            }
        }
    } else {
        // Multi-track mode - show aggregate stats from x-axis start to cursor
        double xStart = m_plot->xAxis->range().lower;

        for (int i = 0; i < m_visibleSeries.size(); ++i) {
            double minVal = std::numeric_limits<double>::max();
            double maxVal = std::numeric_limits<double>::lowest();
            double sumVal = 0;
            int count = 0;

            // Calculate stats across all graphs of this measurement type
            for (auto it = m_graphInfoMap->constBegin(); it != m_graphInfoMap->constEnd(); ++it) {
                QCPGraph* graph = it.key();
                const GraphInfo& info = it.value();

                if (info.sensorId == m_visibleSeries[i].sensorId &&
                    info.measurementId == m_visibleSeries[i].measurementId &&
                    graph->visible()) {

                    double min, avg, max;
                    calculateRangeStats(graph, xStart, xCoord, min, avg, max);
                    if (!std::isnan(min)) {
                        minVal = std::min(minVal, min);
                        maxVal = std::max(maxVal, max);
                        sumVal += avg;
                        count++;
                    }
                }
            }

            if (count > 0) {
                QString statText = QString("Min:%1 Avg:%2 Max:%3")
                .arg(formatValue(minVal, m_visibleSeries[i].measurementId))
                    .arg(formatValue(sumVal/count, m_visibleSeries[i].measurementId))
                    .arg(formatValue(maxVal, m_visibleSeries[i].measurementId));
                setElementText(i + 1, 1, statText);
            } else {
                setElementText(i + 1, 1, "--");
            }
        }
    }

    updateLegendPosition();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void LegendManager::updateRangeStats(double xStart, double xEnd)
{
    if (!m_visible || m_mode != RangeStatsMode)
        return;

    clearDataRows();

    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        // Use first available graph for this measurement type
        QCPGraph* graph = m_visibleSeries[i].graph;
        if (!graph)
            continue;

        double startVal = interpolateValueAtX(graph, xStart);
        double endVal = interpolateValueAtX(graph, xEnd);
        double change = endVal - startVal;

        double minVal, avgVal, maxVal;
        calculateRangeStats(graph, xStart, xEnd, minVal, avgVal, maxVal);

        // Set values in columns: Series | Value | Change | Min | Avg | Max
        setElementText(i + 1, 1, formatValue(endVal, m_visibleSeries[i].measurementId));
        setElementText(i + 1, 2, formatValue(change, m_visibleSeries[i].measurementId));
        setElementText(i + 1, 3, formatValue(minVal, m_visibleSeries[i].measurementId));
        setElementText(i + 1, 4, formatValue(avgVal, m_visibleSeries[i].measurementId));
        setElementText(i + 1, 5, formatValue(maxVal, m_visibleSeries[i].measurementId));
    }

    updateLegendPosition();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

double LegendManager::interpolateValueAtX(QCPGraph* graph, double x) const
{
    if (!graph || graph->data()->isEmpty())
        return std::numeric_limits<double>::quiet_NaN();

    return PlotWidget::interpolateY(graph, x);
}

void LegendManager::calculateRangeStats(QCPGraph* graph, double xStart, double xEnd,
                                        double& minVal, double& avgVal, double& maxVal) const
{
    if (!graph || graph->data()->isEmpty()) {
        minVal = avgVal = maxVal = std::numeric_limits<double>::quiet_NaN();
        return;
    }

    auto data = graph->data();
    auto itStart = data->findBegin(xStart, false);
    auto itEnd = data->findEnd(xEnd, false);

    if (itStart == itEnd) {
        minVal = avgVal = maxVal = std::numeric_limits<double>::quiet_NaN();
        return;
    }

    minVal = std::numeric_limits<double>::max();
    maxVal = std::numeric_limits<double>::lowest();
    double sum = 0;
    int count = 0;

    for (auto it = itStart; it != itEnd; ++it) {
        double val = it->value;
        if (!std::isnan(val)) {
            minVal = std::min(minVal, val);
            maxVal = std::max(maxVal, val);
            sum += val;
            count++;
        }
    }

    if (count > 0) {
        avgVal = sum / count;
    } else {
        minVal = avgVal = maxVal = std::numeric_limits<double>::quiet_NaN();
    }
}

QString LegendManager::formatValue(double value, const QString& measurementId) const
{
    if (std::isnan(value))
        return "--";

    // Format based on measurement type
    int precision = 1;
    if (measurementId.contains("Lat") || measurementId.contains("Lon")) {
        precision = 6;
    } else if (measurementId.contains("Time")) {
        precision = 3;
    }

    return QString::number(value, 'f', precision);
}

void LegendManager::setElementText(int row, int col, const QString& text, const QColor& color)
{
    if (row > 0 && row <= m_dataElements.size() && col < m_dataElements[row-1].size()) {
        m_dataElements[row-1][col]->setText(text);
        if (color.isValid())
            m_dataElements[row-1][col]->setTextColor(color);
    }
}

void LegendManager::clearDataRows()
{
    for (const auto& row : m_dataElements) {
        for (int i = 1; i < row.size(); ++i) {
            row[i]->setText("");
        }
    }
}

void LegendManager::clearLegendElements()
{
    // Clear text elements
    for (auto* elem : m_headerElements) {
        m_legendLayout->remove(elem);
    }
    m_headerElements.clear();

    for (const auto& row : m_dataElements) {
        for (auto* elem : row) {
            m_legendLayout->remove(elem);
        }
    }
    m_dataElements.clear();
}

void LegendManager::updateLegendPosition()
{
    if (!m_legendLayout || !m_backgroundRect)
        return;

    // Force layout update first
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    // Calculate the actual bounds of the legend content
    double minX = m_plot->axisRect()->right();
    double minY = m_plot->axisRect()->top();
    double maxX = m_plot->axisRect()->left();
    double maxY = m_plot->axisRect()->bottom();

    // Find the actual bounds of all text elements
    bool hasElements = false;
    for (auto* elem : m_headerElements) {
        if (elem && elem->visible()) {
            QRectF elemRect = elem->outerRect();
            if (!elemRect.isEmpty()) {
                minX = qMin(minX, elemRect.left());
                minY = qMin(minY, elemRect.top());
                maxX = qMax(maxX, elemRect.right());
                maxY = qMax(maxY, elemRect.bottom());
                hasElements = true;
            }
        }
    }

    for (const auto& row : m_dataElements) {
        for (auto* elem : row) {
            if (elem && elem->visible()) {
                QRectF elemRect = elem->outerRect();
                if (!elemRect.isEmpty()) {
                    minX = qMin(minX, elemRect.left());
                    minY = qMin(minY, elemRect.top());
                    maxX = qMax(maxX, elemRect.right());
                    maxY = qMax(maxY, elemRect.bottom());
                    hasElements = true;
                }
            }
        }
    }

    if (hasElements) {
        // Add padding around the text
        minX -= 5;
        minY -= 5;
        maxX += 5;
        maxY += 5;

        // Set background rectangle position using plot coordinates
        m_backgroundRect->topLeft->setType(QCPItemPosition::ptPlotCoords);
        m_backgroundRect->bottomRight->setType(QCPItemPosition::ptPlotCoords);

        // Convert pixel positions to plot coordinates
        double x1 = m_plot->xAxis->pixelToCoord(minX);
        double y1 = m_plot->yAxis->pixelToCoord(minY);
        double x2 = m_plot->xAxis->pixelToCoord(maxX);
        double y2 = m_plot->yAxis->pixelToCoord(maxY);

        m_backgroundRect->topLeft->setCoords(x1, y1);
        m_backgroundRect->bottomRight->setCoords(x2, y2);
    }
}

} // namespace FlySight
