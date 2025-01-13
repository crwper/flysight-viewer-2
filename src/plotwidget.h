#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QPointer>
#include <QSet>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"

namespace FlySight {

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    enum class Tool {
        Pan,
        Zoom,
        Select
    };

    PlotWidget(SessionModel *model, QStandardItemModel *plotModel, QWidget *parent = nullptr);

    void setCurrentTool(Tool tool);
    void setXAxisRange(double min, double max);

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
    // Highlight graphs if the model signals a hovered session change
    void onHoveredSessionChanged(const QString& sessionId);

private:
    QCustomPlot *customPlot;
    SessionModel *model;
    QStandardItemModel *plotModel;

    // Keep track of plotted graphs and their sessions
    QList<QCPGraph*> m_plottedGraphs;
    QMap<QString, QCPAxis*> m_plotValueAxes;
    QMap<QCPGraph*, QString> m_graphToSessionMap;
    QMap<QCPGraph*, QPen> m_graphDefaultPens;

    bool m_updatingYAxis = false;

    // Crosshair items
    QCPItemLine *crosshairH;
    QCPItemLine *crosshairV;

    // Cursor handling
    QCursor transparentCursor;
    QCursor originalCursor;
    bool isCursorOverPlot;

    // Current tool mode
    Tool currentTool;

    // Selection tool-related members
    bool m_selecting;
    QCPItemRect *m_selectionRect;
    QPoint m_selectionStartPixel;

    // Zoom tool-related
    bool m_zooming;
    QCPItemRect *m_zoomRect;
    QPoint m_zoomStartPixel;

    void setupPlot();
    void setupCrosshairs();
    void setupSelectionRectangle();
    void setupZoomRectangle();

    bool isCursorOverPlotArea(const QPoint &pos) const;
    void updateCrosshairs(const QPoint &pos);

    // Selection-related helper functions
    void updateSelectionRect(const QPoint &currentPos);
    void finalizeSelectionRect(const QPoint &endPos);
    QPointF pixelToPlotCoords(const QPoint &pixel) const;

    QList<QString> sessionsInRect(double xMin, double xMax, double yMin, double yMax) const;
    bool lineSegmentIntersectsRect(double x1, double y1, double x2, double y2,
                                   double xMin, double xMax, double yMin, double yMax) const;
    bool pointInRect(double x, double y, double xMin, double xMax, double yMin, double yMax) const;
    bool intersectSegmentWithVerticalLine(double x1, double y1, double x2, double y2,
                                          double lineX, double yMin, double yMax) const;
    bool intersectSegmentWithHorizontalLine(double x1, double y1, double x2, double y2,
                                            double lineY, double xMin, double xMax) const;

    static double interpolateY(const QCPGraph* graph, double x);
};

} // namespace FlySight

#endif // PLOTWIDGET_H
