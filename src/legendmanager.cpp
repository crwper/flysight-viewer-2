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
    m_backgroundRect->setLayer("overlay");
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

    if (m_backgroundRect)
        m_backgroundRect->setVisible(visible);

    if (m_legendLayout)
        m_legendLayout->setVisible(visible);
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
        auto* valueHeader = new QCPTextElement(m_plot, "Value", QFont("Arial", 9, QFont::Bold));
        valueHeader->setLayer("overlay");
        m_legendLayout->addElement(0, 1, valueHeader);
        m_headerElements << valueHeader;
    } else {
        // Headers: Series | Min | Avg | Max (removed Value and Change)
        QStringList headers = {"Min", "Avg", "Max"};
        for (int i = 0; i < headers.size(); ++i) {
            auto* header = new QCPTextElement(m_plot, headers[i], QFont("Arial", 9, QFont::Bold));
            header->setLayer("overlay");
            m_legendLayout->addElement(0, i + 1, header);
            m_headerElements << header;
        }
    }

    // Create data rows for each series
    m_dataElements.clear();
    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        QVector<QCPTextElement*> row;
        int colCount = (m_mode == PointDataMode) ? 2 : 4; // Changed from 6 to 4 for range mode

        for (int j = 0; j < colCount; ++j) {
            auto* elem = new QCPTextElement(m_plot, "", QFont("Arial", 8));
            elem->setLayer("overlay");
            if (j == 0) {
                elem->setTextFlags(Qt::AlignLeft | Qt::AlignVCenter);
            } else {
                elem->setTextFlags(Qt::AlignHCenter | Qt::AlignVCenter);
            }
            m_legendLayout->addElement(i + 1, j, elem);
            row << elem;
        }

        // Set series name and color
        row[0]->setText(m_visibleSeries[i].name);
        row[0]->setTextColor(m_visibleSeries[i].color);

        m_dataElements << row;
    }
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
            si.name = info.displayName;
            si.color = info.defaultPen.color();
            si.sensorId = info.sensorId;
            si.measurementId = info.measurementId;
            si.graph = graph; // Store first graph for this measurement type
            seriesMap.insert(key, si);
        }
    }

    m_visibleSeries = seriesMap.values().toVector();
}

bool LegendManager::updatePointData(double xCoord, const QString& targetSessionId)
{
    if (m_mode != PointDataMode)
        return false;

    clearDataRows();
    bool hasData = false;

    // Single session mode - show values for the target session
    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        // Find the graph for this series and target session
        QCPGraph* targetGraph = nullptr;
        for (auto it = m_graphInfoMap->constBegin(); it != m_graphInfoMap->constEnd(); ++it) {
            const GraphInfo& info = it.value();
            if (info.sessionId == targetSessionId &&
                info.sensorId == m_visibleSeries[i].sensorId &&
                info.measurementId == m_visibleSeries[i].measurementId) {
                targetGraph = it.key();
                break;
            }
        }

        if (targetGraph && targetGraph->visible()) {
            double value = interpolateValueAtX(targetGraph, xCoord);
            if (!std::isnan(value)) {
                hasData = true;
            }
            setElementText(i + 1, 1, formatValue(value, m_visibleSeries[i].measurementId));
        } else {
            setElementText(i + 1, 1, "--");
        }
    }

    return hasData;
}

bool LegendManager::updateRangeStats(double xCoord)
{
    if (m_mode != RangeStatsMode)
        return false;

    clearDataRows();
    bool hasData = false;

    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        // Collect interpolated values at cursor position for this measurement type
        QVector<double> valuesAtCursor;

        // Iterate through all graphs to find those matching this measurement type
        for (auto it = m_graphInfoMap->constBegin(); it != m_graphInfoMap->constEnd(); ++it) {
            QCPGraph* graph = it.key();
            const GraphInfo& info = it.value();

            // Check if this graph matches the current series and is visible
            if (info.sensorId == m_visibleSeries[i].sensorId &&
                info.measurementId == m_visibleSeries[i].measurementId &&
                graph->visible()) {

                // Get the interpolated value at the cursor position
                double value = interpolateValueAtX(graph, xCoord);
                if (!std::isnan(value)) {
                    valuesAtCursor.append(value);
                }
            }
        }

        // Calculate min/max/average from the collected values
        if (!valuesAtCursor.isEmpty()) {
            hasData = true;
            double minVal = *std::min_element(valuesAtCursor.begin(), valuesAtCursor.end());
            double maxVal = *std::max_element(valuesAtCursor.begin(), valuesAtCursor.end());
            double avgVal = std::accumulate(valuesAtCursor.begin(), valuesAtCursor.end(), 0.0) / valuesAtCursor.size();

            // Set values in columns: Series | Min | Avg | Max
            setElementText(i + 1, 1, formatValue(minVal, m_visibleSeries[i].measurementId));
            setElementText(i + 1, 2, formatValue(avgVal, m_visibleSeries[i].measurementId));
            setElementText(i + 1, 3, formatValue(maxVal, m_visibleSeries[i].measurementId));
        } else {
            setElementText(i + 1, 1, "--");
            setElementText(i + 1, 2, "--");
            setElementText(i + 1, 3, "--");
        }
    }

    return hasData;
}

double LegendManager::interpolateValueAtX(QCPGraph* graph, double x) const
{
    if (!graph || graph->data()->isEmpty())
        return std::numeric_limits<double>::quiet_NaN();

    return PlotWidget::interpolateY(graph, x);
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
    if (!m_legendLayout || !m_backgroundRect || !m_visible)
        return;

    // Calculate the actual bounds of the legend content in pixels
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();

    bool hasElements = false;
    auto updateBounds = [&](QCPLayoutElement* elem) {
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
    };

    // Find the actual bounds of all text elements
    for (auto* elem : m_headerElements) {
        updateBounds(elem);
    }

    for (const auto& row : m_dataElements) {
        for (auto* elem : row) {
            updateBounds(elem);
        }
    }

    if (hasElements) {
        // Add padding around the text
        const int padding = 5;
        minX -= padding;
        minY -= padding;
        maxX += padding;
        maxY += padding;

        // Set background rectangle position using absolute pixel coordinates
        m_backgroundRect->topLeft->setType(QCPItemPosition::ptAbsolute);
        m_backgroundRect->bottomRight->setType(QCPItemPosition::ptAbsolute);

        // Use the calculated pixel coordinates directly
        m_backgroundRect->topLeft->setCoords(minX, minY);
        m_backgroundRect->bottomRight->setCoords(maxX, maxY);

        m_backgroundRect->setVisible(true);
    } else {
        m_backgroundRect->setVisible(false);
    }
}

} // namespace FlySight
