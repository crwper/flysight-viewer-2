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
    , m_mainLayout(nullptr)
    , m_tableLayout(nullptr)
    , m_backgroundRect(nullptr)
    , m_sessionDescElement(nullptr)
    , m_utcElement(nullptr)
    , m_coordsElement(nullptr)
    , m_separatorLine(nullptr)
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

    // Create and configure the main layout and table layout
    QCPLayoutInset *insetLayout = m_plot->axisRect()->insetLayout();
    m_mainLayout = new QCPLayoutGrid();
    m_mainLayout->setMargins(QMargins(5, 5, 5, 5));
    m_mainLayout->setRowSpacing(2);

    m_tableLayout = new QCPLayoutGrid();
    m_tableLayout->setRowSpacing(2);
    m_tableLayout->setColumnSpacing(10);

    // Add to inset layout at top-right
    insetLayout->addElement(m_mainLayout, Qt::AlignTop | Qt::AlignRight);
    m_mainLayout->setVisible(false);
}

void LegendManager::setVisible(bool visible)
{
    m_visible = visible;

    if (m_mainLayout)
        m_mainLayout->setVisible(visible);

    if (m_backgroundRect)
        m_backgroundRect->setVisible(visible);

    if (m_separatorLine)
        m_separatorLine->setVisible(visible);
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

    // Rebuild using the nested layout structure
    int currentRow = 0;

    if (m_mode == PointDataMode) {
        // 1. Create header info elements and add to mainLayout
        m_sessionDescElement = new QCPTextElement(m_plot, "", QFont("Arial", 8, QFont::Weight::Normal, true)); // CORRECTED: QFont::Italic
        m_sessionDescElement->setLayer("overlay");
        m_sessionDescElement->setTextFlags(Qt::AlignCenter);
        m_mainLayout->addElement(currentRow++, 0, m_sessionDescElement);

        m_utcElement = new QCPTextElement(m_plot, "", QFont("Arial", 8));
        m_utcElement->setLayer("overlay");
        m_utcElement->setTextFlags(Qt::AlignCenter);
        m_mainLayout->addElement(currentRow++, 0, m_utcElement);

        m_coordsElement = new QCPTextElement(m_plot, "", QFont("Arial", 8));
        m_coordsElement->setLayer("overlay");
        m_coordsElement->setTextFlags(Qt::AlignCenter);
        m_mainLayout->addElement(currentRow++, 0, m_coordsElement);

        // Create separator line (will be positioned later)
        m_separatorLine = new QCPItemLine(m_plot);
        m_separatorLine->setLayer("overlay");
        m_separatorLine->setPen(QPen(QColor(180, 180, 180), 1));
        m_separatorLine->setVisible(false); // Hide until positioned
    }


    // Create a NEW table layout and add it to the main layout.
    m_tableLayout = new QCPLayoutGrid();
    m_tableLayout->setRowSpacing(2);
    m_tableLayout->setColumnSpacing(10);
    m_mainLayout->addElement(currentRow++, 0, m_tableLayout);


    // 3. Populate the tableLayout with headers and data
    m_headerElements.clear();
    if (m_mode == PointDataMode) {
        auto* seriesHeader = new QCPTextElement(m_plot, "", QFont("Arial", 9, QFont::Bold));
        seriesHeader->setLayer("overlay");
        seriesHeader->setTextFlags(Qt::AlignLeft | Qt::AlignVCenter);
        auto* valueHeader = new QCPTextElement(m_plot, "Value", QFont("Arial", 9, QFont::Bold));
        valueHeader->setLayer("overlay");
        valueHeader->setTextFlags(Qt::AlignRight | Qt::AlignVCenter);
        m_tableLayout->addElement(0, 0, seriesHeader);
        m_tableLayout->addElement(0, 1, valueHeader);
        m_headerElements << seriesHeader << valueHeader;
    } else {
        QStringList headers = {"", "Min", "Avg", "Max"};
        for (int i = 0; i < headers.size(); ++i) {
            auto* header = new QCPTextElement(m_plot, headers[i], QFont("Arial", 9, QFont::Bold));
            header->setLayer("overlay");
            if (i == 0) header->setTextFlags(Qt::AlignLeft | Qt::AlignVCenter);
            else header->setTextFlags(Qt::AlignRight | Qt::AlignVCenter);
            m_tableLayout->addElement(0, i, header);
            m_headerElements << header;
        }
    }

    m_dataElements.clear();
    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        QVector<QCPTextElement*> row;
        int colCount = (m_mode == PointDataMode) ? 2 : 4;
        for (int j = 0; j < colCount; ++j) {
            auto* elem = new QCPTextElement(m_plot, "", QFont("Arial", 8));
            elem->setLayer("overlay");
            if (j == 0) elem->setTextFlags(Qt::AlignLeft | Qt::AlignVCenter);
            else elem->setTextFlags(Qt::AlignRight | Qt::AlignVCenter);
            m_tableLayout->addElement(i + 1, j, elem);
            row << elem;
        }
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

bool LegendManager::updatePointData(double xCoord, const QString& targetSessionId, const QString& sessionDescription, const QString& xAxisKey)
{
    if (m_mode != PointDataMode)
        return false;

    // Update header info for single session
    m_sessionDescElement->setText(sessionDescription);
    m_sessionDescElement->setVisible(true);

    // Get absolute time from raw data, not from x-axis
    double utcSecs = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", SessionKeys::Time);
    if (!std::isnan(utcSecs)) {
        QString utcText = QString("%1 UTC").arg(QDateTime::fromMSecsSinceEpoch(qint64(utcSecs * 1000.0), Qt::UTC).toString("yy-MM-dd HH:mm:ss.zzz"));
        m_utcElement->setText(utcText);
        m_utcElement->setVisible(true);
    } else {
        m_utcElement->setVisible(false);
    }

    // Get lat/lon/alt from raw data
    double lat = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", "lat");
    double lon = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", "lon");
    double alt = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", "hMSL");

    QString coordsText;
    if (!std::isnan(lat) && !std::isnan(lon) && !std::isnan(alt)) {
        coordsText = QString("(%1 deg, %2 deg, %3 m)")
        .arg(lat, 0, 'f', 7)
            .arg(lon, 0, 'f', 7)
            .arg(alt, 0, 'f', 3);
    }
    m_coordsElement->setText(coordsText);
    m_coordsElement->setVisible(!coordsText.isEmpty());

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

    // No need to hide session-specific info as it's not created in this mode
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
    // First, remove the QCPItemLine, which is not part of a layout.
    if (m_separatorLine) {
        m_plot->removeItem(m_separatorLine); // removeItem also deletes the object
        m_separatorLine = nullptr;
    }

    // Now, clearing the main layout will recursively delete all its children,
    // including all text elements AND the old m_tableLayout.
    if (m_mainLayout) {
        m_mainLayout->clear();
    }

    // The old elements are now deleted. We must clear our pointers to avoid dangling pointers.
    m_tableLayout = nullptr; // VERY IMPORTANT!
    m_sessionDescElement = nullptr;
    m_utcElement = nullptr;
    m_coordsElement = nullptr;
    m_headerElements.clear();
    m_dataElements.clear();
}

void LegendManager::updateLegendPosition()
{
    if (!m_mainLayout || !m_backgroundRect || !m_visible)
        return;

    // Calculate the actual bounds of the main layout, which now contains everything
    QRectF layoutRect = m_mainLayout->outerRect();
    if (layoutRect.isEmpty()) {
        m_backgroundRect->setVisible(false);
        if (m_separatorLine) m_separatorLine->setVisible(false);
        return;
    }

    double minX = layoutRect.left();
    double minY = layoutRect.top();
    double maxX = layoutRect.right();
    double maxY = layoutRect.bottom();

    m_backgroundRect->topLeft->setType(QCPItemPosition::ptAbsolute);
    m_backgroundRect->bottomRight->setType(QCPItemPosition::ptAbsolute);
    m_backgroundRect->topLeft->setCoords(minX, minY);
    m_backgroundRect->bottomRight->setCoords(maxX, maxY);
    m_backgroundRect->setVisible(true);

    // Position the separator line
    if (m_separatorLine && m_tableLayout->elementCount() > 0) {
        QRectF tableRect = m_tableLayout->outerRect();
        const int separatorPadding = 2;
        double yPos = tableRect.top() - separatorPadding;

        m_separatorLine->start->setType(QCPItemPosition::ptAbsolute);
        m_separatorLine->end->setType(QCPItemPosition::ptAbsolute);
        m_separatorLine->start->setCoords(minX + 5, yPos);
        m_separatorLine->end->setCoords(maxX - 5, yPos);

        // Show separator only if there's header text visible
        bool headerVisible = (m_sessionDescElement && m_sessionDescElement->visible()) ||
                             (m_utcElement && m_utcElement->visible()) ||
                             (m_coordsElement && m_coordsElement->visible());
        m_separatorLine->setVisible(headerVisible);
    } else if (m_separatorLine) {
        m_separatorLine->setVisible(false);
    }
}

double LegendManager::getRawValueAtX(const QString& sessionId, const QString& xAxisKey, double xCoord, const QString& sensorId, const QString& measurementId)
{
    if (!m_model) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    int row = m_model->getSessionRow(sessionId);
    if (row < 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Get non-const reference to session to allow lazy calculation of values
    SessionData& session = m_model->sessionRef(row);

    QVector<double> xData = session.getMeasurement(sensorId, xAxisKey);
    QVector<double> yData = session.getMeasurement(sensorId, measurementId);

    if (xData.isEmpty() || xData.size() != yData.size()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Find the closest data points for interpolation
    auto itLower = std::lower_bound(xData.cbegin(), xData.cend(), xCoord);

    if (itLower == xData.cbegin() || itLower == xData.cend()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    int index = std::distance(xData.cbegin(), itLower);

    double x1 = xData[index - 1];
    double y1 = yData[index - 1];
    double x2 = xData[index];
    double y2 = yData[index];

    if (x2 == x1) {
        // Avoid division by zero
        return y1;
    }

    // Linear interpolation
    return y1 + (y2 - y1) * (xCoord - x1) / (x2 - x1);
}

} // namespace FlySight
