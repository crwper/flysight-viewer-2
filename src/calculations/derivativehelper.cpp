#include "derivativehelper.h"
#include <QDebug>

namespace FlySight {
namespace Calculations {

std::optional<QVector<double>> computeDerivative(
    const QVector<double>& values,
    const QVector<double>& times)
{
    if (values.isEmpty()) {
        qWarning() << "computeDerivative: values array is empty.";
        return std::nullopt;
    }

    if (values.size() != times.size()) {
        qWarning() << "computeDerivative: values and times size mismatch.";
        return std::nullopt;
    }

    if (values.size() < 2) {
        qWarning() << "computeDerivative: not enough data points (need at least 2).";
        return std::nullopt;
    }

    QVector<double> result;
    result.reserve(values.size());

    // Forward difference for first point
    {
        double dt = times[1] - times[0];
        if (dt == 0.0) {
            qWarning() << "computeDerivative: zero time difference between indices 0 and 1.";
            return std::nullopt;
        }
        result.append((values[1] - values[0]) / dt);
    }

    // Centered difference for interior points
    for (int i = 1; i < values.size() - 1; ++i) {
        double dt = times[i + 1] - times[i - 1];
        if (dt == 0.0) {
            qWarning() << "computeDerivative: zero time difference for indices" << i - 1 << "and" << i + 1;
            return std::nullopt;
        }
        result.append((values[i + 1] - values[i - 1]) / dt);
    }

    // Backward difference for last point
    {
        int last = values.size() - 1;
        double dt = times[last] - times[last - 1];
        if (dt == 0.0) {
            qWarning() << "computeDerivative: zero time difference at end indices" << last - 1 << "and" << last;
            return std::nullopt;
        }
        result.append((values[last] - values[last - 1]) / dt);
    }

    return result;
}

} // namespace Calculations
} // namespace FlySight
