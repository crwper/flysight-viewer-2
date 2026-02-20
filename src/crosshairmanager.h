#ifndef CROSSHAIRMANAGER_H
#define CROSSHAIRMANAGER_H

#include <QObject>
#include <QPointer>
#include <QCursor>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QCustomPlot/qcustomplot.h>
#include "sessionmodel.h"
#include "graphinfo.h"
#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

/*!
 * \brief The CrosshairManager class centralizes:
 * 1) Crosshair lines (horizontal + vertical) and toggling them on/off
 * 2) Cursor switching (transparent vs. normal)
 * 3) Single vs. multi tracer display
 * 4) Hovered session detection (calling model->setHoveredSessionId)
 *
 * It focuses the nearest track by default, with Shift+hover for multi-tracer mode.
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

    //! Enable or disable the "multi-trace" fallback (Shift+hover).
    void setMultiTraceEnabled(bool enabled);

    //! Called if your plot was re-drawn or re-laid out,
    //! to keep the crosshair lines in sync if needed
    void updateIfOverPlotArea();

    //! Render externally-driven cursor (e.g., map hover) at xPlot for a single session.
    void setExternalCursor(const QString &sessionId, double xPlot);

    //! Render externally-driven cursor (e.g., video playback) at per-session xPlot positions.
    //! If showVerticalLineIfPossible is true, a vertical line is shown only when all sessions share the same xPlot.
    void setExternalCursorMulti(const QHash<QString, double> &xBySession, bool showVerticalLineIfPossible);

    //! Clear externally-driven cursor visuals (tracers/crosshair) if shown.
    void clearExternalCursor();

    //! Returns the set of session IDs currently marked by a visible tracer.
    QSet<QString> getTracedSessionIds() const;

signals:
    //! Emitted whenever the set of traced sessions changes (including from modifier key changes).
    void tracedSessionsChanged(const QSet<QString> &sessionIds);

private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);
    void checkModifiers();

private:
    void applyCrosshairPreferences();

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

    // Finds the "closest graph" to pixelPos; returns vertical pixel distance
    QCPGraph* findClosestGraph(const QPoint &pixelPos, double &distOut) const;

    // Returns the minimum vertical pixel distance from pixelPos to any graph in the given session
    double distToSession(const QPoint &pixelPos, double xPlot, const QString &sessionId) const;

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

    // If false, the Shift+hover multi-trace mode is disabled.
    bool m_multiTraceEnabled = true;

    // Each graph can have its own tracer. We'll create them on demand.
    QMap<QCPGraph*, QCPItemTracer*> m_tracers;

    // Keep track of which sessions have visible tracers
    QSet<QString> m_currentlyTracedSessionIds;

    // Modifier key polling for instant Shift response
    QTimer *m_modifierPollTimer = nullptr;
    bool m_lastShiftState = false;

    // Cached preference values
    QColor m_crosshairColor = Qt::gray;
    double m_crosshairThickness = 1.0;
};

} // namespace FlySight

#endif // CROSSHAIRMANAGER_H
