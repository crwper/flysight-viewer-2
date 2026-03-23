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
    void cancelColumnWorkerRequested();
    void cancelLoaderRequested();

public slots:
    void selectSessions(const QList<QString> &sessionIds);
    void onSaveProgressChanged(int remaining, int total);
    void onLoadProgressChanged(int remaining, int total);
    void onColumnWorkerProgressChanged(int remaining, int total);

private slots:
    void onContextMenuRequested(const QPoint &pos);

protected:
    QSize minimumSizeHint() const override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTreeView *treeView;
    SessionModel *model;
    QProgressBar *m_saveProgressBar;
    QProgressBar *m_loadProgressBar;
    QToolButton *m_loadCancelButton;
    QProgressBar *m_columnWorkerProgressBar;
    QToolButton *m_columnWorkerCancelButton;

    void setupView();
};

} // namespace FlySight

#endif // LOGBOOKVIEW_H
