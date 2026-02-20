#include "PlotWidget.h"
#include "plotmodel.h"
#include "markermodel.h"
#include "plotviewsettingsmodel.h"
#include "cursormodel.h"
#include "plotrangemodel.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"
#include "units/unitconverter.h"

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
#include "plottool/setexittool.h"
#include "plottool/setgroundtool.h"
#include "plottool/measuretool.h"

namespace {

/**
 * @brief Custom axis ticker that converts SI values to display units.
 *
 * This ticker applies a linear transformation (scale + offset) to tick values
 * to display them in the user's preferred unit system.
 */
class UnitConvertingTicker : public QCPAxisTicker
{
public:
    void setMeasurementType(const QString &type) { m_measurementType = type; }
    void setScale(double scale) { m_scale = scale; }
    void setOffset(double offset) { m_offset = offset; }
    void setPrecision(int precision) { m_precision = precision; }

protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
    {
        Q_UNUSED(locale)
        Q_UNUSED(formatChar)

        // Convert SI value to display units
        double displayValue = (tick * m_scale) + m_offset;

        // Use our precision, not the default
        int usePrecision = (m_precision >= 0) ? m_precision : precision;
        return QString::number(displayValue, 'f', usePrecision);
    }

private:
    QString m_measurementType;
    double m_scale = 1.0;
    double m_offset = 0.0;
    int m_precision = -1;
};

} // anonymous namespace

namespace FlySight {

// Constructor
PlotWidget::PlotWidget(SessionModel *model,
                       PlotModel *plotModel,
                       MarkerModel *markerModel,
                       PlotViewSettingsModel *viewSettingsModel,
                       CursorModel *cursorModel,
                       PlotRangeModel *rangeModel,
                       MeasureModel *measureModel,
                       QWidget *parent)
    : QWidget(parent)
    , customPlot(new QCustomPlot(this))
    , model(model)
    , plotModel(plotModel)
    , markerModel(markerModel)
    , m_viewSettingsModel(viewSettingsModel)
    , m_cursorModel(cursorModel)
    , m_rangeModel(rangeModel)
    , m_xAxisKey(SessionKeys::TimeFromExit)
    , m_xAxisLabel(tr("Time from exit (s)"))
{
    if (m_viewSettingsModel) {
        m_xAxisKey = m_viewSettingsModel->xAxisKey();
        m_xAxisLabel = m_viewSettingsModel->xAxisLabel();

        connect(m_viewSettingsModel, &PlotViewSettingsModel::xAxisChanged,
                this, &PlotWidget::onXAxisKeyChanged);
    }

    // set up the layout with the custom plot
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(customPlot);
    setLayout(layout);

    // initialize the plot and crosshairs
    setupPlot();

    // create the plot context for tools
    PlotContext ctx;
    ctx.widget = this;
    ctx.plot = customPlot;
    ctx.graphMap = &m_graphInfoMap;
    ctx.model = model;
    ctx.plotModel = plotModel;
    ctx.measureModel = measureModel;

    // instantiate tools for interacting with the plot
    m_panTool = std::make_unique<PanTool>(ctx);
    m_zoomTool = std::make_unique<ZoomTool>(ctx);
    m_measureTool = std::make_unique<MeasureTool>(ctx);
    m_selectTool = std::make_unique<SelectTool>(ctx);
    m_setExitTool = std::make_unique<SetExitTool>(ctx);
    m_setGroundTool = std::make_unique<SetGroundTool>(ctx);

    m_currentTool = m_panTool.get();
    m_primaryTool = Tool::Pan;

    // install an event filter to capture mouse events
    customPlot->installEventFilter(this);

    // create crosshair manager
    m_crosshairManager = std::make_unique<CrosshairManager>(
        customPlot,
        model,
        &m_graphInfoMap, // existing QMap<QCPGraph*, GraphInfo>
        this
        );

    // Update cursor model when traced sessions change (e.g. Shift press/release)
    connect(m_crosshairManager.get(), &CrosshairManager::tracedSessionsChanged,
            this, [this](const QSet<QString> &tracedSessions) {
        if (!m_cursorModel || !m_mouseInPlotArea)
            return;
        m_cursorModel->setCursorTargetsExplicit(
            QStringLiteral("mouse"), tracedSessions);
        m_cursorModel->setCursorActive(
            QStringLiteral("mouse"), !tracedSessions.isEmpty());
    });

    // For example, installing event filter:
    customPlot->installEventFilter(this);

    if (m_cursorModel) {
        connect(m_cursorModel, &CursorModel::cursorsChanged,
                this, &PlotWidget::onCursorsChanged);
    }

    // connect signals to slots for updates and interactions
    connect(model, &SessionModel::modelChanged, this, &PlotWidget::updatePlot);

    if (plotModel) {
        connect(plotModel, &QAbstractItemModel::modelReset,
                this, &PlotWidget::updatePlot);

        connect(plotModel, &QAbstractItemModel::dataChanged,
                this,
                [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                    updatePlot();
                });
    }

    connect(customPlot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged),
            this, &PlotWidget::onXAxisRangeChanged);

    connect(model, &SessionModel::hoveredSessionChanged,
            this, &PlotWidget::onHoveredSessionChanged);

    connect(model, &SessionModel::exitTimeChanged,
            this, &PlotWidget::onExitTimeChanged);

    // Connect to preferences system
    connect(&PreferencesManager::instance(), &PreferencesManager::preferenceChanged,
            this, &PlotWidget::onPreferenceChanged);

    // Connect to unit system changes for reactive updates
    connect(&UnitConverter::instance(), &UnitConverter::systemChanged,
            this, &PlotWidget::updatePlot);

    // Apply initial preferences
    applyPlotPreferences();

    // update the plot with initial data
    updatePlot();

    // Ensure ticker matches the initial x-axis mode (especially for UTC time)
    updateXAxisTicker();
}

PlotWidget::~PlotWidget() = default;

// Public Methods
void PlotWidget::setCurrentTool(Tool tool)
{
    // Close the old tool
    if (m_currentTool)
        m_currentTool->closeTool();

    // Switch to the appropriate tool based on the provided enum
    switch (tool) {
    case Tool::Pan:
        m_currentTool = m_panTool.get();
        break;
    case Tool::Zoom:
        m_currentTool = m_zoomTool.get();
        break;
    case Tool::Measure:
        m_currentTool = m_measureTool.get();
        break;
    case Tool::Select:
        m_currentTool = m_selectTool.get();
        break;
    case Tool::SetExit:
        m_currentTool = m_setExitTool.get();
        break;
    case Tool::SetGround:
        m_currentTool = m_setGroundTool.get();
        break;
    }

    // Update previous primary tool
    if (m_currentTool->isPrimary()) {
        m_primaryTool = tool;
    }

    // Activate the new tool
    if (m_currentTool)
        m_currentTool->activateTool();

    // Finally, notify
    emit toolChanged(tool);
}

void PlotWidget::revertToPrimaryTool()
{
    setCurrentTool(m_primaryTool);
}

void PlotWidget::setXAxisRange(double min, double max)
{
    // directly set the x-axis range on the plot
    customPlot->xAxis->setRange(min, max);
}

void PlotWidget::zoomToExtent()
{
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    bool hasData = false;

    const QVector<SessionData> &sessions = model->getAllSessions();

    for (const SessionData &session : sessions) {
        if (!session.isVisible())
            continue;

        QVector<double> xData = const_cast<SessionData &>(session)
            .getMeasurement(QStringLiteral("GNSS"), m_xAxisKey);
        if (xData.isEmpty())
            continue;

        auto [minIt, maxIt] = std::minmax_element(xData.begin(), xData.end());
        minX = std::min(minX, *minIt);
        maxX = std::max(maxX, *maxIt);
        hasData = true;
    }

    if (!hasData || minX >= maxX)
        return;

    customPlot->xAxis->setRange(minX, maxX);
}

void PlotWidget::handleSessionsSelected(const QList<QString> &sessionIds)
{
    // emit the sessionsSelected signal with the provided session IDs
    qDebug() << "Selected sessions:" << sessionIds;
    emit sessionsSelected(sessionIds);
}

CrosshairManager* PlotWidget::crosshairManager() const
{
    return m_crosshairManager.get();
}

void PlotWidget::lockFocusToSession(const QString &sessionId)
{
    if (m_crosshairManager)
        m_crosshairManager->setToolFocusLock(sessionId);
}

void PlotWidget::unlockFocus()
{
    if (m_crosshairManager)
        m_crosshairManager->clearToolFocusLock();
}

QString PlotWidget::getXAxisKey() const
{
    return m_xAxisKey;
}

// Slots
void PlotWidget::updatePlot()
{
    // Apply any pending x-axis adjustment from exit time change.
    // This must happen at the start of updatePlot() so the range adjustment
    // and graph data rebuild occur together, preventing visual flash.
    if (m_pendingExitDelta != 0.0) {
        QCPRange newRange;
        {
            // Block signals to prevent onXAxisRangeChanged from triggering
            // an intermediate replot while graphs are being rebuilt.
            QSignalBlocker blocker(customPlot->xAxis);
            QCPRange oldRange = customPlot->xAxis->range();
            newRange.lower = oldRange.lower - m_pendingExitDelta;
            newRange.upper = oldRange.upper - m_pendingExitDelta;
            customPlot->xAxis->setRange(newRange);
        }

        // Broadcast the updated range to interested listeners (e.g., map).
        if (m_rangeModel) {
            m_rangeModel->setRange(m_xAxisKey, newRange.lower, newRange.upper);
        }

        m_pendingExitDelta = 0.0;
    }

    // Clear existing graphs and axes
    customPlot->clearPlottables();
    m_graphInfoMap.clear();
    m_graphDrawOrder.clear();

    const QList<QCPAxis *> axesToRemove = m_plotValueAxes.values();
    m_plotValueAxes.clear();
    for (auto axis : axesToRemove) {
        customPlot->axisRect()->removeAxis(axis);
    }

    // Retrieve all session data
    const QVector<SessionData> &sessions = model->getAllSessions();
    QString hoveredSessionId = model->hoveredSessionId(); // Retrieve hovered session ID

    // Add graphs for each enabled plot
    const QVector<PlotValue> plots = plotModel ? plotModel->enabledPlots() : QVector<PlotValue>{};
    for (const PlotValue &pv : plots) {
        // Retrieve metadata for the graph
        QColor color = pv.defaultColor;

        // Check for user-configured color preference
        QString colorKey = PreferenceKeys::plotColorKey(pv.sensorID, pv.measurementID);
        QVariant colorPref = PreferencesManager::instance().getValue(colorKey);
        if (colorPref.isValid()) {
            QColor prefColor = colorPref.value<QColor>();
            if (prefColor.isValid()) {
                color = prefColor;
            }
        }

        QString sensorID = pv.sensorID;
        QString measurementID = pv.measurementID;
        QString plotName = pv.plotName;
        QString plotUnits = pv.plotUnits;

        QString plotValueID = sensorID + "/" + measurementID;

        // Create a new y-axis if one doesn't exist for this plot value
        if (!m_plotValueAxes.contains(plotValueID)) {
            QCPAxis *newYAxis = customPlot->axisRect()->addAxis(QCPAxis::atLeft);

            // Determine display unit label based on measurementType
            QString displayUnits = plotUnits; // Fallback to static plotUnits
            if (!pv.measurementType.isEmpty()) {
                QString convertedLabel = UnitConverter::instance().getUnitLabel(pv.measurementType);
                if (!convertedLabel.isEmpty()) {
                    displayUnits = convertedLabel;
                }
            }

            if (!displayUnits.isEmpty()) {
                newYAxis->setLabel(plotName + " (" + displayUnits + ")");
            } else {
                newYAxis->setLabel(plotName);
            }
            newYAxis->setLabelColor(color);
            newYAxis->setTickLabelColor(color);
            newYAxis->setBasePen(QPen(color));
            newYAxis->setTickPen(QPen(color));
            newYAxis->setSubTickPen(QPen(color));

            // Configure unit-converting ticker for this axis
            if (!pv.measurementType.isEmpty()) {
                auto ticker = QSharedPointer<UnitConvertingTicker>::create();
                ticker->setMeasurementType(pv.measurementType);

                // Get conversion parameters: scale = convert(1) - convert(0), offset = convert(0)
                double offset = UnitConverter::instance().convert(0.0, pv.measurementType);
                double scale = UnitConverter::instance().convert(1.0, pv.measurementType) - offset;
                ticker->setScale(scale);
                ticker->setOffset(offset);
                ticker->setPrecision(UnitConverter::instance().getPrecision(pv.measurementType));

                newYAxis->setTicker(ticker);
            }

            m_plotValueAxes.insert(plotValueID, newYAxis);
        }

        QCPAxis *assignedYAxis = m_plotValueAxes.value(plotValueID);

        // Add graphs for each visible session
        for (const auto &session : sessions) {
            if (!session.isVisible()) {
                continue;
            }

            QVector<double> yData = const_cast<SessionData &>(session).getMeasurement(sensorID, measurementID);
            if (yData.isEmpty()) {
                qWarning() << "No data available for plot:" << plotName << "in session:" << session.getAttribute(SessionKeys::SessionId);
                continue;
            }

            QVector<double> xData = const_cast<SessionData &>(session).getMeasurement(sensorID, m_xAxisKey);
            if (xData.isEmpty() || xData.size() != yData.size()) {
                qWarning() << "Time and measurement data size mismatch for session:" << session.getAttribute(SessionKeys::SessionId);
                continue;
            }

            GraphInfo info;
            info.sessionId = session.getAttribute(SessionKeys::SessionId).toString();
            info.sensorId = sensorID;
            info.measurementId = measurementID;

            // Determine display unit label based on measurementType (same logic as Y-axis labels)
            QString graphDisplayUnits = plotUnits; // Fallback
            if (!pv.measurementType.isEmpty()) {
                QString convertedLabel = UnitConverter::instance().getUnitLabel(pv.measurementType);
                if (!convertedLabel.isEmpty()) {
                    graphDisplayUnits = convertedLabel;
                }
            }

            if (!graphDisplayUnits.isEmpty()) {
                info.displayName = QString("%1 (%2)").arg(plotName).arg(graphDisplayUnits);
            } else {
                info.displayName = plotName;
            }
            info.defaultPen = QPen(QColor(color), m_lineThickness);

            QCPGraph *graph = customPlot->addGraph(customPlot->xAxis, assignedYAxis);
            graph->setPen(determineGraphPen(info, hoveredSessionId));
            graph->setData(xData, yData);
            graph->setLineStyle(QCPGraph::lsLine);
            graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));
            graph->setLayer(determineGraphLayer(info, hoveredSessionId));

            m_graphInfoMap.insert(graph, info);
            m_graphDrawOrder.append(graph);
        }
    }

    updateReferenceMarkers(UpdateMode::Rebuild);

    // Adjust y-axis ranges based on the updated x-axis range
    onXAxisRangeChanged(customPlot->xAxis->range());
}

void PlotWidget::updateMarkersOnly()
{
    // Marker-only update path: do not rebuild graphs
    updateReferenceMarkers(UpdateMode::Rebuild);

    // Rebuild may change axisRect margins (lane count), which changes coordToPixel mapping.
    // Force QCustomPlot to apply the new layout before we position absolute marker items.
    customPlot->replot(QCustomPlot::rpImmediateRefresh);

    updateReferenceMarkers(UpdateMode::Reflow);
    customPlot->replot(QCustomPlot::rpQueuedReplot);
}

void PlotWidget::applyPlotPreferences()
{
    auto &prefs = PreferencesManager::instance();

    // Cache global settings
    m_lineThickness = prefs.getValue(PreferenceKeys::PlotsLineThickness).toDouble();
    m_textSize = prefs.getValue(PreferenceKeys::PlotsTextSize).toInt();
    m_yAxisPadding = prefs.getValue(PreferenceKeys::PlotsYAxisPadding).toDouble();

    // Apply text size to axis labels
    QFont axisFont = customPlot->xAxis->labelFont();
    axisFont.setPointSize(m_textSize);
    customPlot->xAxis->setLabelFont(axisFont);
    customPlot->xAxis->setTickLabelFont(axisFont);

    // Apply to all Y axes
    for (auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it) {
        QCPAxis *yAxis = it.value();
        yAxis->setLabelFont(axisFont);
        yAxis->setTickLabelFont(axisFont);
    }
}

void PlotWidget::onPreferenceChanged(const QString &key, const QVariant &value)
{
    // Global plot settings
    if (key == PreferenceKeys::PlotsLineThickness) {
        m_lineThickness = value.toDouble();
        updatePlot(); // Rebuild graphs with new line thickness
        return;
    }

    if (key == PreferenceKeys::PlotsTextSize) {
        m_textSize = value.toInt();
        applyPlotPreferences();
        customPlot->replot();
        return;
    }

    if (key == PreferenceKeys::PlotsYAxisPadding) {
        m_yAxisPadding = value.toDouble();
        onXAxisRangeChanged(customPlot->xAxis->range()); // Recalculate Y ranges
        return;
    }

    // Per-plot color changes
    if (key.startsWith("plots/") && key.endsWith("/color")) {
        updatePlot(); // Rebuild to apply new color
        return;
    }

    // Per-plot Y-axis mode changes
    if (key.startsWith("plots/") && (key.endsWith("/yAxisMode") ||
                                      key.endsWith("/yAxisMin") ||
                                      key.endsWith("/yAxisMax"))) {
        onXAxisRangeChanged(customPlot->xAxis->range()); // Recalculate Y ranges
        return;
    }
}

void PlotWidget::onXAxisRangeChanged(const QCPRange &newRange)
{
    if (m_updatingYAxis) return;

    m_updatingYAxis = true;

    // iterate through all y-axes and adjust their ranges
    for (auto it = m_plotValueAxes.constBegin(); it != m_plotValueAxes.constEnd(); ++it) {
        QCPAxis* yAxis = it.value();
        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();

        for (int i = 0; i < customPlot->graphCount(); ++i) {
            QCPGraph* graph = customPlot->graph(i);
            if (graph->valueAxis() != yAxis) continue;

            auto itLower = graph->data()->findBegin(newRange.lower, false);
            auto itUpper = graph->data()->findEnd(newRange.upper, false);
            for (auto it = itLower; it != itUpper; ++it) {
                double y = it->value;
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
            }

            double yLower = interpolateY(graph, newRange.lower);
            double yUpper = interpolateY(graph, newRange.upper);
            if (!std::isnan(yLower)) {
                yMin = std::min(yMin, yLower);
                yMax = std::max(yMax, yLower);
            }
            if (!std::isnan(yUpper)) {
                yMin = std::min(yMin, yUpper);
                yMax = std::max(yMax, yUpper);
            }
        }

        if (yMin < yMax) {
            // Extract sensorID/measurementID from axis key
            QString axisKey = it.key(); // Format: "sensorID/measurementID"
            QStringList parts = axisKey.split('/');

            bool useManualRange = false;
            double manualMin = 0.0;
            double manualMax = 100.0;

            if (parts.size() == 2) {
                QString sensorID = parts[0];
                QString measurementID = parts[1];

                // Check Y-axis mode preference
                QString modeKey = PreferenceKeys::plotYAxisModeKey(sensorID, measurementID);
                QString mode = PreferencesManager::instance().getValue(modeKey).toString();

                if (mode.toLower() == "manual") {
                    useManualRange = true;
                    QString minKey = PreferenceKeys::plotYAxisMinKey(sensorID, measurementID);
                    QString maxKey = PreferenceKeys::plotYAxisMaxKey(sensorID, measurementID);
                    manualMin = PreferencesManager::instance().getValue(minKey).toDouble();
                    manualMax = PreferencesManager::instance().getValue(maxKey).toDouble();
                }
            }

            if (useManualRange && manualMax > manualMin) {
                yAxis->setRange(manualMin, manualMax);
            } else {
                // Auto mode: use data range with padding
                double padding = (yMax - yMin) * m_yAxisPadding;
                padding = (padding == 0) ? 1.0 : padding;
                yAxis->setRange(yMin - padding, yMax + padding);
            }
        }
    }

    customPlot->replot();

    // Position markers and crosshairs AFTER replot so coord/pixel conversion uses
    // correct axis rect geometry. Y-axis range changes (above) affect label widths,
    // which affect axisRect layout.
    updateReferenceMarkers(UpdateMode::Reflow);
    if (m_crosshairManager)
        m_crosshairManager->updateIfOverPlotArea();
    customPlot->replot(QCustomPlot::rpQueuedReplot);

    if (m_xAxisKey == SessionKeys::Time)
        updateXAxisTicker();      // update format when span changes

    // Broadcast range to interested listeners (e.g., map)
    if (m_rangeModel) {
        m_rangeModel->setRange(m_xAxisKey, newRange.lower, newRange.upper);
    }

    m_updatingYAxis = false;
}

void PlotWidget::onXAxisKeyChanged(const QString &newKey, const QString &newLabel)
{
    qDebug() << "PlotWidget::onXAxisKeyAndLabelChanged - Key:" << newKey << "Label:" << newLabel;
    if (m_xAxisKey == newKey && m_xAxisLabel == newLabel) {
        return; // No change
    }
    applyXAxisChange(newKey, newLabel);
}

void PlotWidget::onHoveredSessionChanged(const QString &sessionId)
{
    // Update graph appearance based on the hovered session.
    // Iterate in plot-model draw order (not QMap pointer order) so that
    // setLayer() calls re-establish the correct visual stacking.
    for (QCPGraph *graph : m_graphDrawOrder) {
        auto it = m_graphInfoMap.constFind(graph);
        if (it == m_graphInfoMap.constEnd())
            continue;
        const GraphInfo &info = it.value();

        graph->setLayer(determineGraphLayer(info, sessionId));
        graph->setPen(determineGraphPen(info, sessionId));
    }

    customPlot->replot();
}

void PlotWidget::onCursorsChanged()
{
    if (!m_cursorModel || !m_crosshairManager)
        return;

    // While the mouse is inside the plot area, PlotWidget owns cursor visuals.
    if (m_mouseInPlotArea)
        return;

    // Choose the effective cursor using the same precedence rules as the legend:
    // 1) usable "mouse" (active + explicit non-empty targets)
    // 2) first active non-mouse cursor (CursorModel insertion order)
    // 3) none
    CursorModel::Cursor effective;

    auto mouseUsable = [](const CursorModel::Cursor &c) -> bool {
        return c.active &&
               c.targetPolicy == CursorModel::TargetPolicy::Explicit &&
               !c.targetSessions.isEmpty();
    };

    if (m_cursorModel->hasCursor(QStringLiteral("mouse"))) {
        const CursorModel::Cursor mouse = m_cursorModel->cursorById(QStringLiteral("mouse"));
        if (mouseUsable(mouse)) {
            effective = mouse;
        }
    }

    if (effective.id.isEmpty()) {
        const int n = m_cursorModel->rowCount();
        for (int row = 0; row < n; ++row) {
            const QModelIndex idx = m_cursorModel->index(row, 0);
            const QString id = m_cursorModel->data(idx, CursorModel::IdRole).toString();
            if (id.isEmpty() || id == QStringLiteral("mouse"))
                continue;

            const CursorModel::Cursor c = m_cursorModel->cursorById(id);
            if (c.active) {
                effective = c;
                break;
            }
        }
    }

    if (effective.id.isEmpty() || !effective.active) {
        m_crosshairManager->clearExternalCursor();
        return;
    }

    const QVector<SessionData> &sessions = model->getAllSessions();

    // Build a fast id → session lookup.
    QHash<QString, const SessionData *> sessionById;
    sessionById.reserve(sessions.size());
    for (const auto &s : sessions) {
        const QString sid = s.getAttribute(SessionKeys::SessionId).toString();
        if (!sid.isEmpty())
            sessionById.insert(sid, &s);
    }

    auto tryExitTimeSeconds = [](const SessionData &s, double *outExitUtcSeconds) -> bool {
        if (!outExitUtcSeconds)
            return false;

        QVariant v = s.getAttribute(SessionKeys::ExitTime);
        if (!v.canConvert<QDateTime>())
            return false;

        QDateTime dt = v.toDateTime();
        if (!dt.isValid())
            return false;

        *outExitUtcSeconds = dt.toMSecsSinceEpoch() / 1000.0;
        return true;
    };

    auto xPlotForSession = [&](const CursorModel::Cursor &c,
                              const SessionData &s,
                              double *outXPlot) -> bool {
        if (!outXPlot)
            return false;

        if (c.positionSpace == CursorModel::PositionSpace::PlotAxisCoord) {
            if (c.axisKey != m_xAxisKey)
                return false;

            *outXPlot = c.positionValue;
            return true;
        }

        if (c.positionSpace == CursorModel::PositionSpace::UtcSeconds) {
            if (m_xAxisKey == SessionKeys::Time) {
                *outXPlot = c.positionValue;
                return true;
            }

            if (m_xAxisKey == SessionKeys::TimeFromExit) {
                double exitUtc = 0.0;
                if (!tryExitTimeSeconds(s, &exitUtc))
                    return false;

                *outXPlot = c.positionValue - exitUtc;
                return true;
            }

            // Fallback: treat UTC seconds as plot x.
            *outXPlot = c.positionValue;
            return true;
        }

        return false;
    };

    auto sessionOverlapsAtXPlot = [&](const QString &sessionId, double xPlot) -> bool {
        for (auto it = m_graphInfoMap.cbegin(); it != m_graphInfoMap.cend(); ++it) {
            if (it.value().sessionId != sessionId)
                continue;

            QCPGraph *g = it.key();
            if (!g || !g->visible())
                continue;

            const double yPlot = PlotWidget::interpolateY(g, xPlot);
            if (!std::isnan(yPlot))
                return true;
        }
        return false;
    };

    // Case 1: Map hover (existing behavior): "mouse" + exactly one explicit target session.
    if (effective.id == QStringLiteral("mouse")) {
        if (effective.targetPolicy != CursorModel::TargetPolicy::Explicit ||
            effective.targetSessions.size() != 1) {
            m_crosshairManager->clearExternalCursor();
            return;
        }

        const QString sessionId = *effective.targetSessions.constBegin();
        const SessionData *sessionPtr = sessionById.value(sessionId, nullptr);
        if (!sessionPtr) {
            m_crosshairManager->clearExternalCursor();
            return;
        }

        double xPlot = 0.0;
        if (!xPlotForSession(effective, *sessionPtr, &xPlot)) {
            m_crosshairManager->clearExternalCursor();
            return;
        }

        m_crosshairManager->setExternalCursor(sessionId, xPlot);
        return;
    }

    // Case 2: Non-mouse effective cursor (video playback / pinned / etc.): multi-session external cursor.
    QHash<QString, double> xBySession;

    const bool explicitTargets =
        (effective.targetPolicy == CursorModel::TargetPolicy::Explicit &&
         !effective.targetSessions.isEmpty());

    if (explicitTargets) {
        for (const QString &sid : effective.targetSessions) {
            const SessionData *s = sessionById.value(sid, nullptr);
            if (!s || !s->isVisible())
                continue;

            double xPlot = 0.0;
            if (!xPlotForSession(effective, *s, &xPlot))
                continue;

            xBySession.insert(sid, xPlot);
        }
    } else {
        // Auto-visible-overlap: derive from visible sessions that overlap at the cursor time.
        for (const auto &s : sessions) {
            if (!s.isVisible())
                continue;

            const QString sid = s.getAttribute(SessionKeys::SessionId).toString();
            if (sid.isEmpty())
                continue;

            double xPlot = 0.0;
            if (!xPlotForSession(effective, s, &xPlot))
                continue;

            if (!sessionOverlapsAtXPlot(sid, xPlot))
                continue;

            xBySession.insert(sid, xPlot);
        }
    }

    if (xBySession.isEmpty()) {
        m_crosshairManager->clearExternalCursor();
        return;
    }

    // Show a vertical line only when all sessions share the same xPlot.
    m_crosshairManager->setExternalCursorMulti(xBySession, true);
}

void PlotWidget::onExitTimeChanged(const QString& sessionId, double deltaSeconds)
{
    // Only apply range adjustment in TimeFromExit mode.
    // In absolute Time mode, exit time changes don't affect x-coordinates.
    if (m_xAxisKey != SessionKeys::TimeFromExit)
        return;

    // Check if the changed session is the reference session.
    // The adjustment should be based on the reference session only.
    const SessionData* ref = referenceSession();
    if (!ref)
        return;

    QString refId = ref->getAttribute(SessionKeys::SessionId).toString();
    if (refId != sessionId)
        return;

    // Store the delta to be applied at the start of updatePlot().
    // We don't adjust the range here because Qt may process a screen refresh
    // between this signal and modelChanged, causing a visual flash where the
    // x-axis is shifted but the graph data hasn't been rebuilt yet.
    m_pendingExitDelta = deltaSeconds;
}

// Protected Methods
bool PlotWidget::eventFilter(QObject *obj, QEvent *event)
{
    // Step 5: Keep reference markers aligned when plot geometry changes (resize/layout, margins).
    // Spec trigger: QEvent::Resize on customPlot -> updateReferenceMarkers(Reflow)
    if (obj == customPlot && event->type() == QEvent::Resize) {
        // Defer to the next event loop turn so QCustomPlot can apply its new geometry first.
        QTimer::singleShot(0, this, [this]() {
            updateReferenceMarkers(UpdateMode::Reflow);
            customPlot->replot(QCustomPlot::rpQueuedReplot);
        });
    }

    if (obj == customPlot && m_currentTool) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto me = static_cast<QMouseEvent*>(event);

            const bool wasInPlotArea = m_mouseInPlotArea;

            const bool inPlotArea =
                customPlot->axisRect()->rect().contains(me->pos());
            m_mouseInPlotArea = inPlotArea;

            // forward to crosshairManager if desired:
            if (m_crosshairManager) {
                m_crosshairManager->handleMouseMove(me->pos());
            }

            // Step 3: write mouse cursor state into CursorModel
            if (m_cursorModel) {
                const QSet<QString> tracedSessions =
                    m_crosshairManager ? m_crosshairManager->getTracedSessionIds()
                                       : QSet<QString>{};

                if (inPlotArea) {
                    const double xCoord =
                        customPlot->xAxis->pixelToCoord(me->pos().x());

                    m_cursorModel->setCursorPositionPlotAxis(
                        QStringLiteral("mouse"),
                        m_xAxisKey,
                        xCoord
                    );
                }

                m_cursorModel->setCursorTargetsExplicit(
                    QStringLiteral("mouse"),
                    tracedSessions
                );

                // v1 semantics: mouse cursor is "usable" only when in plot AND has targets
                m_cursorModel->setCursorActive(
                    QStringLiteral("mouse"),
                    inPlotArea && !tracedSessions.isEmpty()
                );
            }

            // If the mouse just left the plot area, force a re-evaluation of the effective cursor
            // so external (e.g., video) cursors re-appear immediately even if CursorModel didn't change.
            if (wasInPlotArea && !inPlotArea) {
                onCursorsChanged();
            }

            // also forward to the current tool
            return m_currentTool->mouseMoveEvent(me);
        }
        case QEvent::MouseButtonPress: {
            auto me = static_cast<QMouseEvent*>(event);
            return m_currentTool->mousePressEvent(me);
        }
        case QEvent::MouseButtonRelease: {
            auto me = static_cast<QMouseEvent*>(event);
            return m_currentTool->mouseReleaseEvent(me);
        }
        case QEvent::Leave:
            m_mouseInPlotArea = false;

            if (m_crosshairManager) {
                m_crosshairManager->handleMouseLeave();
            }

            // Step 3: mark mouse cursor inactive and clear targets on leave
            if (m_cursorModel) {
                m_cursorModel->setCursorTargetsExplicit(
                    QStringLiteral("mouse"),
                    QSet<QString>{}
                );
                m_cursorModel->setCursorActive(
                    QStringLiteral("mouse"),
                    false
                );
            }

            // Force re-evaluation so external (e.g., video) cursors come back immediately.
            onCursorsChanged();

            m_currentTool->leaveEvent(event);
            return false;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

// Initialization

void PlotWidget::setupPlot()
{
    // configure basic plot settings
    customPlot->xAxis->setLabel(m_xAxisLabel);
    customPlot->yAxis->setVisible(false);

    // enable interactions for range dragging and zooming
    customPlot->setInteraction(QCP::iRangeDrag, true);
    customPlot->setInteraction(QCP::iRangeZoom, true);

    // restrict range interactions to horizontal only
    customPlot->axisRect()->setRangeDrag(Qt::Horizontal);
    customPlot->axisRect()->setRangeZoom(Qt::Horizontal);

    // Reserve top margin for the reference marker lane
    const int laneHeightPx = 32;
    customPlot->axisRect()->setMinimumMargins(QMargins(0, laneHeightPx, 0, 0));

    // create a dedicated layer for highlighted graphs
    customPlot->addLayer("highlighted", customPlot->layer("main"), QCustomPlot::limAbove);

    // create a dedicated layer for reference marker items (drawn above all plot content)
    QCPLayer *aboveLayer = customPlot->layer("overlay");
    if (!aboveLayer)
        aboveLayer = customPlot->layer("highlighted");
    customPlot->addLayer("referenceMarkers", aboveLayer, QCustomPlot::limAbove);
}

void PlotWidget::updateXAxisTicker()
{
    if (m_xAxisKey == SessionKeys::Time) {
        // Absolute UTC time → human‑readable
        auto dtTicker = QSharedPointer<QCPAxisTickerDateTime>::create();
        dtTicker->setDateTimeSpec(Qt::UTC);

        // Choose format based on current visible span (seconds)
        double span = customPlot->xAxis->range().size();
        if (span < 30)                   // < 30 s
            dtTicker->setDateTimeFormat("HH:mm:ss.z");
        else if (span < 3600)            // < 1 h
            dtTicker->setDateTimeFormat("HH:mm:ss");
        else if (span < 86400)           // < 1 day
            dtTicker->setDateTimeFormat("HH:mm");
        else                             // ≥ 1 day
            dtTicker->setDateTimeFormat("yyyy‑MM‑dd\nHH:mm");

        customPlot->xAxis->setTicker(dtTicker);
    } else {
        // Relative seconds → default numeric ticker
        customPlot->xAxis->setTicker(QSharedPointer<QCPAxisTicker>::create());
    }
}

// Utility Methods
double PlotWidget::interpolateY(const QCPGraph* graph, double x)
{
    // find the closest data points for interpolation
    auto itLower = graph->data()->findBegin(x, false);
    if (itLower == graph->data()->constBegin() || itLower == graph->data()->constEnd()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    auto itPrev = itLower;
    --itPrev;

    double x1 = itPrev->key;
    double y1 = itPrev->value;
    double x2 = itLower->key;
    double y2 = itLower->value;

    if (x2 == x1) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

QPen PlotWidget::determineGraphPen(const GraphInfo &info, const QString &hoveredSessionId) const
{
    // Always use the default pen
    return info.defaultPen;
}

QString PlotWidget::determineGraphLayer(const GraphInfo &info, const QString &hoveredSessionId) const
{
    // Always use the default layer
    return "highlighted";
}

// View management
const SessionData* PlotWidget::referenceSession() const
{
    // 1. hovered?
    QString hovered = model->hoveredSessionId();
    if (!hovered.isEmpty()) {
        for (const auto& s : model->getAllSessions())
            if (s.getAttribute(SessionKeys::SessionId).toString() == hovered)
                return &s;
    }
    // 2. first visible
    for (const auto& s : model->getAllSessions())
        if (s.isVisible()) return &s;

    return nullptr;           // should not happen
}

double PlotWidget::exitTimeSeconds(const SessionData& s)
{
    QVariant v = s.getAttribute(SessionKeys::ExitTime);
    if (!v.canConvert<QDateTime>()) return 0.0;
    return v.toDateTime().toMSecsSinceEpoch() / 1000.0;
}

QVector<ReferenceMoment> PlotWidget::collectExitMoments() const
{
    QVector<ReferenceMoment> exitMoments;

    for (const auto& s : model->getAllSessions()) {
        if (!s.isVisible())
            continue;

        QVariant v = s.getAttribute(SessionKeys::ExitTime);
        if (!v.canConvert<QDateTime>())
            continue;

        QDateTime dt = v.toDateTime();
        if (!dt.isValid())
            continue;

        ReferenceMoment moment;
        moment.sessionId = s.getAttribute(SessionKeys::SessionId).toString();
        moment.exitUtcSeconds = dt.toMSecsSinceEpoch() / 1000.0;

        exitMoments.append(moment);
    }

    return exitMoments;
}

void PlotWidget::updateReferenceMarkers(UpdateMode mode)
{
    auto clearLaneItems = [this](QVector<QPointer<QCPAbstractItem>> &laneItems) {
        for (auto &item : laneItems) {
            if (!item)
                continue;

            // Keep marker-bubble metadata in sync with item lifetime.
            if (QCPItemText *bubble = qobject_cast<QCPItemText *>(item.data())) {
                m_markerBubbleMeta.remove(bubble);
            }

            customPlot->removeItem(item);
        }
        laneItems.clear();
    };

    auto clearAllItems = [this, &clearLaneItems]() {
        for (auto &laneItems : m_markerItemsByLane) {
            clearLaneItems(laneItems);
        }
        m_markerItemsByLane.clear();
        m_markerBubbleMeta.clear();
    };

    if (mode == UpdateMode::Rebuild) {
        clearAllItems();
    }

    const QVector<MarkerDefinition> enabledDefs =
        markerModel ? markerModel->enabledMarkers() : QVector<MarkerDefinition>{};

    // Step 5: One lane per enabled marker type (single marker parity: 1 lane == 32px)
    const int laneHeightPx = 32;
    customPlot->axisRect()->setMinimumMargins(QMargins(0, laneHeightPx * enabledDefs.size(), 0, 0));

    if (enabledDefs.isEmpty()) {
        if (!m_markerItemsByLane.isEmpty() || !m_markerBubbleMeta.isEmpty())
            clearAllItems();
        return;
    }

    // Ensure lane storage matches the enabled marker count
    if (m_markerItemsByLane.size() != enabledDefs.size()) {
        clearAllItems();
        m_markerItemsByLane.resize(enabledDefs.size());
    } else if (m_markerItemsByLane.isEmpty()) {
        m_markerItemsByLane.resize(enabledDefs.size());
    }

    // Only draw markers that are currently within the visible x range
    const QCPRange xRange = customPlot->xAxis->range();
    const QRect axisRectPx = customPlot->axisRect()->rect();

    auto tryAttributeUtcSeconds =
        [](const SessionData &s, const QString &attributeKey, double *outUtcSeconds) -> bool {
            if (!outUtcSeconds)
                return false;

            QVariant v = s.getAttribute(attributeKey);
            if (!v.canConvert<QDateTime>())
                return false;

            QDateTime dt = v.toDateTime();
            if (!dt.isValid())
                return false;

            dt = dt.toUTC();
            *outUtcSeconds = dt.toMSecsSinceEpoch() / 1000.0;
            return true;
        };

    const int clusterThresholdPx = 20;

    // Style (must match existing EXIT marker visuals)
    const int pointerHeightPx = 8;
    const int pointerBaseWidthPx = 10;
    const int bubbleGapPx = 0;

    for (int laneIndex = 0; laneIndex < enabledDefs.size(); ++laneIndex) {
        const MarkerDefinition &def = enabledDefs[laneIndex];

        struct DrawableInstance {
            double xPixel = 0.0;
            double markerUtcSeconds = 0.0;
        };

        QVector<DrawableInstance> drawable;
        drawable.reserve(model->getAllSessions().size());

        for (const auto &s : model->getAllSessions()) {
            if (!s.isVisible())
                continue;

            double markerUtcSeconds = 0.0;
            if (!tryAttributeUtcSeconds(s, def.attributeKey, &markerUtcSeconds))
                continue;

            double xCoord = 0.0;

            if (m_xAxisKey == SessionKeys::Time) {
                // Absolute UTC time axis
                xCoord = markerUtcSeconds;
            } else if (m_xAxisKey == SessionKeys::TimeFromExit) {
                // Relative time-from-exit axis (must use ExitTime from the same session)
                double exitUtcSeconds = 0.0;
                if (!tryAttributeUtcSeconds(s, SessionKeys::ExitTime, &exitUtcSeconds))
                    continue;

                xCoord = markerUtcSeconds - exitUtcSeconds;
            } else {
                // Fallback: treat as absolute UTC seconds
                xCoord = markerUtcSeconds;
            }

            if (xCoord < xRange.lower || xCoord > xRange.upper)
                continue;

            DrawableInstance di;
            di.xPixel = customPlot->xAxis->coordToPixel(xCoord);
            di.markerUtcSeconds = markerUtcSeconds;
            drawable.append(di);
        }

        QVector<QPointer<QCPAbstractItem>> &laneItems = m_markerItemsByLane[laneIndex];

        if (drawable.isEmpty()) {
            if (!laneItems.isEmpty())
                clearLaneItems(laneItems);
            continue;
        }

        std::sort(drawable.begin(), drawable.end(),
                  [](const DrawableInstance &a, const DrawableInstance &b) { return a.xPixel < b.xPixel; });

        struct Cluster {
            double anchorXPixel = 0.0;
            QString label;

            double sumXPixel = 0.0;
            double lastXPixel = 0.0;
            int count = 0;

            double markerUtcSeconds = 0.0; // Only valid when count == 1
        };

        QVector<Cluster> clusters;
        clusters.reserve(drawable.size());

        Cluster current;
        current.sumXPixel = drawable.first().xPixel;
        current.lastXPixel = drawable.first().xPixel;
        current.count = 1;
        current.markerUtcSeconds = drawable.first().markerUtcSeconds;

        for (int i = 1; i < drawable.size(); ++i) {
            const double xPix = drawable[i].xPixel;
            const double utcSeconds = drawable[i].markerUtcSeconds;

            if (qAbs(xPix - current.lastXPixel) <= clusterThresholdPx) {
                current.sumXPixel += xPix;
                current.lastXPixel = xPix;
                current.count += 1;

                // Only valid for single-marker clusters.
                if (current.count != 1) {
                    current.markerUtcSeconds = 0.0;
                }

                continue;
            }

            // finalize cluster
            current.anchorXPixel = current.sumXPixel / current.count;
            current.label = (current.count == 1)
                                ? def.label
                                : QStringLiteral("%1 \u00D7%2").arg(def.label).arg(current.count);
            clusters.append(current);

            // start new cluster
            current = Cluster{};
            current.sumXPixel = xPix;
            current.lastXPixel = xPix;
            current.count = 1;
            current.markerUtcSeconds = utcSeconds;
        }

        // finalize last cluster
        current.anchorXPixel = current.sumXPixel / current.count;
        current.label = (current.count == 1)
                            ? def.label
                            : QStringLiteral("%1 \u00D7%2").arg(def.label).arg(current.count);
        clusters.append(current);

        if (clusters.isEmpty()) {
            if (!laneItems.isEmpty())
                clearLaneItems(laneItems);
            continue;
        }

        // Rebuild lane items if cluster count changed (or items are stale)
        const int requiredItemCount = clusters.size() * 2;
        bool needsRebuild = (laneItems.size() != requiredItemCount);
        if (!needsRebuild) {
            for (auto &item : laneItems) {
                if (!item) {
                    needsRebuild = true;
                    break;
                }
            }
        }
        if (needsRebuild) {
            clearLaneItems(laneItems);
        }

        // Geometry per lane (lane 0 is closest to plot)
        const int laneBottomY = axisRectPx.top() - laneIndex * laneHeightPx;
        const int bubbleBottomY = laneBottomY - pointerHeightPx - bubbleGapPx;

        // Lazily create marker items (one pointer + one bubble per cluster)
        if (laneItems.isEmpty()) {
            const QColor markerColor = def.color;

            for (int i = 0; i < clusters.size(); ++i) {
                // Pointer triangle (implemented via a flat arrow head)
                QCPItemLine *pointer = new QCPItemLine(customPlot);
                pointer->setLayer("referenceMarkers");
                pointer->setClipToAxisRect(false);
                pointer->setSelectable(false);

                QPen pointerPen(markerColor);
                pointerPen.setWidthF(0);
                pointer->setPen(pointerPen);
                pointer->setHead(QCPLineEnding(QCPLineEnding::esFlatArrow, pointerBaseWidthPx, pointerHeightPx));
                pointer->setTail(QCPLineEnding(QCPLineEnding::esNone));

                pointer->start->setType(QCPItemPosition::ptAbsolute);
                pointer->end->setType(QCPItemPosition::ptAbsolute);

                // Label bubble
                QCPItemText *bubble = new QCPItemText(customPlot);
                bubble->setLayer("referenceMarkers");
                bubble->setClipToAxisRect(false);
                bubble->setSelectable(false);

                bubble->position->setType(QCPItemPosition::ptAbsolute);
                bubble->setPositionAlignment(Qt::AlignHCenter | Qt::AlignBottom);
                bubble->setTextAlignment(Qt::AlignCenter);

                bubble->setText(def.label);
                bubble->setPadding(QMargins(6, 2, 6, 2));
                bubble->setBrush(QBrush(markerColor));
                bubble->setPen(QPen(markerColor));
                bubble->setColor(Qt::white);

                QFont f = bubble->font();
                f.setBold(true);
                bubble->setFont(f);

                laneItems << pointer << bubble;
            }
        }

        // Update item positions and labels (for panning/zooming and re-clustering)
        for (int i = 0; i < clusters.size(); ++i) {
            const double xPixel = clusters[i].anchorXPixel;

            QCPItemLine *pointer = qobject_cast<QCPItemLine *>(laneItems[2 * i].data());
            QCPItemText *bubble = qobject_cast<QCPItemText *>(laneItems[2 * i + 1].data());

            if (pointer) {
                pointer->start->setCoords(xPixel, laneBottomY - pointerHeightPx);
                pointer->end->setCoords(xPixel, axisRectPx.top());
            }
            if (bubble) {
                bubble->setText(clusters[i].label);
                bubble->position->setCoords(xPixel, bubbleBottomY);

                MarkerBubbleMeta meta;
                meta.count = clusters[i].count;
                meta.utcSeconds = (clusters[i].count == 1) ? clusters[i].markerUtcSeconds : 0.0;
                m_markerBubbleMeta.insert(bubble, meta);
            }
        }
    }
}

QCPRange PlotWidget::keyRangeOf(const SessionData& s,
                                const QString& sensor,
                                const QString& meas) const
{
    QVector<double> v =
        const_cast<SessionData&>(s).getMeasurement(sensor, meas);
    if (v.isEmpty()) return QCPRange(0,0);

    auto [minIt,maxIt] = std::minmax_element(v.begin(), v.end());
    return QCPRange(*minIt, *maxIt);
}

void PlotWidget::applyXAxisChange(const QString& key, const QString& label)
{
    if (key == m_xAxisKey)           // already on this scale
        return;

    qDebug() << "PlotWidget::applyXAxisChange - Applying Key:" << key << "Label:" << label;

    /* 1. keep a copy of the current window (old scale) */
    QCPRange oldRange = customPlot->xAxis->range();

    /* 2. locate a reference session */
    const SessionData* ref = referenceSession();
    double exitT = ref ? exitTimeSeconds(*ref) : 0.0;

    /* 3. translate the window */
    QCPRange newRange;
    if (key == SessionKeys::Time) {                  // going relative → absolute
        newRange.lower = oldRange.lower + exitT;
        newRange.upper = oldRange.upper + exitT;
    } else {                                         // absolute → relative
        newRange.lower = oldRange.lower - exitT;
        newRange.upper = oldRange.upper - exitT;
    }

    /* 4. commit the switch */
    m_xAxisKey   = key;
    m_xAxisLabel = label;
    customPlot->xAxis->setLabel(label);
    customPlot->xAxis->setRange(newRange);

    /* 5. Update x-axis ticker */
    updateXAxisTicker();

    updatePlot();                 // rebuild graphs with new x-values
    customPlot->replot();

    updateXAxisTicker();

    // Broadcast range with new axis key
    if (m_rangeModel) {
        m_rangeModel->setRange(m_xAxisKey, newRange.lower, newRange.upper);
    }

    // If an external source (e.g., map hover) is driving the cursor, re-apply it under the new axis mode.
    onCursorsChanged();
}

} // namespace FlySight
