#ifndef LEGENDMANAGER_H
#define LEGENDMANAGER_H

#include <QObject>
#include <QPointer>
#include <QMap>
#include <QVector>
#include <QString>
#include "qcustomplot/qcustomplot.h"
#include "graphinfo.h"

namespace FlySight {

class SessionModel;
class LegendWidget;

class LegendManager : public QObject
{
    Q_OBJECT
public:
    enum Mode {
        PointDataMode,
        RangeStatsMode
    };

    explicit LegendManager(QCustomPlot *plot,
                           SessionModel *model,
                           QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                           LegendWidget *legendWidget,
                           QObject *parent = nullptr);
    ~LegendManager() override;

    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    // Update methods for different modes
    bool updatePointData(double xCoord,
                         const QString& targetSessionId,
                         const QString& sessionDescription,
                         const QString& xAxisKey);
    bool updateRangeStats(double xCoord);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Call when graphs change to rebuild legend structure (now: just refresh series list + clear widget)
    void rebuildLegend();

    // No longer needed (overlay removed), kept for API compatibility
    void updateLegendPosition();

private:
    struct SeriesInfo {
        QString name;
        QColor color;
        QString sensorId;
        QString measurementId;
        QCPGraph* graph;
    };

    QPointer<QCustomPlot> m_plot;
    QPointer<SessionModel> m_model;
    QMap<QCPGraph*, GraphInfo> *m_graphInfoMap = nullptr;

    QPointer<LegendWidget> m_legendWidget;

    bool m_visible = false;
    Mode m_mode = PointDataMode;

    QVector<SeriesInfo> m_visibleSeries;

    void collectVisibleSeries();

    double interpolateValueAtX(QCPGraph* graph, double x) const;
    QString formatValue(double value, const QString& measurementId) const;

    double getRawValueAtX(const QString& sessionId,
                          const QString& xAxisKey,
                          double xCoord,
                          const QString& sensorId,
                          const QString& measurementId);
};

} // namespace FlySight

#endif // LEGENDMANAGER_H
