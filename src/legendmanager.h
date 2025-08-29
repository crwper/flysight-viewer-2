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

class LegendManager : public QObject
{
    Q_OBJECT
public:
    enum Mode {
        PointDataMode,
        RangeStatsMode
    };

    explicit LegendManager(QCustomPlot *plot, SessionModel *model,
                           QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                           QObject *parent = nullptr);
    ~LegendManager();

    void setVisible(bool visible);
    bool isVisible() const { return m_visible; }

    // Update methods for different modes
    bool updatePointData(double xCoord, const QString& hoveredSessionId);
    bool updateRangeStats(double xCoord);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Call when graphs change to rebuild legend structure
    void rebuildLegend();

private:
    struct SeriesInfo {
        QString name;
        QColor color;
        QString sensorId;
        QString measurementId;
        QCPGraph* graph;
    };

    // Core components
    QPointer<QCustomPlot> m_plot;
    QPointer<SessionModel> m_model;
    QMap<QCPGraph*, GraphInfo> *m_graphInfoMap;

    // Legend UI elements
    QCPLayoutGrid *m_legendLayout;
    QCPItemRect *m_backgroundRect;
    QVector<QCPTextElement*> m_headerElements;
    QVector<QVector<QCPTextElement*>> m_dataElements; // [row][col]

    bool m_visible;
    Mode m_mode;
    QVector<SeriesInfo> m_visibleSeries;

    // Helper methods
    void createLegendStructure();
    void clearLegendElements();
    void updateLegendPosition();
    void collectVisibleSeries();

    // Data calculation helpers
    double interpolateValueAtX(QCPGraph* graph, double x) const;
    QString formatValue(double value, const QString& measurementId) const;

    // Layout helpers
    void setElementText(int row, int col, const QString& text,
                        const QColor& color = Qt::black);
    void clearDataRows();
};

} // namespace FlySight

#endif // LEGENDMANAGER_H
