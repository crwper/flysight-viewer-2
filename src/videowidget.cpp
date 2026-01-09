#include "videowidget.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QDir>

namespace FlySight {

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_label(new QLabel(this))
{
    auto *layout = new QVBoxLayout(this);

    m_label->setWordWrap(true);
    m_label->setText(tr("No video loaded."));

    layout->addWidget(m_label);
}

void VideoWidget::loadVideo(const QString &filePath)
{
    m_filePath = filePath;

    if (!m_label)
        return;

    if (m_filePath.isEmpty()) {
        m_label->setText(tr("No video loaded."));
    } else {
        m_label->setText(tr("Video: %1").arg(QDir::toNativeSeparators(m_filePath)));
    }
}

} // namespace FlySight
