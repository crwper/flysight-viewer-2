#include "plotviewsettingsmodel.h"

#include "sessiondata.h"

namespace FlySight {

PlotViewSettingsModel::PlotViewSettingsModel(QSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    const QString savedKey = (m_settings)
        ? m_settings->value("plot/xAxisKey", SessionKeys::TimeFromExit).toString()
        : SessionKeys::TimeFromExit;

    m_xAxisKey = savedKey;
    m_xAxisLabel = defaultLabelForKey(savedKey);
}

QString PlotViewSettingsModel::xAxisKey() const
{
    return m_xAxisKey;
}

QString PlotViewSettingsModel::xAxisLabel() const
{
    return m_xAxisLabel;
}

void PlotViewSettingsModel::setXAxis(const QString &key, const QString &label)
{
    const QString normalizedKey = key;
    const QString normalizedLabel = label.isEmpty() ? defaultLabelForKey(key) : label;

    if (m_xAxisKey == normalizedKey && m_xAxisLabel == normalizedLabel)
        return;

    m_xAxisKey = normalizedKey;
    m_xAxisLabel = normalizedLabel;

    if (m_settings)
        m_settings->setValue("plot/xAxisKey", m_xAxisKey);

    emit xAxisChanged(m_xAxisKey, m_xAxisLabel);
}

QString PlotViewSettingsModel::defaultLabelForKey(const QString &key) const
{
    if (key == SessionKeys::Time)
        return tr("Time (s)");

    // Default: relative to exit
    return tr("Time from exit (s)");
}

} // namespace FlySight
