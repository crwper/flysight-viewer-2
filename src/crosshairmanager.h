#ifndef CROSSHAIRMANAGER_H
#define CROSSHAIRMANAGER_H

#include <QObject>
#include <QPointer>
#include <QCursor>
#include <QMap>
#include <QSet>
#include <QCustomPlot/qcustomplot.h>
#include "sessionmodel.h"
#include "graphinfo.h"

namespace FlySight {

/*!
 * \brief The CrosshairManager class centralizes:
 * 1) Crosshair lines (horizontal + vertical) and toggling them on/off
 * 2) Cursor switching (transparent vs. normal)
 * 3) Single vs. multi tracer display
 * 4) Hovered session detection (calling model->setHoveredSessionId)
 *
 * It duplicates the "single-tracer if near, else multi-tracer" logic
 * that was previously inside the tools.
 */
class CrosshairManager : public QObject
{
    Q_OBJECT
public:
    explicit CrosshairManager(QCustomPlot *plot,
                              SessionModel *model,
                              QMap<QCPGraph*, GraphInfo> *graphInfoMap,
                              QObject *parent = nullptr);

    //! Call this from eventFilter() on each mouse move
    void handleMouseMove(const QPoint &pixelPos);

    //! Call this from eventFilter() on QEvent::Leave
    void handleMouseLeave();

    //! If false, crosshair lines and tracers are all hidden, and no cursor change
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    //! If you want to override the distance threshold for single-tracer mode
    void setPixelThreshold(double px) { m_pixelThreshold = px; }

    //! Called if your plot was re-drawn or re-laid out,
    //! to keep the crosshair lines in sync if needed
    void updateIfOverPlotArea();

    //! Returns the set of session IDs currently marked by a visible tracer.
    QSet<QString> getTracedSessionIds() const;

private:
    // Create crosshair lines if needed
    void ensureCrosshairCreated();

    // Show or hide crosshair lines
    void setCrosshairVisible(bool visible);

    // Utility to see if a pixel is within the main axisRect
    bool isCursorOverPlotArea(const QPoint &pixelPos) const;

    // Move crosshair lines to the current position
    void updateCrosshairLines(const QPoint &pixelPos);

    // Single-tracer vs. multi-tracer logic:
    void updateTracers(const QPoint &pixelPos);

    // Repositions or hides QCPItemTracers
    QCPItemTracer* getOrCreateTracer(QCPGraph* g);
    void hideAllTracers();
    void hideAllExcept(QCPGraph* keep);

    // Finds the "closest graph" to pixelPos; returns distance in px
    QCPGraph* findClosestGraph(const QPoint &pixelPos, double &distOut) const;

private:
    QPointer<QCustomPlot> m_plot;
    QPointer<SessionModel> m_model;
    QMap<QCPGraph*, GraphInfo> *m_graphInfoMap; // Borrowed pointer

    bool m_enabled = true; // If false, we do nothing

    // Crosshair lines + old cursor
    QCPItemLine *m_crosshairH = nullptr;
    QCPItemLine *m_crosshairV = nullptr;
    bool m_isCursorOverPlot = false;
    QCursor m_originalCursor;
    QCursor m_transparentCursor;

    // We do "single-tracer" if distance < pixelThreshold, else "multi-tracer"
    double m_pixelThreshold = 8.0;

    // Each graph can have its own tracer. We'll create them on demand.
    QMap<QCPGraph*, QCPItemTracer*> m_tracers;

    // Keep track of which sessions have visible tracers
    QSet<QString> m_currentlyTracedSessionIds;
};

} // namespace FlySight

#endif // CROSSHAIRMANAGER_H
