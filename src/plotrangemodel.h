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
 * The range is expressed in the current axis coordinate system, defined
 * by the (xVariable, referenceMarkerKey) pair:
 * - referenceMarkerKey empty: absolute UTC seconds
 * - referenceMarkerKey non-empty: seconds relative to the reference marker
 */
class PlotRangeModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double rangeLower READ rangeLower NOTIFY rangeChanged)
    Q_PROPERTY(double rangeUpper READ rangeUpper NOTIFY rangeChanged)
    Q_PROPERTY(QString xVariable READ xVariable NOTIFY rangeChanged)
    Q_PROPERTY(QString referenceMarkerKey READ referenceMarkerKey NOTIFY rangeChanged)
    Q_PROPERTY(bool hasRange READ hasRange NOTIFY rangeChanged)

public:
    explicit PlotRangeModel(QObject *parent = nullptr);

    double rangeLower() const { return m_rangeLower; }
    double rangeUpper() const { return m_rangeUpper; }
    QString xVariable() const { return m_xVariable; }
    QString referenceMarkerKey() const { return m_referenceMarkerKey; }
    bool hasRange() const { return m_hasRange; }

public slots:
    /**
     * Set the visible range on the plot x-axis.
     * @param xVariable           The independent variable key (e.g. SessionKeys::Time)
     * @param referenceMarkerKey  The reference marker attribute key (empty = absolute)
     * @param lower               The lower bound of the visible range
     * @param upper               The upper bound of the visible range
     */
    void setRange(const QString &xVariable, const QString &referenceMarkerKey,
                  double lower, double upper);

    /**
     * Clear the range (e.g., when no data is displayed).
     * Listeners should show full data when hasRange() is false.
     */
    void clearRange();

signals:
    void rangeChanged();

private:
    QString m_xVariable;
    QString m_referenceMarkerKey;
    double m_rangeLower = 0.0;
    double m_rangeUpper = 0.0;
    bool m_hasRange = false;
};

} // namespace FlySight

#endif // PLOTRANGEMODEL_H
