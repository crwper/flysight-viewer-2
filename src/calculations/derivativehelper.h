#ifndef DERIVATIVEHELPER_H
#define DERIVATIVEHELPER_H

#include <QVector>
#include <optional>

namespace FlySight {
namespace Calculations {

std::optional<QVector<double>> computeDerivative(
    const QVector<double>& values,
    const QVector<double>& times);

} // namespace Calculations
} // namespace FlySight

#endif // DERIVATIVEHELPER_H
