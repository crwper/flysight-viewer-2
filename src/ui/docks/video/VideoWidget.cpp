#include "VideoWidget.h"

#include "cursormodel.h"
#include "sessionmodel.h"

#include <QQuickWidget>
#include <QQuickItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QStyle>
#include <QSignalBlocker>
#include <QUrl>
#include <QDir>
#include <QDateTime>
#include <QTimeZone>
#include <QVideoSink>
#include <QWheelEvent>

#if QT_VERSION_MAJOR >= 6
#include <QAudioOutput>
#else
#include <QMediaContent>
#include <QOverload>
#endif

namespace FlySight {

static constexpr qint64 kStepMs = 40;

// Wheel scrubbing configuration
static constexpr qint64 kWheelBaseStepMs = 40;       // Base step (one frame at ~25fps)
static constexpr qint64 kWheelMinStepMs = 40;        // Minimum step (single frame)
static constexpr qint64 kWheelMaxStepMs = 5000;      // Maximum step (5 seconds)
static constexpr qint64 kWheelFastThresholdMs = 50;  // Time threshold for fast scrolling
static constexpr qint64 kWheelSlowThresholdMs = 200; // Time threshold for slow scrolling
static constexpr double kWheelMaxMultiplier = 50.0;  // Max acceleration multiplier
static constexpr int kWheelDeltaPerStep = 120;       // Standard wheel delta per notch
static constexpr int   kSeekWatchdogMs = 300;        // Failsafe timeout for frame-gated seeks
static constexpr int   kWheelPixelsPerStep = 40;     // Approx pixels per "notch" for trackpads

VideoWidget::VideoWidget(SessionModel *sessionModel,
                         CursorModel *cursorModel,
                         QWidget *parent)
    : QWidget(parent)
    , m_sessionModel(sessionModel)
    , m_cursorModel(cursorModel)
    , m_player(new QMediaPlayer(this))
{
#if QT_VERSION_MAJOR >= 6
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setMuted(false);
    m_audioOutput->setVolume(1.0);
    m_player->setAudioOutput(m_audioOutput);
#endif

    // Video surface via QQuickWidget (avoids macOS QVideoWidget stutter)
    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_quickWidget->setClearColor(Qt::black);
    m_quickWidget->setSource(QUrl(QStringLiteral("qrc:/qml/VideoSurface.qml")));

    // Wire VideoOutput to player when QML is ready
    auto wireOutput = [this]() {
        auto *root = m_quickWidget->rootObject();
        if (!root) {
            qWarning() << "VideoSurface: rootObject is null";
            return;
        }
        auto *voObj = root->findChild<QObject*>(QStringLiteral("videoOutput"));
        if (!voObj) {
            qWarning() << "VideoSurface: could not find 'videoOutput' object";
            return;
        }
        m_player->setVideoOutput(voObj);
        connect(root, SIGNAL(clicked()), this, SLOT(onPlayPauseClicked()));
        qDebug() << "VideoSurface: setVideoOutput succeeded";
    };

    if (m_quickWidget->status() == QQuickWidget::Ready) {
        wireOutput();
    } else {
        connect(m_quickWidget, &QQuickWidget::statusChanged,
                this, [this, wireOutput](QQuickWidget::Status s) {
            if (s == QQuickWidget::Ready)
                wireOutput();
            else if (s == QQuickWidget::Error) {
                for (const auto &e : m_quickWidget->errors())
                    qWarning().noquote() << "Video QML error:" << e.toString();
            }
        });
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    layout->addWidget(m_quickWidget, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setText(tr("No video loaded."));
    layout->addWidget(m_statusLabel);

    auto *controlsRow = new QHBoxLayout();
    controlsRow->setContentsMargins(0, 0, 0, 0);
    controlsRow->setSpacing(4);

    m_stepBackButton = new QToolButton(this);
    m_stepBackButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
    m_stepBackButton->setToolTip(tr("Step backward"));
    controlsRow->addWidget(m_stepBackButton);

    m_playPauseButton = new QToolButton(this);
    m_playPauseButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    controlsRow->addWidget(m_playPauseButton);

    m_stepForwardButton = new QToolButton(this);
    m_stepForwardButton->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    m_stepForwardButton->setToolTip(tr("Step forward"));
    controlsRow->addWidget(m_stepForwardButton);

    controlsRow->addStretch(1);

    m_timeLabel = new QLabel(this);
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_timeLabel->setText(tr("00:00.000 / 00:00.000"));
    controlsRow->addWidget(m_timeLabel);

    layout->addLayout(controlsRow);

    m_positionSlider = new QSlider(Qt::Horizontal, this);
    m_positionSlider->setRange(0, 0);
    layout->addWidget(m_positionSlider);

    // Sync controls (selector + Mark Exit + legacy tools)
    auto *syncLayout = new QVBoxLayout();
    syncLayout->setContentsMargins(0, 0, 0, 0);
    syncLayout->setSpacing(4);

    auto *syncTopRow = new QHBoxLayout();
    syncTopRow->setContentsMargins(0, 0, 0, 0);
    syncTopRow->setSpacing(6);

    m_sessionLabel = new QLabel(tr("Sync session:"), this);
    syncTopRow->addWidget(m_sessionLabel);

    m_sessionCombo = new QComboBox(this);
    m_sessionCombo->setToolTip(tr("Select which visible session to use for Mark Exit sync."));
    syncTopRow->addWidget(m_sessionCombo, 1);

    m_markExitButton = new QPushButton(tr("Mark Exit"), this);
    m_markExitButton->setToolTip(tr("Sync the current video frame to the selected session's Exit Time."));
    syncTopRow->addWidget(m_markExitButton);

    syncTopRow->addStretch(1);

    syncLayout->addLayout(syncTopRow);

    auto *syncBottomRow = new QHBoxLayout();
    syncBottomRow->setContentsMargins(0, 0, 0, 0);
    syncBottomRow->setSpacing(6);

    m_frameAnchorLabel = new QLabel(this);
    syncBottomRow->addWidget(m_frameAnchorLabel);

    m_utcAnchorLabel = new QLabel(this);
    syncBottomRow->addWidget(m_utcAnchorLabel);

    m_syncStatusLabel = new QLabel(this);
    syncBottomRow->addWidget(m_syncStatusLabel);

    syncBottomRow->addStretch(1);

    syncLayout->addLayout(syncBottomRow);

    layout->addLayout(syncLayout);

    // Player -> UI
    connect(m_player, &QMediaPlayer::durationChanged,
            this, &VideoWidget::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged,
            this, &VideoWidget::onPositionChanged);

#if QT_VERSION_MAJOR >= 6
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &VideoWidget::onPlaybackStateChanged);
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, &VideoWidget::onErrorOccurred);
#else
    connect(m_player, &QMediaPlayer::stateChanged,
            this, &VideoWidget::onStateChanged);
    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
            this, &VideoWidget::onErrorOccurred);
#endif

    // UI controls
    connect(m_playPauseButton, &QToolButton::clicked,
            this, &VideoWidget::onPlayPauseClicked);
    connect(m_stepBackButton, &QToolButton::clicked,
            this, &VideoWidget::onStepBackwardClicked);
    connect(m_stepForwardButton, &QToolButton::clicked,
            this, &VideoWidget::onStepForwardClicked);

    connect(m_positionSlider, &QSlider::sliderPressed,
            this, &VideoWidget::onSliderPressed);
    connect(m_positionSlider, &QSlider::sliderReleased,
            this, &VideoWidget::onSliderReleased);
    connect(m_positionSlider, &QSlider::sliderMoved,
            this, &VideoWidget::onSliderMoved);

    connect(m_markExitButton, &QPushButton::clicked,
            this, &VideoWidget::onMarkExitClicked);

    if (m_sessionCombo) {
        connect(m_sessionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &VideoWidget::onSelectedSessionChanged);
    }

    if (m_sessionModel) {
        connect(m_sessionModel, &SessionModel::modelChanged,
                this, &VideoWidget::rebuildSessionSelector);
    }

    rebuildSessionSelector();

    // Seek watchdog: if a frame doesn't arrive after setPosition(), don't let gating get stuck.
    m_seekWatchdog.setSingleShot(true);
    m_seekWatchdog.setInterval(kSeekWatchdogMs);
    connect(&m_seekWatchdog, &QTimer::timeout, this, [this]() {
        if (!m_seekInFlight)
            return;

        // Treat as "seek completion" even if no new frame arrived (e.g., snapped to same frame).
        m_seekInFlight = false;

        if (m_pendingSeekPos >= 0) {
            const qint64 next = m_pendingSeekPos;
            m_pendingSeekPos = -1;
            seekVideo(next);
            return;
        }

        // Release gating so normal playback/UI updates are not blocked.
        cancelPendingSeek();

        // Option A: snap UI/cursor to actual backend position once seek settles.
        if (m_player) {
            const qint64 actual = m_player->position();
            m_lastSeekTarget = -1;

            if (m_positionSlider && !m_sliderIsDown) {
                const QSignalBlocker blocker(m_positionSlider);
                m_positionSlider->setValue(static_cast<int>(qMax<qint64>(0, actual)));
            }

            updateTimeLabel(actual, m_player->duration());
            updateVideoCursorFromPositionMs(actual);
        } else {
            m_lastSeekTarget = -1;
        }
    });

    // Capture wheel events from all children so scrolling anywhere in the
    // dock triggers video scrubbing instead of the child's default behavior.
    const auto children = findChildren<QWidget *>();
    for (QWidget *child : children)
        child->installEventFilter(this);

    setControlsEnabled(false);
    updatePlayPauseButton();
    updateSyncLabels();
}

void VideoWidget::loadVideo(const QString &filePath)
{
    // Stop any previous playback and clear sync anchor state (v1: single video at a time).
    m_anchorVideoSeconds.reset();
    m_anchorUtcSeconds.reset();
    updateSyncLabels();

    // Reset any in-flight seek gating so it can't affect the next media.
    cancelPendingSeek();
    m_lastSeekTarget = -1;

    if (m_player) {
        m_player->stop();
    }

    m_filePath = filePath;

    if (!m_player || !m_statusLabel) {
        return;
    }

    if (m_filePath.isEmpty()) {
        m_statusLabel->setText(tr("No video loaded."));

#if QT_VERSION_MAJOR >= 6
        m_player->setSource(QUrl());
#else
        m_player->setMedia(QMediaContent());
#endif

        setControlsEnabled(false);
        onDurationChanged(0);
        onPositionChanged(0);
        updatePlayPauseButton();
        updateMarkExitEnabled();
        return;
    }

    m_statusLabel->setText(tr("Video: %1").arg(QDir::toNativeSeparators(m_filePath)));

#if QT_VERSION_MAJOR >= 6
    m_player->setSource(QUrl::fromLocalFile(m_filePath));
#else
    m_player->setMedia(QMediaContent(QUrl::fromLocalFile(m_filePath)));
#endif

    setControlsEnabled(true);
    updatePlayPauseButton();
    updateMarkExitEnabled();
}

void VideoWidget::onPlayPauseClicked()
{
    if (!m_player)
        return;

#if QT_VERSION_MAJOR >= 6
    const bool isPlaying = (m_player->playbackState() == QMediaPlayer::PlayingState);
#else
    const bool isPlaying = (m_player->state() == QMediaPlayer::PlayingState);
#endif

    if (isPlaying) {
        m_player->pause();
    } else {
        // Starting playback should never be blocked by a pending gated seek.
        cancelPendingSeek();
        m_lastSeekTarget = -1;
        m_player->play();
    }
}

void VideoWidget::onStepBackwardClicked()
{
    if (!m_player)
        return;

    m_player->pause();

    const qint64 pos = (m_lastSeekTarget >= 0) ? m_lastSeekTarget
                                                : m_player->position();
    const qint64 newPos = qMax<qint64>(0, pos - kStepMs);
    seekVideo(newPos);
}

void VideoWidget::onStepForwardClicked()
{
    if (!m_player)
        return;

    m_player->pause();

    const qint64 pos = (m_lastSeekTarget >= 0) ? m_lastSeekTarget
                                                : m_player->position();
    const qint64 dur = m_player->duration();

    const qint64 unclamped = pos + kStepMs;
    const qint64 newPos = (dur > 0) ? qMin(dur, unclamped) : unclamped;
    seekVideo(newPos);
}

void VideoWidget::onSliderPressed()
{
    if (!m_player)
        return;

    m_sliderIsDown = true;

#if QT_VERSION_MAJOR >= 6
    m_resumeAfterScrub = (m_player->playbackState() == QMediaPlayer::PlayingState);
#else
    m_resumeAfterScrub = (m_player->state() == QMediaPlayer::PlayingState);
#endif

    // Optional: pause during drag so playback doesn't fight the scrubber.
    if (m_resumeAfterScrub) {
        m_player->pause();
    }
}

void VideoWidget::onSliderReleased()
{
    if (!m_player || !m_positionSlider)
        return;

    cancelPendingSeek();
    m_lastSeekTarget = -1;

    m_sliderIsDown = false;

    const qint64 newPos = static_cast<qint64>(m_positionSlider->value());
    m_player->setPosition(newPos);

    updateVideoCursorFromPositionMs(newPos);

    if (m_resumeAfterScrub) {
        m_player->play();
    }
    m_resumeAfterScrub = false;
}

void VideoWidget::onSliderMoved(int value)
{
    if (!m_player || !m_sliderIsDown)
        return;

    seekVideo(static_cast<qint64>(value));
}

void VideoWidget::seekVideo(qint64 positionMs)
{
    if (!m_player)
        return;

    m_lastSeekTarget = positionMs;

    updateTimeLabel(positionMs, m_player->duration());
    updateVideoCursorFromPositionMs(positionMs);

    if (m_positionSlider && !m_sliderIsDown) {
        const QSignalBlocker blocker(m_positionSlider);
        m_positionSlider->setValue(static_cast<int>(qMax<qint64>(0, positionMs)));
    }

    // If we don't have a sink yet (e.g., QML output not wired), we can't gate on frames.
    // Fall back to direct seeking so we never deadlock the gating state.
    auto *sink = m_player->videoSink();
    if (!sink) {
        m_player->setPosition(positionMs);
        return;
    }

    if (!m_seekSignalConnected) {
        connect(sink, &QVideoSink::videoFrameChanged,
                this, &VideoWidget::onVideoFrameChanged);
        m_seekSignalConnected = true;
    }

    if (m_seekInFlight) {
        m_pendingSeekPos = positionMs;
        return;
    }

    m_seekInFlight = true;
    m_pendingSeekPos = -1;

    m_player->setPosition(positionMs);

    // Failsafe in case no frame arrives for this seek.
    m_seekWatchdog.start();
}

void VideoWidget::cancelPendingSeek()
{
    m_seekWatchdog.stop();

    m_seekInFlight = false;
    m_pendingSeekPos = -1;

    if (m_seekSignalConnected) {
        if (auto *sink = m_player ? m_player->videoSink() : nullptr) {
            disconnect(sink, &QVideoSink::videoFrameChanged,
                       this, &VideoWidget::onVideoFrameChanged);
        }
        m_seekSignalConnected = false;
    }
}

void VideoWidget::onVideoFrameChanged()
{
    if (!m_seekInFlight)
        return;

    m_seekWatchdog.stop();
    m_seekInFlight = false;

    if (m_pendingSeekPos >= 0) {
        const qint64 next = m_pendingSeekPos;
        m_pendingSeekPos = -1;
        seekVideo(next);
        return;
    }

    // No more pending seeks: release gating.
    cancelPendingSeek();

    // Option A: snap UI/cursor to the actual backend position (keyframe-snapped),
    // then clear the "requested position is authoritative" override.
    if (m_player) {
        const qint64 actual = m_player->position();
        m_lastSeekTarget = -1;

        if (m_positionSlider && !m_sliderIsDown) {
            const QSignalBlocker blocker(m_positionSlider);
            m_positionSlider->setValue(static_cast<int>(qMax<qint64>(0, actual)));
        }

        updateTimeLabel(actual, m_player->duration());
        updateVideoCursorFromPositionMs(actual);
    } else {
        m_lastSeekTarget = -1;
    }
}

void VideoWidget::onDurationChanged(qint64 durationMs)
{
    if (!m_positionSlider)
        return;

    const int durInt = static_cast<int>(qMax<qint64>(0, durationMs));

    {
        const QSignalBlocker blocker(m_positionSlider);
        m_positionSlider->setRange(0, durInt);

        if (!m_sliderIsDown) {
            const int posInt = static_cast<int>(qMax<qint64>(0, m_player ? m_player->position() : 0));
            m_positionSlider->setValue(posInt);
        }
    }

    updateTimeLabel(m_player ? m_player->position() : 0, durationMs);
}

void VideoWidget::onPositionChanged(qint64 positionMs)
{
    if (!m_positionSlider)
        return;

    // While throttled seeking is active, seekVideo maintains the UI
    // at the intended position. Don't let the backend's actual position
    // (which may be keyframe-snapped) overwrite it.
    if (m_seekSignalConnected)
        return;

    // After seeking completes, the scrub target remains authoritative
    // until the user explicitly transitions (play, slider release, etc.).
    // This prevents keyframe-snapped positions from causing jitter.
    if (m_lastSeekTarget >= 0)
        return;

    if (!m_sliderIsDown) {
        const QSignalBlocker blocker(m_positionSlider);
        m_positionSlider->setValue(static_cast<int>(qMax<qint64>(0, positionMs)));
    }

    updateTimeLabel(positionMs, m_player ? m_player->duration() : 0);

    updateVideoCursorFromPositionMs(positionMs);
}

#if QT_VERSION_MAJOR >= 6
void VideoWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    Q_UNUSED(state);
    updatePlayPauseButton();
}

void VideoWidget::onErrorOccurred(QMediaPlayer::Error error, const QString &errorString)
{
    Q_UNUSED(error);

    if (m_statusLabel) {
        m_statusLabel->setText(tr("Video error: %1").arg(errorString));
    }
    updatePlayPauseButton();
}
#else
void VideoWidget::onStateChanged(QMediaPlayer::State state)
{
    Q_UNUSED(state);
    updatePlayPauseButton();
}

void VideoWidget::onErrorOccurred(QMediaPlayer::Error error)
{
    Q_UNUSED(error);

    if (m_statusLabel && m_player) {
        m_statusLabel->setText(tr("Video error: %1").arg(m_player->errorString()));
    }
    updatePlayPauseButton();
}
#endif

void VideoWidget::onMarkExitClicked()
{
    if (!m_player || !m_sessionModel)
        return;

    if (m_filePath.isEmpty())
        return;

    const QString sessionId = selectedSessionId();
    if (sessionId.isEmpty())
        return;

    const int row = m_sessionModel->getSessionRow(sessionId);
    if (row < 0)
        return;

    SessionData &session = m_sessionModel->sessionRef(row);

    const QVariant exitVar = session.getAttribute(SessionKeys::ExitTime);
    if (!exitVar.canConvert<QDateTime>()) {
        if (m_statusLabel) {
            m_statusLabel->setText(tr("Video: %1\nCannot mark exit: selected session has no Exit Time.")
                                   .arg(QDir::toNativeSeparators(m_filePath)));
        }
        return;
    }

    const QDateTime exitDt = exitVar.toDateTime();
    if (!exitDt.isValid()) {
        if (m_statusLabel) {
            m_statusLabel->setText(tr("Video: %1\nCannot mark exit: selected session Exit Time is invalid.")
                                   .arg(QDir::toNativeSeparators(m_filePath)));
        }
        return;
    }

    const double exitUtcSeconds = exitDt.toMSecsSinceEpoch() / 1000.0;

    const qint64 posMs = m_player->position();
    m_anchorVideoSeconds = static_cast<double>(posMs) / 1000.0;
    m_anchorUtcSeconds = exitUtcSeconds;

    updateSyncLabels();
}

void VideoWidget::onSelectedSessionChanged(int index)
{
    Q_UNUSED(index);
    updateMarkExitEnabled();
    updateVideoCursorSyncState();
}

void VideoWidget::rebuildSessionSelector()
{
    if (!m_sessionCombo)
        return;

    const QString prevId = selectedSessionId();

    const QSignalBlocker blocker(m_sessionCombo);
    m_sessionCombo->clear();

    if (!m_sessionModel) {
        m_sessionCombo->addItem(tr("No sessions"), QString());
        m_sessionCombo->setEnabled(false);
        updateMarkExitEnabled();
        updateVideoCursorSyncState();
        return;
    }

    int selectIndex = -1;
    int visibleCount = 0;

    const QVector<SessionData> &sessions = m_sessionModel->getAllSessions();
    for (const SessionData &s : sessions) {
        if (!s.isVisible())
            continue;

        const QString id = s.getAttribute(SessionKeys::SessionId).toString();
        if (id.isEmpty())
            continue;

        QString label = s.getAttribute(SessionKeys::Description).toString();
        if (label.isEmpty())
            label = id;

        m_sessionCombo->addItem(label, id);

        if (!prevId.isEmpty() && id == prevId)
            selectIndex = visibleCount;

        ++visibleCount;
    }

    if (m_sessionCombo->count() == 0) {
        m_sessionCombo->addItem(tr("No visible sessions"), QString());
        m_sessionCombo->setEnabled(false);
    } else {
        m_sessionCombo->setEnabled(true);
        m_sessionCombo->setCurrentIndex(selectIndex >= 0 ? selectIndex : 0);
    }

    updateMarkExitEnabled();
    updateVideoCursorSyncState();
}

QString VideoWidget::selectedSessionId() const
{
    if (!m_sessionCombo)
        return QString();

    return m_sessionCombo->currentData().toString();
}

void VideoWidget::updateMarkExitEnabled()
{
    if (!m_markExitButton)
        return;

    const bool videoLoaded = !m_filePath.isEmpty();
    const bool playbackEnabled = (m_positionSlider && m_positionSlider->isEnabled());
    const bool hasSession = !selectedSessionId().isEmpty();

    m_markExitButton->setEnabled(videoLoaded && playbackEnabled && hasSession);
}

QString VideoWidget::formatTimeMs(qint64 ms)
{
    if (ms < 0)
        ms = 0;

    const qint64 hours = ms / 3600000;
    const qint64 minutes = (ms / 60000) % 60;
    const qint64 seconds = (ms / 1000) % 60;
    const qint64 millis = ms % 1000;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hours,   2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(millis,  3, 10, QChar('0'));
    }

    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(millis,  3, 10, QChar('0'));
}

void VideoWidget::updatePlayPauseButton()
{
    if (!m_playPauseButton || !m_player)
        return;

#if QT_VERSION_MAJOR >= 6
    const bool isPlaying = (m_player->playbackState() == QMediaPlayer::PlayingState);
#else
    const bool isPlaying = (m_player->state() == QMediaPlayer::PlayingState);
#endif

    m_playPauseButton->setIcon(style()->standardIcon(
        isPlaying ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));

    m_playPauseButton->setToolTip(isPlaying ? tr("Pause") : tr("Play"));
}

void VideoWidget::updateTimeLabel(qint64 positionMs, qint64 durationMs)
{
    if (!m_timeLabel)
        return;

    m_timeLabel->setText(tr("%1 / %2")
        .arg(formatTimeMs(positionMs))
        .arg(formatTimeMs(durationMs)));
}

void VideoWidget::updateSyncLabels()
{
    if (!m_frameAnchorLabel || !m_utcAnchorLabel || !m_syncStatusLabel)
        return;

    const QString frameText = m_anchorVideoSeconds.has_value()
        ? formatTimeMs(static_cast<qint64>(*m_anchorVideoSeconds * 1000.0))
        : tr("—");

    const QString utcText = m_anchorUtcSeconds.has_value()
        ? QDateTime::fromMSecsSinceEpoch(
              qint64((*m_anchorUtcSeconds) * 1000.0),
              QTimeZone::UTC
          ).toString(QStringLiteral("yy-MM-dd HH:mm:ss.zzz"))
        : tr("—");

    m_frameAnchorLabel->setText(tr("Frame: %1").arg(frameText));
    m_utcAnchorLabel->setText(tr("UTC: %1").arg(utcText));

    const bool synced = m_anchorVideoSeconds.has_value() && m_anchorUtcSeconds.has_value();
    m_syncStatusLabel->setText(synced ? tr("Synced") : tr("Not synced"));

    updateVideoCursorSyncState();
}

void VideoWidget::setControlsEnabled(bool enabled)
{
    if (m_stepBackButton)    m_stepBackButton->setEnabled(enabled);
    if (m_playPauseButton)   m_playPauseButton->setEnabled(enabled);
    if (m_stepForwardButton) m_stepForwardButton->setEnabled(enabled);

    if (m_positionSlider)    m_positionSlider->setEnabled(enabled);

    updateMarkExitEnabled();
}

void VideoWidget::updateVideoCursorSyncState()
{
    if (!m_cursorModel)
        return;

    const bool synced = m_anchorVideoSeconds.has_value() && m_anchorUtcSeconds.has_value();
    if (!synced) {
        m_cursorModel->setCursorActive(QStringLiteral("video"), false);
        return;
    }

    const QString sessionId = selectedSessionId();
    if (!sessionId.isEmpty()) {
        QSet<QString> targets;
        targets.insert(sessionId);
        m_cursorModel->setCursorTargetsExplicit(QStringLiteral("video"), targets);
    } else {
        m_cursorModel->setCursorTargetPolicy(QStringLiteral("video"), CursorModel::TargetPolicy::AutoVisibleOverlap);
    }

    m_cursorModel->setCursorActive(QStringLiteral("video"), true);

    if (m_player) {
        updateVideoCursorFromPositionMs(m_player->position());
    }
}

void VideoWidget::updateVideoCursorFromPositionMs(qint64 positionMs)
{
    if (!m_cursorModel)
        return;

    if (!m_anchorVideoSeconds.has_value() || !m_anchorUtcSeconds.has_value())
        return;

    const double videoNowSeconds = static_cast<double>(positionMs) / 1000.0;
    const double utcNow = (*m_anchorUtcSeconds) + (videoNowSeconds - (*m_anchorVideoSeconds));

    m_cursorModel->setCursorPositionUtc(QStringLiteral("video"), utcNow);
}

bool VideoWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        wheelEvent(static_cast<QWheelEvent *>(event));
        return true;   // consumed — don't let the child handle it
    }
    return QWidget::eventFilter(obj, event);
}

void VideoWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_player || m_filePath.isEmpty()) {
        QWidget::wheelEvent(event);
        return;
    }

    // Pause playback during wheel scrubbing (matches step-button behavior).
    m_player->pause();

    // Get wheel delta (positive = scroll up/away = forward, negative = scroll down/toward = backward)
    int delta = event->angleDelta().y();
    bool usingPixels = false;

    if (delta == 0) {
        delta = event->pixelDelta().y();
        usingPixels = true;
    }

    if (delta == 0) {
        event->ignore();
        return;
    }

    event->accept();

    // Calculate velocity-based acceleration
    double accelerationMultiplier = 1.0;

    if (m_wheelTimerStarted) {
        const qint64 elapsedMs = m_wheelTimer.elapsed();

        if (elapsedMs < kWheelFastThresholdMs) {
            // Fast scrolling: apply strong acceleration with quadratic ramp
            const double t = 1.0 - (static_cast<double>(elapsedMs) / kWheelFastThresholdMs);
            accelerationMultiplier = 1.0 + (kWheelMaxMultiplier - 1.0) * t * t;
        } else if (elapsedMs < kWheelSlowThresholdMs) {
            // Medium scrolling: mild acceleration with linear ramp
            const double t = 1.0 - (static_cast<double>(elapsedMs - kWheelFastThresholdMs)
                                    / (kWheelSlowThresholdMs - kWheelFastThresholdMs));
            accelerationMultiplier = 1.0 + t * 2.0;
        }
        // else: slow scrolling, keep multiplier at 1.0
    }

    m_wheelTimer.restart();
    m_wheelTimerStarted = true;

    // Calculate step based on delta magnitude and acceleration
    // Standard wheel notch is 120 units; high-resolution wheels may send smaller values
    const double denom = usingPixels ? static_cast<double>(kWheelPixelsPerStep)
                                     : static_cast<double>(kWheelDeltaPerStep);
    const double notches = static_cast<double>(delta) / denom;
    const double rawStepMs = kWheelBaseStepMs * accelerationMultiplier * qAbs(notches);

    // Clamp to reasonable bounds
    const qint64 stepMs = qBound(kWheelMinStepMs,
                                  static_cast<qint64>(rawStepMs),
                                  kWheelMaxStepMs);

    // Apply direction
    const qint64 signedStep = (delta > 0) ? stepMs : -stepMs;

    // Calculate and clamp new position
    const qint64 currentPos = (m_lastSeekTarget >= 0) ? m_lastSeekTarget
                                                       : m_player->position();
    const qint64 duration = m_player->duration();
    const qint64 newPos = qBound(static_cast<qint64>(0),
                                  currentPos + signedStep,
                                  duration > 0 ? duration : currentPos);

    // Apply the position change
    seekVideo(newPos);
}

} // namespace FlySight
