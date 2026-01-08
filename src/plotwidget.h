#ifndef PLOTWIDGET_H
#define PLOTWIDGET_H

#include <QWidget>
#include <QMap>
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
class PlotViewSettingsModel;
class PlotModel;
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
        SetGround
    };

    struct PlotContext {
        PlotWidget *widget = nullptr;
        QCustomPlot* plot = nullptr;
        QMap<QCPGraph*, GraphInfo>* graphMap = nullptr;
        SessionModel *model = nullptr;
    };

    // Constructor
    PlotWidget(SessionModel *model,
               PlotModel *plotModel,
               PlotViewSettingsModel *viewSettingsModel,
               CursorModel *cursorModel,
               QWidget *parent = nullptr);

    // Public Methods
    void setCurrentTool(Tool tool);
    void revertToPrimaryTool();
    void setXAxisRange(double min, double max);
    void handleSessionsSelected(const QList<QString> &sessionIds);
    CrosshairManager* crosshairManager() const;

    static double interpolateY(const QCPGraph* graph, double x);

    QString getXAxisKey() const;

signals:
    void sessionsSelected(const QList<QString> &sessionIds);
    void toolChanged(PlotWidget::Tool newTool);

public slots:
    void updatePlot();
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

    // Plot Management
    QMap<QCPGraph*, GraphInfo> m_graphInfoMap;
    QMap<QString, QCPAxis*> m_plotValueAxes;

    // State Management
    bool m_updatingYAxis = false;
    bool m_mouseInPlotArea = false;

    std::unique_ptr<CrosshairManager> m_crosshairManager;

    QVector<QPointer<QCPAbstractItem>> m_referenceMarkerItems;

    QString m_xAxisKey   = SessionKeys::TimeFromExit;
    QString m_xAxisLabel = "Time from exit (s)";

    void applyXAxisChange(const QString& key, const QString& label);
};

} // namespace FlySight

#endif // PLOTWIDGET_H
