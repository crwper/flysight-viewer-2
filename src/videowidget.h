#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace FlySight {

class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);

    void loadVideo(const QString &filePath);
    QString videoFilePath() const { return m_filePath; }

private:
    QLabel *m_label = nullptr;
    QString m_filePath;
};

} // namespace FlySight

#endif // VIDEOWIDGET_H
