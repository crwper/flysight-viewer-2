#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QPointer>
#include <QSet>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"

namespace FlySight {

class PlotTool;
class PanTool;
class ZoomTool;
class SelectTool;

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Tool {
        Pan,
        Zoom,
        Select
    };

    struct GraphInfo
    {
        QString sessionId;
        QString sensorId;
        QString measurementId;
        QPen defaultPen;
    };

    struct PlotContext
    {
        PlotWidget *widget = nullptr;
        QCustomPlot* plot = nullptr;
        QMap<QCPGraph*, GraphInfo>* graphMap = nullptr;
    };

    PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent = nullptr);

    void setCurrentTool(Tool tool);
    void setXAxisRange(double min, double max);

    void handleSessionsSelected(const QList<QString> &sessionIds);

signals:
    // Emitted when a rectangular selection is made in select mode.
    // Contains the SESSION_IDs of the sessions that intersect with the selection area.
    void sessionsSelected(const QList<QString> &sessionIds);

public slots:
    // Called when the model or plot model changes, update the plot accordingly
    void updatePlot();

    // Called when the x-axis range changes, auto-adjusts y-axes
    void onXAxisRangeChanged(const QCPRange &newRange);

protected:
    // Event filter to capture mouse events on the plot widget
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onHoveredSessionChanged(const QString& sessionId);

private:
    QCustomPlot *customPlot;
    SessionModel *model;
    QStandardItemModel *plotModel;

    // Tools
    PlotTool* m_currentTool = nullptr;
    std::unique_ptr<PanTool> m_panTool;
    std::unique_ptr<ZoomTool> m_zoomTool;
    std::unique_ptr<SelectTool> m_selectTool;

    // Keep track of plotted graphs and their sessions
    QMap<QCPGraph*, GraphInfo> m_graphInfoMap;
    QMap<QString, QCPAxis*> m_plotValueAxes;

    bool m_updatingYAxis = false;

    // Crosshair items
    QCPItemLine *crosshairH;
    QCPItemLine *crosshairV;

    // Cursor handling
    QCursor transparentCursor;
    QCursor originalCursor;
    bool isCursorOverPlot;

    void setupPlot();

    void handleCrosshairMouseMove(QMouseEvent *mouseEvent);
    void handleCrosshairLeave(QEvent *event);

    void setupCrosshairs();
    void enableCrosshairs(bool enable);
    void updateCrosshairs(const QPoint &pos);
    bool isCursorOverPlotArea(const QPoint &pos) const;

    static double interpolateY(const QCPGraph* graph, double x);
};

} // namespace FlySight

#endif // PLOTWIDGET_H
