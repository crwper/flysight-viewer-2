#include "legendmanager.h"
#include "sessionmodel.h"
#include "plotwidget.h"
#include "legendwidget.h"

#include <QDateTime>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <limits>

namespace FlySight {

static LegendWidget::Mode toWidgetMode(LegendManager::Mode m)
{
    return (m == LegendManager::PointDataMode)
    ? LegendWidget::PointDataMode
    : LegendWidget::RangeStatsMode;
}

LegendManager::LegendManager(QCustomPlot *plot,
                             SessionModel *model,
                             QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                             LegendWidget *legendWidget,
                             QObject *parent)
    : QObject(parent)
    , m_plot(plot)
    , m_model(model)
    , m_graphInfoMap(graphInfoMap)
    , m_legendWidget(legendWidget)
    , m_visible(false)
    , m_mode(PointDataMode)
{
    collectVisibleSeries();
    if (m_legendWidget) {
        m_legendWidget->clear();
        m_legendWidget->setMode(toWidgetMode(m_mode));
    }
}

LegendManager::~LegendManager() = default;

void LegendManager::setVisible(bool visible)
{
    m_visible = visible;

    // In dock mode, "hidden" means "clear the widget"
    if (!m_legendWidget)
        return;

    if (!visible) {
        m_legendWidget->clear();
    }
}

void LegendManager::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    if (m_legendWidget) {
        m_legendWidget->setMode(toWidgetMode(m_mode));
    }
}

void LegendManager::rebuildLegend()
{
    collectVisibleSeries();

    // Clear the dock legend; it will be repopulated on next mouse move
    if (m_legendWidget) {
        m_legendWidget->clear();
        m_legendWidget->setMode(toWidgetMode(m_mode));
    }
}

void LegendManager::updateLegendPosition()
{
    // No-op: legend is no longer a QCustomPlot overlay.
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

bool LegendManager::updatePointData(double xCoord,
                                    const QString& targetSessionId,
                                    const QString& sessionDescription,
                                    const QString& xAxisKey)
{
    if (m_mode != PointDataMode)
        return false;

    if (m_visibleSeries.isEmpty()) {
        if (m_legendWidget)
            m_legendWidget->clear();
        return false;
    }

    // Header: session, UTC time, lat/lon/alt
    QString utcText;
    {
        double utcSecs = getRawValueAtX(targetSessionId, xAxisKey, xCoord,
                                        "GNSS", SessionKeys::Time);
        if (!std::isnan(utcSecs)) {
            utcText = QString("%1 UTC")
            .arg(QDateTime::fromMSecsSinceEpoch(
                     qint64(utcSecs * 1000.0),
                     Qt::UTC)
                     .toString("yy-MM-dd HH:mm:ss.zzz"));
        }
    }

    QString coordsText;
    {
        double lat = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", "lat");
        double lon = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", "lon");
        double alt = getRawValueAtX(targetSessionId, xAxisKey, xCoord, "GNSS", "hMSL");

        if (!std::isnan(lat) && !std::isnan(lon) && !std::isnan(alt)) {
            coordsText = QString("(%1 deg, %2 deg, %3 m)")
            .arg(lat, 0, 'f', 7)
                .arg(lon, 0, 'f', 7)
                .arg(alt, 0, 'f', 3);
        }
    }

    QVector<LegendWidget::Row> rows;
    rows.reserve(m_visibleSeries.size());

    bool hasData = false;

    // Single session mode - show values for the target session
    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        LegendWidget::Row row;
        row.name = m_visibleSeries[i].name;
        row.color = m_visibleSeries[i].color;

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
            if (!std::isnan(value))
                hasData = true;

            row.value = formatValue(value, m_visibleSeries[i].measurementId);
        } else {
            row.value = QStringLiteral("--");
        }

        rows.push_back(row);
    }

    if (m_legendWidget) {
        // Fallback: always show a session title in the header
        QString headerSession = sessionDescription;
        if (headerSession.isEmpty())
            headerSession = targetSessionId;

        m_legendWidget->setMode(LegendWidget::PointDataMode);

        m_legendWidget->setHeader(headerSession, utcText, coordsText);
        m_legendWidget->setRows(rows);
    }

    return hasData;
}

bool LegendManager::updateRangeStats(double xCoord)
{
    if (m_mode != RangeStatsMode)
        return false;

    if (m_visibleSeries.isEmpty()) {
        if (m_legendWidget)
            m_legendWidget->clear();
        return false;
    }

    QVector<LegendWidget::Row> rows;
    rows.reserve(m_visibleSeries.size());

    bool hasData = false;

    for (int i = 0; i < m_visibleSeries.size(); ++i) {
        LegendWidget::Row row;
        row.name = m_visibleSeries[i].name;
        row.color = m_visibleSeries[i].color;

        QVector<double> valuesAtCursor;

        // Iterate through all graphs to find those matching this measurement type
        for (auto it = m_graphInfoMap->constBegin(); it != m_graphInfoMap->constEnd(); ++it) {
            QCPGraph* graph = it.key();
            const GraphInfo& info = it.value();

            if (info.sensorId == m_visibleSeries[i].sensorId &&
                info.measurementId == m_visibleSeries[i].measurementId &&
                graph->visible()) {

                double value = interpolateValueAtX(graph, xCoord);
                if (!std::isnan(value)) {
                    valuesAtCursor.append(value);
                }
            }
        }

        if (!valuesAtCursor.isEmpty()) {
            hasData = true;

            double minVal = *std::min_element(valuesAtCursor.begin(), valuesAtCursor.end());
            double maxVal = *std::max_element(valuesAtCursor.begin(), valuesAtCursor.end());
            double avgVal = std::accumulate(valuesAtCursor.begin(), valuesAtCursor.end(), 0.0) / valuesAtCursor.size();

            row.minValue = formatValue(minVal, m_visibleSeries[i].measurementId);
            row.avgValue = formatValue(avgVal, m_visibleSeries[i].measurementId);
            row.maxValue = formatValue(maxVal, m_visibleSeries[i].measurementId);
        } else {
            row.minValue = QStringLiteral("--");
            row.avgValue = QStringLiteral("--");
            row.maxValue = QStringLiteral("--");
        }

        rows.push_back(row);
    }

    if (m_legendWidget) {
        m_legendWidget->setMode(LegendWidget::RangeStatsMode);
        m_legendWidget->setRows(rows);
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
        return QStringLiteral("--");

    int precision = 1;
    if (measurementId.contains("Lat") || measurementId.contains("Lon")) {
        precision = 6;
    } else if (measurementId.contains("Time")) {
        precision = 3;
    }

    return QString::number(value, 'f', precision);
}

double LegendManager::getRawValueAtX(const QString& sessionId,
                                     const QString& xAxisKey,
                                     double xCoord,
                                     const QString& sensorId,
                                     const QString& measurementId)
{
    if (!m_model) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    int row = m_model->getSessionRow(sessionId);
    if (row < 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    SessionData& session = m_model->sessionRef(row);

    QVector<double> xData = session.getMeasurement(sensorId, xAxisKey);
    QVector<double> yData = session.getMeasurement(sensorId, measurementId);

    if (xData.isEmpty() || xData.size() != yData.size()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

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
        return y1;
    }

    return y1 + (y2 - y1) * (xCoord - x1) / (x2 - x1);
}

} // namespace FlySight
