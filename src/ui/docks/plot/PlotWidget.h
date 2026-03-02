#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QMap>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <memory>
#include <optional>
#include "QCustomPlot/qcustomplot.h"
#include "dependencykey.h"
#include "markerregistry.h"
#include "sessionmodel.h"
#include "graphinfo.h"
#include "crosshairmanager.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

class PlotTool;
class PanTool;
class ZoomTool;
class SelectTool;
class SetExitTool;
class SetGroundTool;
class MeasureTool;
class PlotViewSettingsModel;
class PlotModel;
class MarkerModel;
class CursorModel;
class PlotRangeModel;
class MeasureModel;

class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    // Enums and Structs
    enum class Tool {
        Pan,
        Zoom,
        Measure,
        Select,
        SetExit,
        SetGround
    };

    struct PlotContext {
        PlotWidget *widget = nullptr;
        QCustomPlot* plot = nullptr;
        QMap<QCPGraph*, GraphInfo>* graphMap = nullptr;
        SessionModel *model = nullptr;
        PlotModel *plotModel = nullptr;
        MeasureModel *measureModel = nullptr;
    };

    struct MarkerBubbleMeta
    {
        int     count = 0;
        double  utcSeconds = 0.0;   // only valid when count == 1
        QString attributeKey;       // marker definition's attributeKey
        QString sessionId;          // only valid when count == 1
        bool    editable = false;   // from MarkerDefinition
        QVector<MeasurementKey> measurements;  // from MarkerDefinition
    };

    // Constructor / Destructor
    PlotWidget(SessionModel *model,
            PlotModel *plotModel,
            MarkerModel *markerModel,
            PlotViewSettingsModel *viewSettingsModel,
            CursorModel *cursorModel,
            PlotRangeModel *rangeModel,
            MeasureModel *measureModel,
            QWidget *parent = nullptr);
    ~PlotWidget();

    // Public Methods
    void setCurrentTool(Tool tool);
    void revertToPrimaryTool();
    void setXAxisRange(double min, double max);
    void handleSessionsSelected(const QList<QString> &sessionIds);
    CrosshairManager* crosshairManager() const;
    void lockFocusToSession(const QString &sessionId);
    void unlockFocus();

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

    QDateTime xCoordToUtcDateTime(double xCoord, const QString &sessionId) const;

    // Axis configuration getters (used by tools and downstream consumers)
    QString xVariable() const { return m_xVariable; }
    QString referenceMarkerKey() const { return m_referenceMarkerKey; }

signals:
    void sessionsSelected(const QList<QString> &sessionIds);
    void toolChanged(PlotWidget::Tool newTool);

public slots:
    void updatePlot();
    void updateMarkersOnly();
    void onXAxisRangeChanged(const QCPRange &newRange);
    void onXVariableChanged(const QString &newXVariable);
    void onReferenceMarkerKeyChanged(const QString &oldKey, const QString &newKey);
    void zoomToExtent();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onHoveredSessionChanged(const QString& sessionId);
    void onCursorsChanged();
    void onPreferenceChanged(const QString &key, const QVariant &value);
    void onDependencyChanged(const QString &sessionId, const DependencyKey &key);

private:
    // Initialization
    void setupPlot();
    void updateXAxisTicker();
    void applyPlotPreferences();
    void applyThemeColors();

    // Utility Methods
    QPen determineGraphPen(const GraphInfo &info, const QString &hoveredSessionId) const;
    QString determineGraphLayer(const GraphInfo &info, const QString &hoveredSessionId) const;

    // Coalescing rebuild helpers
    void schedulePlotRebuild();
    void scheduleMarkerUpdate();

    // View management
    const SessionData* referenceSession() const;
    std::optional<double> referenceOffsetForSession(const SessionData &session) const;
    QCPRange keyRangeOf(const SessionData& s,
                        const QString& sensor,
                        const QString& meas) const;

    enum class UpdateMode { Rebuild, Reflow };
    void updateReferenceMarkers(UpdateMode mode);

    // Bubble hit-testing and drag interaction
    QCPItemText* hitTestMarkerBubble(const QPoint &pos) const;
    bool handleBubblePress(QCPItemText *bubble, QMouseEvent *event);
    bool handleBubbleDrag(QMouseEvent *event);
    bool handleBubbleRelease(QMouseEvent *event);
    void showBubbleContextMenu(QCPItemText *bubble, const QPoint &globalPos);
    void handleBubbleDoubleClick(QCPItemText *bubble);
    void applyPinchZoom(double factor, const QPointF &centerPos);

    // Member Variables
    QCustomPlot *customPlot;
    SessionModel *model;
    PlotModel *plotModel;
    MarkerModel *markerModel;
    PlotViewSettingsModel* m_viewSettingsModel;
    CursorModel* m_cursorModel = nullptr;
    PlotRangeModel* m_rangeModel = nullptr;

    // Tools
    PlotTool* m_currentTool = nullptr;
    Tool m_primaryTool;

    std::unique_ptr<PanTool> m_panTool;
    std::unique_ptr<ZoomTool> m_zoomTool;
    std::unique_ptr<SelectTool> m_selectTool;
    std::unique_ptr<SetExitTool> m_setExitTool;
    std::unique_ptr<SetGroundTool> m_setGroundTool;
    std::unique_ptr<MeasureTool> m_measureTool;

    // Plot Management
    QMap<QCPGraph*, GraphInfo> m_graphInfoMap;
    QVector<QCPGraph*> m_graphDrawOrder;   // graphs in plot-model draw order
    QMap<QString, QCPAxis*> m_plotValueAxes;

    // State Management
    bool m_updatingYAxis = false;
    bool m_mouseInPlotArea = false;

    std::unique_ptr<CrosshairManager> m_crosshairManager;

    QVector<QVector<QPointer<QCPAbstractItem>>> m_markerItemsByLane;
    QHash<QCPItemText*, MarkerBubbleMeta> m_markerBubbleMeta;

    QString m_xVariable        = SessionKeys::Time;
    QString m_referenceMarkerKey = SessionKeys::ExitTime;
    QString m_xAxisLabel = "Time from exit (s)";

    // Cached preference values
    double m_lineThickness = 1.0;
    int m_textSize = 9;
    double m_yAxisPadding = 0.05;

    // Coalescing state for dependencyChanged signals
    enum class RebuildLevel { None, Full };
    RebuildLevel m_pendingRebuildLevel = RebuildLevel::None;
    QTimer m_rebuildTimer;
    QTimer m_markerUpdateTimer;

    // Viewport shift state for reference marker changes
    QString m_lastRefSessionId;
    double m_lastRefOffset = 0.0;

    // Drag state for editable marker bubbles
    QCPItemText *m_dragBubble = nullptr;
    QString      m_dragSessionId;
    QString      m_dragAttributeKey;
    double       m_dragXCoordOffset = 0.0;  // x-axis offset between click point and bubble anchor
};

} // namespace FlySight

#endif // PLOTWIDGET_H
