#include "plotrangemodel.h"

namespace FlySight {

PlotRangeModel::PlotRangeModel(QObject *parent)
    : QObject(parent)
{
}

void PlotRangeModel::setRange(const QString &axisKey, double lower, double upper)
{
    if (m_hasRange && m_axisKey == axisKey &&
        qFuzzyCompare(m_rangeLower, lower) &&
        qFuzzyCompare(m_rangeUpper, upper)) {
        return; // No change
    }

    m_axisKey = axisKey;
    m_rangeLower = lower;
    m_rangeUpper = upper;
    m_hasRange = true;

    emit rangeChanged();
}

void PlotRangeModel::clearRange()
{
    if (!m_hasRange)
        return;

    m_hasRange = false;
    m_axisKey.clear();
    m_rangeLower = 0.0;
    m_rangeUpper = 0.0;

    emit rangeChanged();
}

} // namespace FlySight
