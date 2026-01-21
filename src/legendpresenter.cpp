#include "legendpresenter.h"

#include "sessionmodel.h"
#include "plotmodel.h"
#include "cursormodel.h"
#include "plotviewsettingsmodel.h"
#include "legendwidget.h"
#include "sessiondata.h"
#include "units/unitconverter.h"

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

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QString seriesDisplayName(const PlotValue &pv)
{
    QString displayUnits = pv.plotUnits; // Fallback

    if (!pv.measurementType.isEmpty()) {
        QString convertedLabel = UnitConverter::instance().getUnitLabel(pv.measurementType);
        if (!convertedLabel.isEmpty()) {
            displayUnits = convertedLabel;
        }
    }

    if (!displayUnits.isEmpty()) {
        return QString("%1 (%2)").arg(pv.plotName).arg(displayUnits);
    }
    return pv.plotName;
}

// Match PlotWidget::interpolateY semantics (NaN at ends / degenerate segment).
double interpolateAtX(const QVector<double> &xData,
                      const QVector<double> &yData,
                      double x)
{
    if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
        return kNaN;
    }

    auto itLower = std::lower_bound(xData.cbegin(), xData.cend(), x);

    if (itLower == xData.cbegin() || itLower == xData.cend()) {
        return kNaN;
    }

    const int index = static_cast<int>(std::distance(xData.cbegin(), itLower));

    const double x1 = xData[index - 1];
    const double y1 = yData[index - 1];
    const double x2 = xData[index];
    const double y2 = yData[index];

    if (x2 == x1) {
        return kNaN;
    }

    return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

double interpolateSessionMeasurement(const SessionData &session,
                                    const QString &sensorId,
                                    const QString &xAxisKey,
                                    const QString &measurementId,
                                    double x)
{
    const QVector<double> xData = session.getMeasurement(sensorId, xAxisKey);
    const QVector<double> yData = session.getMeasurement(sensorId, measurementId);
    return interpolateAtX(xData, yData, x);
}

QString formatValue(double value, const QString &measurementId, const QString &measurementType)
{
    if (std::isnan(value))
        return QStringLiteral("--");

    // Preserve coordinate precision regardless of unit system (per constraints)
    const QString m = measurementId.toLower();
    if (m.contains(QStringLiteral("lat")) || m.contains(QStringLiteral("lon"))) {
        return QString::number(value, 'f', 6);
    }

    // Apply unit conversion if measurementType is specified
    if (!measurementType.isEmpty()) {
        double displayValue = UnitConverter::instance().convert(value, measurementType);
        int precision = UnitConverter::instance().getPrecision(measurementType);
        if (precision < 0) precision = 1; // Fallback
        return QString::number(displayValue, 'f', precision);
    }

    // Legacy fallback: use measurement-based heuristics
    int precision = 1;
    if (m.contains(QStringLiteral("time"))) {
        precision = 3;
    }

    return QString::number(value, 'f', precision);
}

CursorModel::Cursor chooseEffectiveCursor(const CursorModel *cursorModel)
{
    if (!cursorModel)
        return CursorModel::Cursor{};

    // 1) Prefer mouse cursor when active and it has usable targets
    if (cursorModel->hasCursor(QStringLiteral("mouse"))) {
        const CursorModel::Cursor mouse = cursorModel->cursorById(QStringLiteral("mouse"));
        if (mouse.active) {
            if (mouse.targetPolicy == CursorModel::TargetPolicy::Explicit && !mouse.targetSessions.isEmpty()) {
                return mouse;
            }
            // If explicit targets are empty, treat as not usable and fall through.
        }
    }

    // 2) Otherwise, first active non-mouse cursor
    const int n = cursorModel->rowCount();
    for (int row = 0; row < n; ++row) {
        const QModelIndex idx = cursorModel->index(row, 0);
        const QString id = cursorModel->data(idx, CursorModel::IdRole).toString();
        if (id.isEmpty() || id == QStringLiteral("mouse"))
            continue;

        const CursorModel::Cursor c = cursorModel->cursorById(id);
        if (c.active)
            return c;
    }

    // 3) None
    return CursorModel::Cursor{};
}

std::optional<double> plotAxisXForSession(const CursorModel::Cursor &cursor,
                                         const QString &axisKey,
                                         const SessionData &session)
{
    using PS = CursorModel::PositionSpace;

    if (cursor.positionSpace == PS::PlotAxisCoord) {
        if (cursor.axisKey != axisKey)
            return std::nullopt;
        return cursor.positionValue;
    }

    // UtcSeconds → depends on current axis mode
    const double utc = cursor.positionValue;

    if (axisKey == SessionKeys::Time) {
        return utc;
    }

    if (axisKey == SessionKeys::TimeFromExit) {
        QVariant v = session.getAttribute(SessionKeys::ExitTime);
        if (!v.canConvert<QDateTime>())
            return std::nullopt;

        const QDateTime dt = v.toDateTime();
        if (!dt.isValid())
            return std::nullopt;

        const double exitUtc = dt.toMSecsSinceEpoch() / 1000.0;
        return utc - exitUtc;
    }

    return std::nullopt;
}

std::optional<double> utcSecondsForHeader(const CursorModel::Cursor &cursor,
                                         const QString &axisKey,
                                         const SessionData &session,
                                         double plotAxisX)
{
    using PS = CursorModel::PositionSpace;

    if (cursor.positionSpace == PS::UtcSeconds) {
        return cursor.positionValue;
    }

    // PlotAxisCoord → depends on axisKey
    if (axisKey == SessionKeys::Time) {
        return plotAxisX;
    }

    if (axisKey == SessionKeys::TimeFromExit) {
        QVariant v = session.getAttribute(SessionKeys::ExitTime);
        if (!v.canConvert<QDateTime>())
            return std::nullopt;

        const QDateTime dt = v.toDateTime();
        if (!dt.isValid())
            return std::nullopt;

        const double exitUtc = dt.toMSecsSinceEpoch() / 1000.0;
        return exitUtc + plotAxisX;
    }

    return std::nullopt;
}

bool sessionOverlapsAtX(const SessionData &session,
                        const QVector<PlotValue> &enabledPlots,
                        const QString &axisKey,
                        double plotAxisX)
{
    for (const PlotValue &pv : enabledPlots) {
        const QVector<double> xData = session.getMeasurement(pv.sensorID, axisKey);
        const QVector<double> yData = session.getMeasurement(pv.sensorID, pv.measurementID);

        if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size()) {
            continue;
        }

        auto minmax = std::minmax_element(xData.cbegin(), xData.cend());
        if (minmax.first == xData.cend() || minmax.second == xData.cend()) {
            continue;
        }

        if (plotAxisX >= *minmax.first && plotAxisX <= *minmax.second) {
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
                                 LegendWidget *legendWidget,
                                 QObject *parent)
    : QObject(parent)
    , m_sessionModel(sessionModel)
    , m_plotModel(plotModel)
    , m_cursorModel(cursorModel)
    , m_plotViewSettings(plotViewSettings)
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
        connect(m_plotViewSettings, &PlotViewSettingsModel::xAxisChanged,
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

    if (!m_sessionModel || !m_plotModel || !m_cursorModel || !m_plotViewSettings) {
        m_legendWidget->clear();
        return;
    }

    const QString axisKey = m_plotViewSettings->xAxisKey();

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

    // If cursor is in plot-axis space, it must match the current axis.
    if (cursor.positionSpace == CursorModel::PositionSpace::PlotAxisCoord && cursor.axisKey != axisKey) {
        m_legendWidget->clear();
        return;
    }

    const QVector<SessionData> &sessions = m_sessionModel->getAllSessions();

    // Build a fast id → session lookup.
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

            const auto xOpt = plotAxisXForSession(cursor, axisKey, s);
            if (!xOpt.has_value())
                continue;

            if (sessionOverlapsAtX(s, enabledPlots, axisKey, *xOpt)) {
                targets.insert(sid);
            }
        }
    }

    if (targets.isEmpty()) {
        m_legendWidget->clear();
        return;
    }

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

        const auto xOpt = plotAxisXForSession(cursor, axisKey, *session);
        if (!xOpt.has_value()) {
            m_legendWidget->clear();
            return;
        }
        const double x = *xOpt;

        // Rows
        for (const PlotValue &pv : enabledPlots) {
            LegendWidget::Row row;
            row.name = seriesDisplayName(pv);
            row.color = pv.defaultColor;

            const double v = interpolateSessionMeasurement(*session, pv.sensorID, axisKey, pv.measurementID, x);
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

        const auto utcOpt = utcSecondsForHeader(cursor, axisKey, *session, x);
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

            const auto xOpt = plotAxisXForSession(cursor, axisKey, *session);
            if (!xOpt.has_value())
                continue;

            const double v = interpolateSessionMeasurement(*session, pv.sensorID, axisKey, pv.measurementID, *xOpt);
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
