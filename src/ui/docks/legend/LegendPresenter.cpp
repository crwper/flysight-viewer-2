#include "LegendPresenter.h"

#include "sessionmodel.h"
#include "plotmodel.h"
#include "cursormodel.h"
#include "plotviewsettingsmodel.h"
#include "measuremodel.h"
#include "LegendWidget.h"
#include "sessiondata.h"
#include "units/unitconverter.h"
#include "plotutils.h"

#include <QHash>
#include <QDateTime>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>

namespace FlySight {
namespace {

std::optional<double> plotAxisXForSession(const CursorModel::Cursor &cursor,
                                         const QString &xVariable,
                                         const QString &referenceMarkerKey,
                                         const SessionData &session)
{
    using PS = CursorModel::PositionSpace;

    if (cursor.positionSpace == PS::PlotAxisCoord) {
        // Match both xVariable and referenceMarkerKey
        if (cursor.xVariable != xVariable || cursor.referenceMarkerKey != referenceMarkerKey)
            return std::nullopt;
        return cursor.positionValue;
    }

    // UtcSeconds -> convert to plot-axis coordinate: plotCoord = utcSeconds - offset
    const double utc = cursor.positionValue;

    const auto optOffset = markerOffsetUtcSeconds(session, referenceMarkerKey);
    if (!optOffset.has_value())
        return std::nullopt;

    return utc - *optOffset;
}

std::optional<double> utcSecondsForHeader(const CursorModel::Cursor &cursor,
                                         const QString &referenceMarkerKey,
                                         const SessionData &session,
                                         double plotAxisX)
{
    using PS = CursorModel::PositionSpace;

    if (cursor.positionSpace == PS::UtcSeconds) {
        return cursor.positionValue;
    }

    // PlotAxisCoord -> convert to UTC: utcSeconds = plotAxisX + offset
    const auto optOffset = markerOffsetUtcSeconds(session, referenceMarkerKey);
    if (!optOffset.has_value())
        return std::nullopt;

    return plotAxisX + *optOffset;
}

bool sessionOverlapsAtX(const SessionData &session,
                        const QVector<PlotValue> &enabledPlots,
                        const QString &xVariable,
                        const QString &referenceMarkerKey,
                        double plotAxisX)
{
    // Compute offset: plot-to-raw conversion is rawX = plotAxisX + offset
    const double offset = markerOffsetUtcSeconds(session, referenceMarkerKey).value_or(0.0);
    const double rawX = plotAxisX + offset;

    for (const PlotValue &pv : enabledPlots) {
        const QVector<double> xData = session.getMeasurement(pv.sensorID, xVariable);
        const QVector<double> yData = session.getMeasurement(pv.sensorID, pv.measurementID);

        if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
            continue;
        }

        auto minmax = std::minmax_element(xData.cbegin(), xData.cend());
        if (minmax.first == xData.cend() || minmax.second == xData.cend()) {
            continue;
        }

        if (rawX >= *minmax.first && rawX <= *minmax.second) {
            return true;
        }
    }

    return false;
}

} // namespace

LegendPresenter::LegendPresenter(SessionModel *sessionModel,
                                 PlotModel *plotModel,
                                 CursorModel *cursorModel,
                                 PlotViewSettingsModel *plotViewSettings,
                                 MeasureModel *measureModel,
                                 LegendWidget *legendWidget,
                                 QObject *parent)
    : QObject(parent)
    , m_sessionModel(sessionModel)
    , m_plotModel(plotModel)
    , m_cursorModel(cursorModel)
    , m_plotViewSettings(plotViewSettings)
    , m_measureModel(measureModel)
    , m_legendWidget(legendWidget)
{
    m_updateTimer.setSingleShot(true);
    m_updateTimer.setInterval(0);

    connect(&m_updateTimer, &QTimer::timeout,
            this, &LegendPresenter::recompute);

    if (m_sessionModel) {
        connect(m_sessionModel, &SessionModel::modelChanged,
                this, &LegendPresenter::scheduleUpdate);
    }

    if (m_plotModel) {
        connect(m_plotModel, &QAbstractItemModel::modelReset,
                this, &LegendPresenter::scheduleUpdate);
        connect(m_plotModel, &QAbstractItemModel::dataChanged,
                this, &LegendPresenter::scheduleUpdate);
    }

    if (m_cursorModel) {
        connect(m_cursorModel, &CursorModel::cursorsChanged,
                this, &LegendPresenter::scheduleUpdate);
    }

    if (m_plotViewSettings) {
        connect(m_plotViewSettings, &PlotViewSettingsModel::xVariableChanged,
                this, &LegendPresenter::scheduleUpdate);
        connect(m_plotViewSettings, &PlotViewSettingsModel::referenceMarkerKeyChanged,
                this, &LegendPresenter::scheduleUpdate);
    }

    if (m_measureModel) {
        connect(m_measureModel, &MeasureModel::dataChanged,
                this, &LegendPresenter::scheduleUpdate);
    }

    // Connect to unit system changes for reactive updates
    connect(&UnitConverter::instance(), &UnitConverter::systemChanged,
            this, &LegendPresenter::scheduleUpdate);

    if (m_legendWidget) {
        m_legendWidget->clear();
    }

    scheduleUpdate();
}

void LegendPresenter::scheduleUpdate()
{
    if (m_updateTimer.isActive())
        return;

    m_updateTimer.start();
}

void LegendPresenter::recompute()
{
    if (!m_legendWidget) {
        return;
    }

    // If the measure tool is actively dragging, it overrides the legend display.
    if (m_measureModel && m_measureModel->isActive()) {
        const auto &mRows = m_measureModel->rows();

        QVector<LegendWidget::Row> widgetRows;
        widgetRows.reserve(mRows.size());
        for (const auto &mr : mRows) {
            LegendWidget::Row wr;
            wr.name       = mr.name;
            wr.color      = mr.color;
            wr.deltaValue = mr.deltaValue;
            wr.value      = mr.finalValue;
            wr.minValue   = mr.minValue;
            wr.avgValue   = mr.avgValue;
            wr.maxValue   = mr.maxValue;
            widgetRows.push_back(wr);
        }

        if (m_measureModel->isMultiTrack()) {
            // Multi-track: show only Min/Avg/Max, no header.
            m_legendWidget->setMode(LegendWidget::RangeStatsMode);
            m_legendWidget->setRows(widgetRows);
        } else {
            // Single-track: full measure layout with header.
            m_legendWidget->setMode(LegendWidget::MeasureMode);
            m_legendWidget->setHeader(m_measureModel->sessionDesc(),
                                      m_measureModel->utcText(),
                                      m_measureModel->coordsText());
            m_legendWidget->setRows(widgetRows);
        }
        return;
    }

    if (!m_sessionModel || !m_plotModel || !m_cursorModel || !m_plotViewSettings) {
        m_legendWidget->clear();
        return;
    }

    const QString xVariable = m_plotViewSettings->xVariable();
    const QString referenceMarkerKey = m_plotViewSettings->referenceMarkerKey();

    const QVector<PlotValue> enabledPlots = m_plotModel->enabledPlots();
    if (enabledPlots.isEmpty()) {
        m_legendWidget->clear();
        return;
    }

    const CursorModel::Cursor cursor = chooseEffectiveCursor(m_cursorModel);
    if (cursor.id.isEmpty() || !cursor.active) {
        m_legendWidget->clear();
        return;
    }

    // If cursor is in plot-axis space, it must match the current axis mode.
    if (cursor.positionSpace == CursorModel::PositionSpace::PlotAxisCoord &&
        (cursor.xVariable != xVariable ||
         cursor.referenceMarkerKey != referenceMarkerKey)) {
        m_legendWidget->clear();
        return;
    }

    const QVector<SessionData> &sessions = m_sessionModel->getAllSessions();

    // Build a fast id -> session lookup.
    QHash<QString, const SessionData *> sessionById;
    sessionById.reserve(sessions.size());
    for (const auto &s : sessions) {
        const QString sid = s.getAttribute(SessionKeys::SessionId).toString();
        if (!sid.isEmpty()) {
            sessionById.insert(sid, &s);
        }
    }

    // Resolve target sessions.
    QSet<QString> targets;
    const bool explicitTargets =
        (cursor.targetPolicy == CursorModel::TargetPolicy::Explicit && !cursor.targetSessions.isEmpty());

    if (explicitTargets) {
        for (const QString &sid : cursor.targetSessions) {
            auto it = sessionById.constFind(sid);
            if (it == sessionById.constEnd())
                continue;
            if (!it.value()->isVisible())
                continue;
            targets.insert(sid);
        }
    } else {
        // Auto-visible-overlap (or explicit with empty targets): derive from visible sessions.
        for (const auto &s : sessions) {
            if (!s.isVisible())
                continue;

            const QString sid = s.getAttribute(SessionKeys::SessionId).toString();
            if (sid.isEmpty())
                continue;

            const auto xOpt = plotAxisXForSession(cursor, xVariable, referenceMarkerKey, s);
            if (!xOpt.has_value())
                continue;

            if (sessionOverlapsAtX(s, enabledPlots, xVariable, referenceMarkerKey, *xOpt)) {
                targets.insert(sid);
            }
        }
    }

    if (targets.isEmpty()) {
        m_legendWidget->clear();
        return;
    }

    // Helper: compute the reference offset for a given session
    auto offsetForSession = [&referenceMarkerKey](const SessionData &s) -> double {
        return markerOffsetUtcSeconds(s, referenceMarkerKey).value_or(0.0);
    };

    const bool pointMode = (targets.size() == 1);

    QVector<LegendWidget::Row> rows;
    rows.reserve(enabledPlots.size());

    bool hasData = false;

    if (pointMode) {
        const QString targetSessionId = *targets.constBegin();
        const SessionData *session = sessionById.value(targetSessionId, nullptr);
        if (!session) {
            m_legendWidget->clear();
            return;
        }

        const auto xOpt = plotAxisXForSession(cursor, xVariable, referenceMarkerKey, *session);
        if (!xOpt.has_value()) {
            m_legendWidget->clear();
            return;
        }
        const double plotX = *xOpt;

        // Convert plot coordinate to raw data space for interpolation
        const double offset = offsetForSession(*session);
        const double rawX = plotX + offset;

        // Rows
        for (const PlotValue &pv : enabledPlots) {
            LegendWidget::Row row;
            row.name = seriesDisplayName(pv);
            row.color = pv.defaultColor;

            const double v = interpolateSessionMeasurement(*session, pv.sensorID, xVariable, pv.measurementID, rawX);
            if (!std::isnan(v)) {
                hasData = true;
            }

            row.value = formatValue(v, pv.measurementID, pv.measurementType);
            rows.push_back(row);
        }

        if (!hasData) {
            m_legendWidget->clear();
            return;
        }

        // Header
        QString sessionDesc = session->getAttribute(SessionKeys::Description).toString();
        if (sessionDesc.isEmpty())
            sessionDesc = targetSessionId;

        QString utcText;
        QString coordsText;

        const auto utcOpt = utcSecondsForHeader(cursor, referenceMarkerKey, *session, plotX);
        if (utcOpt.has_value()) {
            const double utcSecs = *utcOpt;

            utcText = QStringLiteral("%1 UTC")
                          .arg(QDateTime::fromMSecsSinceEpoch(
                                   qint64(utcSecs * 1000.0),
                                   Qt::UTC)
                                   .toString(QStringLiteral("yy-MM-dd HH:mm:ss.zzz")));

            const double lat = interpolateSessionMeasurement(*session, QStringLiteral("GNSS"), SessionKeys::Time, QStringLiteral("lat"), utcSecs);
            const double lon = interpolateSessionMeasurement(*session, QStringLiteral("GNSS"), SessionKeys::Time, QStringLiteral("lon"), utcSecs);
            const double alt = interpolateSessionMeasurement(*session, QStringLiteral("GNSS"), SessionKeys::Time, QStringLiteral("hMSL"), utcSecs);

            if (!std::isnan(lat) && !std::isnan(lon) && !std::isnan(alt)) {
                // Convert altitude to display units
                double displayAlt = UnitConverter::instance().convert(alt, QStringLiteral("altitude"));
                QString altUnit = UnitConverter::instance().getUnitLabel(QStringLiteral("altitude"));
                int altPrecision = UnitConverter::instance().getPrecision(QStringLiteral("altitude"));
                if (altPrecision < 0) altPrecision = 1; // Fallback

                coordsText = QStringLiteral("(%1 deg, %2 deg, %3 %4)")
                                 .arg(lat, 0, 'f', 7)
                                 .arg(lon, 0, 'f', 7)
                                 .arg(displayAlt, 0, 'f', altPrecision)
                                 .arg(altUnit);
            }
        }

        m_legendWidget->setMode(LegendWidget::PointDataMode);
        m_legendWidget->setHeader(sessionDesc, utcText, coordsText);
        m_legendWidget->setRows(rows);

        return;
    }

    // RangeStatsMode
    for (const PlotValue &pv : enabledPlots) {
        LegendWidget::Row row;
        row.name = seriesDisplayName(pv);
        row.color = pv.defaultColor;

        QVector<double> valuesAtCursor;

        for (const QString &sid : targets) {
            const SessionData *session = sessionById.value(sid, nullptr);
            if (!session)
                continue;

            const auto xOpt = plotAxisXForSession(cursor, xVariable, referenceMarkerKey, *session);
            if (!xOpt.has_value())
                continue;

            // Convert plot coordinate to raw data space for interpolation
            const double offset = offsetForSession(*session);
            const double rawX = *xOpt + offset;

            const double v = interpolateSessionMeasurement(*session, pv.sensorID, xVariable, pv.measurementID, rawX);
            if (!std::isnan(v)) {
                valuesAtCursor.append(v);
            }
        }

        if (!valuesAtCursor.isEmpty()) {
            hasData = true;

            const double minVal = *std::min_element(valuesAtCursor.begin(), valuesAtCursor.end());
            const double maxVal = *std::max_element(valuesAtCursor.begin(), valuesAtCursor.end());
            const double avgVal = std::accumulate(valuesAtCursor.begin(), valuesAtCursor.end(), 0.0)
                                    / valuesAtCursor.size();

            row.minValue = formatValue(minVal, pv.measurementID, pv.measurementType);
            row.avgValue = formatValue(avgVal, pv.measurementID, pv.measurementType);
            row.maxValue = formatValue(maxVal, pv.measurementID, pv.measurementType);
        } else {
            row.minValue = QStringLiteral("--");
            row.avgValue = QStringLiteral("--");
            row.maxValue = QStringLiteral("--");
        }

        rows.push_back(row);
    }

    if (!hasData) {
        m_legendWidget->clear();
        return;
    }

    m_legendWidget->setMode(LegendWidget::RangeStatsMode);
    m_legendWidget->setRows(rows);
}

} // namespace FlySight
