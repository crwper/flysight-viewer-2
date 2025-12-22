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
    bool updatePointData(double xCoord, const QString& targetSessionId, const QString& sessionDescription, const QString& xAxisKey);
    bool updateRangeStats(double xCoord);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    // Call when graphs change to rebuild legend structure
    void rebuildLegend();

    void updateLegendPosition();

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
    QCPLayoutGrid *m_mainLayout;    // MODIFIED: This will be the main container
    QCPLayoutGrid *m_tableLayout;   // MODIFIED: This will hold the tabular data
    QCPItemRect *m_backgroundRect;

    // Header Info Elements
    QCPTextElement *m_sessionDescElement;
    QCPTextElement *m_utcElement;
    QCPTextElement *m_coordsElement;
    QCPItemLine *m_separatorLine;

    QVector<QCPTextElement*> m_headerElements;
    QVector<QVector<QCPTextElement*>> m_dataElements; // [row][col]

    bool m_visible;
    Mode m_mode;
    QVector<SeriesInfo> m_visibleSeries;

    // Helper methods
    void createLegendStructure();
    void clearLegendElements();
    void collectVisibleSeries();

    // Data calculation helpers
    double interpolateValueAtX(QCPGraph* graph, double x) const;
    QString formatValue(double value, const QString& measurementId) const;

    // Layout helpers
    void setElementText(int row, int col, const QString& text,
                        const QColor& color = Qt::black);
    void clearDataRows();

    double getRawValueAtX(const QString& sessionId, const QString& xAxisKey, double xCoord, const QString& sensorId, const QString& measurementId);
};

} // namespace FlySight

#endif // LEGENDMANAGER_H
