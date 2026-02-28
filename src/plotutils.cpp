#include "plotutils.h"
#include "sessiondata.h"
#include "plotregistry.h"
#include "units/unitconverter.h"

#include <QDateTime>
#include <QVariant>
#include <QStringLiteral>

#include <algorithm>

namespace FlySight {

std::optional<double> markerOffsetUtcSeconds(const SessionData &session,
                                             const QString &referenceMarkerKey)
{
    if (referenceMarkerKey.isEmpty())
        return 0.0;
    QVariant v = session.getAttribute(referenceMarkerKey);
    if (!v.canConvert<QDateTime>())
        return std::nullopt;
    QDateTime dt = v.toDateTime();
    if (!dt.isValid())
        return std::nullopt;
    return dt.toMSecsSinceEpoch() / 1000.0;
}

QString seriesDisplayName(const PlotValue &pv)
{
    QString displayUnits = pv.plotUnits;
    if (!pv.measurementType.isEmpty()) {
        QString converted = UnitConverter::instance().getUnitLabel(pv.measurementType);
        if (!converted.isEmpty())
            displayUnits = converted;
    }
    if (!displayUnits.isEmpty())
        return QStringLiteral("%1 (%2)").arg(pv.plotName, displayUnits);
    return pv.plotName;
}

double interpolateAtX(const QVector<double> &xData,
                      const QVector<double> &yData,
                      double x)
{
    if (xData.isEmpty() || yData.isEmpty() || xData.size() != yData.size())
        return kNaN;

    auto it = std::lower_bound(xData.cbegin(), xData.cend(), x);
    if (it == xData.cbegin() || it == xData.cend())
        return kNaN;

    const int idx = static_cast<int>(std::distance(xData.cbegin(), it));
    const double x1 = xData[idx - 1], y1 = yData[idx - 1];
    const double x2 = xData[idx],     y2 = yData[idx];
    if (x2 == x1)
        return kNaN;
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

    const QString m = measurementId.toLower();
    if (m.contains(QStringLiteral("lat")) || m.contains(QStringLiteral("lon")))
        return QString::number(value, 'f', 6);

    if (!measurementType.isEmpty()) {
        double displayValue = UnitConverter::instance().convert(value, measurementType);
        int precision = UnitConverter::instance().getPrecision(measurementType);
        if (precision < 0) precision = 1;
        return QString::number(displayValue, 'f', precision);
    }

    int precision = 1;
    if (m.contains(QStringLiteral("time")))
        precision = 3;
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

} // namespace FlySight
