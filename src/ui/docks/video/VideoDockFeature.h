#ifndef VIDEODOCKFEATURE_H
#define VIDEODOCKFEATURE_H

#include "ui/docks/DockFeature.h"

namespace FlySight {

class VideoWidget;
struct AppContext;

/**
 * DockFeature for the video playback view.
 * Displays video playback synced with cursor position.
 */
class VideoDockFeature : public DockFeature
{
    Q_OBJECT

public:
    explicit VideoDockFeature(const AppContext& ctx, QObject* parent = nullptr);

    QString id() const override;
    QString title() const override;
    KDDockWidgets::QtWidgets::DockWidget* dock() const override;
    KDDockWidgets::Location defaultLocation() const override;

    VideoWidget* videoWidget() const { return m_videoWidget; }

private:
    KDDockWidgets::QtWidgets::DockWidget* m_dock = nullptr;
    VideoWidget* m_videoWidget = nullptr;
};

} // namespace FlySight

#endif // VIDEODOCKFEATURE_H
