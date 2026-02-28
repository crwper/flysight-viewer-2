#include "measuretool.h"
#include "../measuremodel.h"
#include "../plotmodel.h"
#include "../sessiondata.h"
#include "../crosshairmanager.h"
#include "../units/unitconverter.h"
#include "../plotutils.h"

#include <QDateTime>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace FlySight {
namespace {

// Convert plot-axis X to UTC seconds for header display.
double plotAxisXToUtcSeconds(double plotAxisX,
                             const QString &referenceMarkerKey,
                             const SessionData &session)
{
    const auto optOffset = markerOffsetUtcSeconds(session, referenceMarkerKey);
    if (!optOffset.has_value())
        return kNaN;
    return plotAxisX + *optOffset;
}

} // anonymous namespace

// -----------------------------------------------------------------------

MeasureTool::MeasureTool(const PlotWidget::PlotContext &ctx)
    : m_widget(ctx.widget)
    , m_plot(ctx.plot)
    , m_graphMap(ctx.graphMap)
    , m_model(ctx.model)
    , m_plotModel(ctx.plotModel)
    , m_measureModel(ctx.measureModel)
    , m_rect(new QCPItemRect(ctx.plot))
    , m_lineLeft(new QCPItemLine(ctx.plot))
    , m_lineRight(new QCPItemLine(ctx.plot))
{
    m_rect->setVisible(false);
    m_rect->setPen(Qt::NoPen);
    m_rect->setBrush(QColor(0, 120, 215, 40));

    m_lineLeft->setVisible(false);
    m_lineRight->setVisible(false);
    applyLinePenFromPreferences();
}

bool MeasureTool::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return false;
    if (!m_plot->axisRect()->rect().contains(event->pos()))
        return false;

    const bool shiftHeld = event->modifiers().testFlag(Qt::ShiftModifier);

    if (shiftHeld) {
        // Multi-track mode: measure across all visible tracks.
        m_multiTrack = true;
        m_lockedSessionId.clear();
    } else {
        // Single-track mode: lock to the focused (hovered) session.
        m_multiTrack = false;
        m_lockedSessionId = m_model->hoveredSessionId();
        if (m_lockedSessionId.isEmpty())
            return false;
        m_widget->lockFocusToSession(m_lockedSessionId);
    }

    m_measuring  = true;
    m_startPixel = event->pos();
    m_startX     = m_plot->xAxis->pixelToCoord(event->pos().x());

    // Show the overlay.
    double yLow  = m_plot->yAxis->range().lower;
    double yHigh = m_plot->yAxis->range().upper;
    m_rect->topLeft->setCoords(m_startX, yHigh);
    m_rect->bottomRight->setCoords(m_startX, yLow);
    m_rect->setVisible(true);

    applyLinePenFromPreferences();
    m_lineLeft->start->setCoords(m_startX, yLow);
    m_lineLeft->end->setCoords(m_startX, yHigh);
    m_lineLeft->setVisible(true);
    m_lineRight->start->setCoords(m_startX, yLow);
    m_lineRight->end->setCoords(m_startX, yHigh);
    m_lineRight->setVisible(true);

    m_plot->replot(QCustomPlot::rpQueuedReplot);

    return true;
}

bool MeasureTool::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_measuring)
        return false;

    updateMeasurement(event->pos());
    return true;
}

bool MeasureTool::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_measuring || event->button() != Qt::LeftButton)
        return false;

    m_measuring = false;
    m_multiTrack = false;
    m_rect->setVisible(false);
    m_lineLeft->setVisible(false);
    m_lineRight->setVisible(false);
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    m_widget->unlockFocus();

    if (m_measureModel)
        m_measureModel->clear();

    return true;
}

bool MeasureTool::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    // If dragging and mouse leaves the widget, keep the measurement active —
    // it will update when the mouse re-enters or on release.
    return false;
}

void MeasureTool::closeTool()
{
    if (m_measuring) {
        m_measuring = false;
        m_multiTrack = false;
        m_rect->setVisible(false);
        m_lineLeft->setVisible(false);
        m_lineRight->setVisible(false);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
        m_widget->unlockFocus();
    }
    if (m_measureModel)
        m_measureModel->clear();
}

// -----------------------------------------------------------------------

void MeasureTool::updateMeasurement(const QPoint &currentPixel)
{
    if (!m_measureModel || !m_plotModel)
        return;

    const double currentX = m_plot->xAxis->pixelToCoord(currentPixel.x());

    // Update the visual overlay.
    {
        double xLeft  = qMin(m_startX, currentX);
        double xRight = qMax(m_startX, currentX);
        double yLow   = m_plot->yAxis->range().lower;
        double yHigh  = m_plot->yAxis->range().upper;

        m_rect->topLeft->setCoords(xLeft, yHigh);
        m_rect->bottomRight->setCoords(xRight, yLow);
        m_rect->setVisible(true);

        applyLinePenFromPreferences();
        m_lineLeft->start->setCoords(xLeft, yLow);
        m_lineLeft->end->setCoords(xLeft, yHigh);
        m_lineLeft->setVisible(true);
        m_lineRight->start->setCoords(xRight, yLow);
        m_lineRight->end->setCoords(xRight, yHigh);
        m_lineRight->setVisible(true);

        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }

    const QString xVariable = m_widget->xVariable();
    const QString referenceMarkerKey = m_widget->referenceMarkerKey();
    const QVector<PlotValue> enabledPlots = m_plotModel->enabledPlots();

    if (enabledPlots.isEmpty()) {
        m_measureModel->clear();
        return;
    }

    const double xLo = qMin(m_startX, currentX);
    const double xHi = qMax(m_startX, currentX);

    // Helper: compute the reference offset for a given session
    auto offsetForSession = [&referenceMarkerKey](const SessionData &s) -> double {
        return markerOffsetUtcSeconds(s, referenceMarkerKey).value_or(0.0);
    };

    if (m_multiTrack) {
        // ---- Multi-track: min/avg/max across all visible sessions ----

        // Collect visible session IDs from the graph map.
        QSet<QString> visibleSessionIds;
        for (auto it = m_graphMap->begin(); it != m_graphMap->end(); ++it) {
            QCPGraph *g = it.key();
            if (g && g->visible())
                visibleSessionIds.insert(it.value().sessionId);
        }

        // Build session lookup.
        QHash<QString, const SessionData *> sessionById;
        for (const auto &s : m_model->getAllSessions()) {
            const QString sid = s.getAttribute(SessionKeys::SessionId).toString();
            if (visibleSessionIds.contains(sid))
                sessionById.insert(sid, &s);
        }

        if (sessionById.isEmpty()) {
            m_measureModel->clear();
            return;
        }

        QVector<MeasureModel::Row> rows;
        rows.reserve(enabledPlots.size());
        bool hasData = false;

        for (const PlotValue &pv : enabledPlots) {
            MeasureModel::Row row;
            row.name  = seriesDisplayName(pv);
            row.color = pv.defaultColor;

            // Collect samples from ALL visible sessions.
            QVector<double> samples;

            for (auto it = sessionById.constBegin(); it != sessionById.constEnd(); ++it) {
                const SessionData *session = it.value();
                const double offset = offsetForSession(*session);

                // Convert plot-space bounds to raw data space
                const double rawLo = xLo + offset;
                const double rawHi = xHi + offset;

                const QVector<double> xData = session->getMeasurement(pv.sensorID, xVariable);
                const QVector<double> yData = session->getMeasurement(pv.sensorID, pv.measurementID);

                // Interpolated endpoints
                const double yAtLo = interpolateAtX(xData, yData, rawLo);
                const double yAtHi = interpolateAtX(xData, yData, rawHi);
                if (!std::isnan(yAtLo)) samples.append(yAtLo);
                if (!std::isnan(yAtHi)) samples.append(yAtHi);

                // Interior data points
                if (!xData.isEmpty() && xData.size() == yData.size()) {
                    auto itBegin = std::lower_bound(xData.cbegin(), xData.cend(), rawLo);
                    auto itEnd   = std::upper_bound(xData.cbegin(), xData.cend(), rawHi);
                    for (auto jt = itBegin; jt != itEnd; ++jt) {
                        int i = static_cast<int>(std::distance(xData.cbegin(), jt));
                        double y = yData[i];
                        if (!std::isnan(y))
                            samples.append(y);
                    }
                }
            }

            if (samples.isEmpty()) {
                row.minValue = QStringLiteral("--");
                row.avgValue = QStringLiteral("--");
                row.maxValue = QStringLiteral("--");
            } else {
                hasData = true;
                double minVal = *std::min_element(samples.begin(), samples.end());
                double maxVal = *std::max_element(samples.begin(), samples.end());
                double avgVal = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();

                row.minValue = formatValue(minVal, pv.measurementID, pv.measurementType);
                row.avgValue = formatValue(avgVal, pv.measurementID, pv.measurementType);
                row.maxValue = formatValue(maxVal, pv.measurementID, pv.measurementType);
            }

            rows.push_back(row);
        }

        if (!hasData) {
            m_measureModel->clear();
            return;
        }

        m_measureModel->setData(QString(), QString(), QString(), rows, /*multiTrack=*/true);
        return;
    }

    // ---- Single-track: locked session ----

    // Find the locked session.
    const SessionData *session = nullptr;
    for (const auto &s : m_model->getAllSessions()) {
        if (s.getAttribute(SessionKeys::SessionId).toString() == m_lockedSessionId) {
            session = &s;
            break;
        }
    }
    if (!session) {
        m_measureModel->clear();
        return;
    }

    // Compute offset for raw data space conversion
    const double offset = offsetForSession(*session);
    const double rawStartX  = m_startX + offset;
    const double rawCurrentX = currentX + offset;
    const double rawLo = qMin(rawStartX, rawCurrentX);
    const double rawHi = qMax(rawStartX, rawCurrentX);

    // Build rows for each enabled plot value.
    QVector<MeasureModel::Row> rows;
    rows.reserve(enabledPlots.size());
    bool hasData = false;

    for (const PlotValue &pv : enabledPlots) {
        MeasureModel::Row row;
        row.name  = seriesDisplayName(pv);
        row.color = pv.defaultColor;

        const QVector<double> xData = session->getMeasurement(pv.sensorID, xVariable);
        const QVector<double> yData = session->getMeasurement(pv.sensorID, pv.measurementID);

        const double initialVal = interpolateAtX(xData, yData, rawStartX);
        const double finalVal   = interpolateAtX(xData, yData, rawCurrentX);

        if (std::isnan(initialVal) && std::isnan(finalVal)) {
            row.deltaValue = QStringLiteral("--");
            row.finalValue = QStringLiteral("--");
            row.minValue   = QStringLiteral("--");
            row.avgValue   = QStringLiteral("--");
            row.maxValue   = QStringLiteral("--");
            rows.push_back(row);
            continue;
        }

        hasData = true;

        // Delta = final - initial
        if (!std::isnan(initialVal) && !std::isnan(finalVal))
            row.deltaValue = formatValue(finalVal - initialVal, pv.measurementID, pv.measurementType);
        else
            row.deltaValue = QStringLiteral("--");

        row.finalValue = formatValue(finalVal, pv.measurementID, pv.measurementType);

        // Compute min / avg / max across the range [rawLo, rawHi].
        // Collect: interpolated endpoints + all interior data points.
        QVector<double> samples;

        // Interpolated endpoints
        const double yAtLo = interpolateAtX(xData, yData, rawLo);
        const double yAtHi = interpolateAtX(xData, yData, rawHi);
        if (!std::isnan(yAtLo)) samples.append(yAtLo);
        if (!std::isnan(yAtHi)) samples.append(yAtHi);

        // Interior data points
        if (!xData.isEmpty() && xData.size() == yData.size()) {
            auto itBegin = std::lower_bound(xData.cbegin(), xData.cend(), rawLo);
            auto itEnd   = std::upper_bound(xData.cbegin(), xData.cend(), rawHi);
            for (auto it = itBegin; it != itEnd; ++it) {
                int i = static_cast<int>(std::distance(xData.cbegin(), it));
                double y = yData[i];
                if (!std::isnan(y))
                    samples.append(y);
            }
        }

        if (samples.isEmpty()) {
            row.minValue = QStringLiteral("--");
            row.avgValue = QStringLiteral("--");
            row.maxValue = QStringLiteral("--");
        } else {
            double minVal = *std::min_element(samples.begin(), samples.end());
            double maxVal = *std::max_element(samples.begin(), samples.end());
            double avgVal = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();

            row.minValue = formatValue(minVal, pv.measurementID, pv.measurementType);
            row.avgValue = formatValue(avgVal, pv.measurementID, pv.measurementType);
            row.maxValue = formatValue(maxVal, pv.measurementID, pv.measurementType);
        }

        rows.push_back(row);
    }

    if (!hasData) {
        m_measureModel->clear();
        return;
    }

    // Header: session description + UTC + coordinates at the *current* cursor position.
    QString sessionDesc = session->getAttribute(SessionKeys::Description).toString();
    if (sessionDesc.isEmpty())
        sessionDesc = m_lockedSessionId;

    QString utcText;
    QString coordsText;

    const double utcSecs = plotAxisXToUtcSeconds(currentX, referenceMarkerKey, *session);
    if (!std::isnan(utcSecs)) {
        utcText = QStringLiteral("%1 UTC")
                      .arg(QDateTime::fromMSecsSinceEpoch(
                               qint64(utcSecs * 1000.0), Qt::UTC)
                               .toString(QStringLiteral("yy-MM-dd HH:mm:ss.zzz")));

        const double lat = interpolateSessionMeasurement(
            *session, QStringLiteral("GNSS"), SessionKeys::Time, QStringLiteral("lat"), utcSecs);
        const double lon = interpolateSessionMeasurement(
            *session, QStringLiteral("GNSS"), SessionKeys::Time, QStringLiteral("lon"), utcSecs);
        const double alt = interpolateSessionMeasurement(
            *session, QStringLiteral("GNSS"), SessionKeys::Time, QStringLiteral("hMSL"), utcSecs);

        if (!std::isnan(lat) && !std::isnan(lon) && !std::isnan(alt)) {
            double displayAlt = UnitConverter::instance().convert(alt, QStringLiteral("altitude"));
            QString altUnit   = UnitConverter::instance().getUnitLabel(QStringLiteral("altitude"));
            int altPrecision  = UnitConverter::instance().getPrecision(QStringLiteral("altitude"));
            if (altPrecision < 0) altPrecision = 1;

            coordsText = QStringLiteral("(%1 deg, %2 deg, %3 %4)")
                             .arg(lat, 0, 'f', 7)
                             .arg(lon, 0, 'f', 7)
                             .arg(displayAlt, 0, 'f', altPrecision)
                             .arg(altUnit);
        }
    }

    m_measureModel->setData(sessionDesc, utcText, coordsText, rows);
}

void MeasureTool::applyLinePenFromPreferences()
{
    auto &prefs = PreferencesManager::instance();
    QColor color = prefs.getValue(PreferenceKeys::PlotsCrosshairColor).value<QColor>();
    if (!color.isValid())
        color = Qt::gray;
    double thickness = prefs.getValue(PreferenceKeys::PlotsCrosshairThickness).toDouble();
    if (thickness <= 0)
        thickness = 1.0;

    QPen pen(color, thickness, Qt::DashLine);
    m_lineLeft->setPen(pen);
    m_lineRight->setPen(pen);
}

} // namespace FlySight
