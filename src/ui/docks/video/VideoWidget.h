#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QString>
#include <optional>

#include <QElapsedTimer>
#include <QTimer>
#include <QMediaPlayer>

QT_BEGIN_NAMESPACE
class QQuickWidget;
class QSlider;
class QToolButton;
class QPushButton;
class QLabel;
class QComboBox;
#if QT_VERSION_MAJOR >= 6
class QAudioOutput;
#endif
QT_END_NAMESPACE

namespace FlySight {

class CursorModel;
class SessionModel;

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(SessionModel *sessionModel,
                         CursorModel *cursorModel,
                         QWidget *parent = nullptr);
    ~VideoWidget() override;

    void loadVideo(const QString &filePath);
    QString videoFilePath() const { return m_filePath; }

protected:
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onPlayPauseClicked();
    void onStepBackwardClicked();
    void onStepForwardClicked();

    void onSliderPressed();
    void onSliderReleased();
    void onSliderMoved(int value);
    void onVideoFrameChanged();

    void onDurationChanged(qint64 durationMs);
    void onPositionChanged(qint64 positionMs);

#if QT_VERSION_MAJOR >= 6
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onErrorOccurred(QMediaPlayer::Error error, const QString &errorString);
#else
    void onStateChanged(QMediaPlayer::State state);
    void onErrorOccurred(QMediaPlayer::Error error);
#endif

    void onMarkExitClicked();
    void onExitTimeChanged(const QString &sessionId, double deltaSeconds);
    void onSelectedSessionChanged(int index);
    void rebuildSessionSelector();

private:
    static QString formatTimeMs(qint64 ms);
    void updatePlayPauseButton();
    void updateTimeLabel(qint64 positionMs, qint64 durationMs);
    void updateSyncLabels();
    void setControlsEnabled(bool enabled);

    QString selectedSessionId() const;
    void updateMarkExitEnabled();

    void seekVideo(qint64 positionMs);
    void cancelPendingSeek();

    // Drives CursorModel's "video" cursor while the video is synced.
    std::optional<double> syncedExitUtcSeconds() const;
    void updateVideoCursorSyncState();
    void updateVideoCursorFromPositionMs(qint64 positionMs);

private:
    SessionModel *m_sessionModel = nullptr;
    CursorModel *m_cursorModel = nullptr;

    QMediaPlayer *m_player = nullptr;
#if QT_VERSION_MAJOR >= 6
    QAudioOutput *m_audioOutput = nullptr;
#endif
    QQuickWidget *m_quickWidget = nullptr;

    QLabel *m_statusLabel = nullptr;

    QToolButton *m_stepBackButton = nullptr;
    QToolButton *m_playPauseButton = nullptr;
    QToolButton *m_stepForwardButton = nullptr;

    QSlider *m_positionSlider = nullptr;
    QLabel *m_timeLabel = nullptr;

    QLabel *m_sessionLabel = nullptr;
    QComboBox *m_sessionCombo = nullptr;

    QPushButton *m_markExitButton = nullptr;

    QLabel *m_frameAnchorLabel = nullptr;
    QLabel *m_utcAnchorLabel = nullptr;
    QLabel *m_syncStatusLabel = nullptr;

    QString m_filePath;

    bool m_sliderIsDown = false;
    bool m_resumeAfterScrub = false;

    // Seek throttle state (frame-ready based)
    bool   m_seekInFlight = false;
    qint64 m_pendingSeekPos = -1;
    bool   m_seekSignalConnected = false;
    qint64 m_lastSeekTarget = -1;

    // Failsafe: prevents seek gating from getting stuck if no new frame arrives
    QTimer m_seekWatchdog;

    std::optional<double> m_anchorVideoSeconds;

    // Wheel scrubbing state
    QElapsedTimer m_wheelTimer;
    bool m_wheelTimerStarted = false;
};

} // namespace FlySight

#endif // VIDEOWIDGET_H
