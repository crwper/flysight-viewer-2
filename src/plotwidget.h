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
    // Enums and Structs
    enum class Tool {
        Pan,
        Zoom,
        Select
    };

    struct GraphInfo {
        QString sessionId;
        QString sensorId;
        QString measurementId;
        QPen defaultPen;
    };

    struct PlotContext {
        PlotWidget *widget = nullptr;
        QCustomPlot* plot = nullptr;
        QMap<QCPGraph*, GraphInfo>* graphMap = nullptr;
    };

    // Constructor
    PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent = nullptr);

    // Public Methods
    void setCurrentTool(Tool tool);
    void setXAxisRange(double min, double max);
    void handleSessionsSelected(const QList<QString> &sessionIds);

signals:
    void sessionsSelected(const QList<QString> &sessionIds);

public slots:
    void updatePlot();
    void onXAxisRangeChanged(const QCPRange &newRange);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onHoveredSessionChanged(const QString& sessionId);

private:
    // Initialization
    void setupPlot();
    void setupCrosshairs();

    // Crosshair Management
    void handleCrosshairMouseMove(QMouseEvent *mouseEvent);
    void handleCrosshairLeave(QEvent *event);
    void enableCrosshairs(bool enable);
    void updateCrosshairs(const QPoint &pos);
    bool isCursorOverPlotArea(const QPoint &pos) const;

    // Utility Methods
    static double interpolateY(const QCPGraph* graph, double x);

    // Member Variables
    QCustomPlot *customPlot;
    SessionModel *model;
    QStandardItemModel *plotModel;

    // Tools
    PlotTool* m_currentTool = nullptr;
    std::unique_ptr<PanTool> m_panTool;
    std::unique_ptr<ZoomTool> m_zoomTool;
    std::unique_ptr<SelectTool> m_selectTool;

    // Plot Management
    QMap<QCPGraph*, GraphInfo> m_graphInfoMap;
    QMap<QString, QCPAxis*> m_plotValueAxes;

    // Crosshairs
    QCPItemLine *crosshairH;
    QCPItemLine *crosshairV;

    // Cursor Management
    QCursor transparentCursor;
    QCursor originalCursor;
    bool isCursorOverPlot;

    // State Management
    bool m_updatingYAxis = false;
};

} // namespace FlySight

#endif // PLOTWIDGET_H
