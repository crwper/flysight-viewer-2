#include "videowidget.h"

#include "cursormodel.h"
#include "sessionmodel.h"

#include <QVideoWidget>
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

#if QT_VERSION_MAJOR >= 6
#include <QAudioOutput>
#else
#include <QMediaContent>
#include <QOverload>
#endif

namespace FlySight {

static constexpr qint64 kStepMs = 40;

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
    m_player->setAudioOutput(m_audioOutput);
#endif

    m_videoWidget = new QVideoWidget(this);
    m_player->setVideoOutput(m_videoWidget);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    layout->addWidget(m_videoWidget, 1);

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

    m_getFrameButton = new QPushButton(tr("Get Frame"), this);
    syncTopRow->addWidget(m_getFrameButton);

    m_selectTimeButton = new QPushButton(tr("Select Time"), this);
    syncTopRow->addWidget(m_selectTimeButton);

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

    connect(m_getFrameButton, &QPushButton::clicked,
            this, &VideoWidget::onGetFrameClicked);
    connect(m_selectTimeButton, &QPushButton::clicked,
            this, &VideoWidget::onSelectTimeClicked);

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
        m_player->play();
    }
}

void VideoWidget::onStepBackwardClicked()
{
    if (!m_player)
        return;

    m_player->pause();

    const qint64 pos = m_player->position();
    const qint64 newPos = qMax<qint64>(0, pos - kStepMs);
    m_player->setPosition(newPos);

    updateVideoCursorFromPositionMs(newPos);
}

void VideoWidget::onStepForwardClicked()
{
    if (!m_player)
        return;

    m_player->pause();

    const qint64 pos = m_player->position();
    const qint64 dur = m_player->duration();

    const qint64 unclamped = pos + kStepMs;
    const qint64 newPos = (dur > 0) ? qMin(dur, unclamped) : unclamped;

    m_player->setPosition(newPos);

    updateVideoCursorFromPositionMs(newPos);
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

    const qint64 newPos = static_cast<qint64>(value);
    m_player->setPosition(newPos);

    // Keep time label responsive while dragging.
    updateTimeLabel(newPos, m_player->duration());

    updateVideoCursorFromPositionMs(newPos);
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

void VideoWidget::onGetFrameClicked()
{
    if (!m_player)
        return;

    const qint64 posMs = m_player->position();
    m_anchorVideoSeconds = static_cast<double>(posMs) / 1000.0;
    updateSyncLabels();
}

void VideoWidget::onSelectTimeClicked()
{
    // Ask the main window to put the plot into Pick-Time mode.
    emit selectTimeRequested();
}

void VideoWidget::setAnchorUtcSeconds(double utcSeconds)
{
    m_anchorUtcSeconds = utcSeconds;
    updateSyncLabels();
}

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
    const bool playbackEnabled = (m_getFrameButton && m_getFrameButton->isEnabled());
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
              Qt::UTC
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

    if (m_getFrameButton)    m_getFrameButton->setEnabled(enabled);
    if (m_selectTimeButton)  m_selectTimeButton->setEnabled(enabled);

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

} // namespace FlySight
