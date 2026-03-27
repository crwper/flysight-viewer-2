// LogbookView.h
#ifndef LOGBOOKVIEW_H
#define LOGBOOKVIEW_H

#include <QToolButton>
#include <QWidget>
#include <QTreeView>
#include <QProgressBar>
#include "sessionmodel.h"

namespace FlySight {

class LogbookView : public QWidget
{
    Q_OBJECT
public:
    LogbookView(SessionModel *model, QWidget *parent = nullptr);
    QList<QModelIndex> selectedRows() const;

signals:
    void showSelectedRequested();
    void hideSelectedRequested();
    void hideOthersRequested();
    void deleteRequested();
    void focusSessionRequested(int row);
    void currentSessionChanged(const QString& sessionId);
    void cancelRequested(int taskId);

public slots:
    void selectSessions(const QList<QString> &sessionIds);
    void onActiveTaskChanged(int id, bool cancellable);
    void onProgressChanged(int id, int remaining, int total);
    void onSchedulerIdle();

private slots:
    void onContextMenuRequested(const QPoint &pos);

protected:
    QSize minimumSizeHint() const override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTreeView *treeView;
    SessionModel *model;
    QProgressBar *m_progressBar;
    QToolButton *m_cancelButton;
    int m_activeTaskId = -1;

    void setupView();
};

} // namespace FlySight

#endif // LOGBOOKVIEW_H
