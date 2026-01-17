#ifndef PLOTRANGEMODEL_H
#define PLOTRANGEMODEL_H

#include <QObject>
#include <QString>

namespace FlySight {

/**
 * Broadcasts the current plot x-axis range to interested components.
 *
 * When the user zooms or pans the plot, the visible range is broadcast
 * via this model. Other widgets (like TrackMapModel) can connect to
 * rangeChanged() and filter their data accordingly.
 *
 * The range is expressed in the current axis coordinate system:
 * - SessionKeys::Time: UTC seconds since epoch
 * - SessionKeys::TimeFromExit: Seconds relative to session exit time
 */
class PlotRangeModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double rangeLower READ rangeLower NOTIFY rangeChanged)
    Q_PROPERTY(double rangeUpper READ rangeUpper NOTIFY rangeChanged)
    Q_PROPERTY(QString axisKey READ axisKey NOTIFY rangeChanged)
    Q_PROPERTY(bool hasRange READ hasRange NOTIFY rangeChanged)

public:
    explicit PlotRangeModel(QObject *parent = nullptr);

    double rangeLower() const { return m_rangeLower; }
    double rangeUpper() const { return m_rangeUpper; }
    QString axisKey() const { return m_axisKey; }
    bool hasRange() const { return m_hasRange; }

public slots:
    /**
     * Set the visible range on the plot x-axis.
     * @param axisKey The current axis mode (SessionKeys::Time or SessionKeys::TimeFromExit)
     * @param lower   The lower bound of the visible range
     * @param upper   The upper bound of the visible range
     */
    void setRange(const QString &axisKey, double lower, double upper);

    /**
     * Clear the range (e.g., when no data is displayed).
     * Listeners should show full data when hasRange() is false.
     */
    void clearRange();

signals:
    void rangeChanged();

private:
    QString m_axisKey;
    double m_rangeLower = 0.0;
    double m_rangeUpper = 0.0;
    bool m_hasRange = false;
};

} // namespace FlySight

#endif // PLOTRANGEMODEL_H
