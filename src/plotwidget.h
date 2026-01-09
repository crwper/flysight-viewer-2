#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <memory>
#include "qcustomplot/qcustomplot.h"
#include "sessionmodel.h"
#include "graphinfo.h"
#include "crosshairmanager.h"

namespace FlySight {

class PlotTool;
class PanTool;
class ZoomTool;
class SelectTool;
class SetExitTool;
class SetGroundTool;
class PickTimeTool;
class PlotViewSettingsModel;
class PlotModel;
class MarkerModel;
class CursorModel;

struct ReferenceMoment
{
    QString sessionId;
    double exitUtcSeconds = 0.0;

    enum class Kind { Exit };
    Kind kind = Kind::Exit;
};

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    // Enums and Structs
    enum class Tool {
        Pan,
        Zoom,
        Select,
        SetExit,
        SetGround,
        PickTime
    };

    struct PlotContext {
        PlotWidget *widget = nullptr;
        QCustomPlot* plot = nullptr;
        QMap<QCPGraph*, GraphInfo>* graphMap = nullptr;
        SessionModel *model = nullptr;
    };

    struct MarkerBubbleMeta
    {
        int count = 0;
        double utcSeconds = 0.0; // Only valid when count == 1
    };

    // Constructor
    PlotWidget(SessionModel *model,
            PlotModel *plotModel,
            MarkerModel *markerModel,
            PlotViewSettingsModel *viewSettingsModel,
            CursorModel *cursorModel,
            QWidget *parent = nullptr);

    // Public Methods
    void setCurrentTool(Tool tool);
    void revertToPrimaryTool();
    void beginPickUtcTime();
    void setXAxisRange(double min, double max);
    void handleSessionsSelected(const QList<QString> &sessionIds);
    CrosshairManager* crosshairManager() const;

    const QVector<QVector<QPointer<QCPAbstractItem>>> &markerItemsByLane() const { return m_markerItemsByLane; }

    bool markerBubbleMeta(QCPItemText *bubble, MarkerBubbleMeta *outMeta) const
    {
        if (!bubble)
            return false;

        auto it = m_markerBubbleMeta.constFind(bubble);
        if (it == m_markerBubbleMeta.constEnd())
            return false;

        if (outMeta)
            *outMeta = it.value();

        return true;
    }

    static double interpolateY(const QCPGraph* graph, double x);

    QString getXAxisKey() const;

signals:
    void sessionsSelected(const QList<QString> &sessionIds);
    void toolChanged(PlotWidget::Tool newTool);

    // Step 5: emitted by Pick-Time mode (pick logic added in later steps)
    void utcTimePicked(double utcSeconds);

public slots:
    void updatePlot();
    void updateMarkersOnly();
    void onXAxisRangeChanged(const QCPRange &newRange);
    void onXAxisKeyChanged(const QString &newKey, const QString &newLabel);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onHoveredSessionChanged(const QString& sessionId);
    void onCursorsChanged();

private:
    // Initialization
    void setupPlot();
    void updateXAxisTicker();

    // Utility Methods
    QPen determineGraphPen(const GraphInfo &info, const QString &hoveredSessionId) const;
    QString determineGraphLayer(const GraphInfo &info, const QString &hoveredSessionId) const;

    // View management
    const SessionData* referenceSession() const;
    static double exitTimeSeconds(const SessionData& s);
    QVector<ReferenceMoment> collectExitMoments() const;
    QCPRange keyRangeOf(const SessionData& s,
                        const QString& sensor,
                        const QString& meas) const;

    enum class UpdateMode { Rebuild, Reflow };
    void updateReferenceMarkers(UpdateMode mode);

    // Member Variables
    QCustomPlot *customPlot;
    SessionModel *model;
    PlotModel *plotModel;
    MarkerModel *markerModel;
    PlotViewSettingsModel* m_viewSettingsModel;
    CursorModel* m_cursorModel = nullptr;

    // Tools
    PlotTool* m_currentTool = nullptr;
    Tool m_primaryTool;

    std::unique_ptr<PanTool> m_panTool;
    std::unique_ptr<ZoomTool> m_zoomTool;
    std::unique_ptr<SelectTool> m_selectTool;
    std::unique_ptr<SetExitTool> m_setExitTool;
    std::unique_ptr<SetGroundTool> m_setGroundTool;
    std::unique_ptr<PickTimeTool> m_pickTimeTool;

    // Plot Management
    QMap<QCPGraph*, GraphInfo> m_graphInfoMap;
    QMap<QString, QCPAxis*> m_plotValueAxes;

    // State Management
    bool m_updatingYAxis = false;
    bool m_mouseInPlotArea = false;

    std::unique_ptr<CrosshairManager> m_crosshairManager;

    QVector<QVector<QPointer<QCPAbstractItem>>> m_markerItemsByLane;
    QHash<QCPItemText*, MarkerBubbleMeta> m_markerBubbleMeta;

    QString m_xAxisKey   = SessionKeys::TimeFromExit;
    QString m_xAxisLabel = "Time from exit (s)";

    void applyXAxisChange(const QString& key, const QString& label);
};

} // namespace FlySight

#endif // PLOTWIDGET_H
