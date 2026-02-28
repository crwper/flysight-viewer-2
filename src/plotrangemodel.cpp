#include "plotrangemodel.h"

namespace FlySight {

PlotRangeModel::PlotRangeModel(QObject *parent)
    : QObject(parent)
{
}

void PlotRangeModel::setRange(const QString &xVariable, const QString &referenceMarkerKey,
                               double lower, double upper)
{
    if (m_hasRange && m_xVariable == xVariable && m_referenceMarkerKey == referenceMarkerKey &&
        qFuzzyCompare(m_rangeLower, lower) &&
        qFuzzyCompare(m_rangeUpper, upper)) {
        return; // No change
    }

    m_xVariable = xVariable;
    m_referenceMarkerKey = referenceMarkerKey;
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
    m_xVariable.clear();
    m_referenceMarkerKey.clear();
    m_rangeLower = 0.0;
    m_rangeUpper = 0.0;

    emit rangeChanged();
}

} // namespace FlySight
