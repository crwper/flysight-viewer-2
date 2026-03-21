// LogbookView.h
#ifndef LOGBOOKVIEW_H
#define LOGBOOKVIEW_H

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

public slots:
    void selectSessions(const QList<QString> &sessionIds);
    void startProgress(int totalStubs);
    void onSessionLoaded();

private slots:
    void onContextMenuRequested(const QPoint &pos);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTreeView *treeView;
    SessionModel *model;
    QProgressBar *m_progressBar;
    int m_totalStubs = 0;
    int m_loadedCount = 0;

    void setupView();
};

} // namespace FlySight

#endif // LOGBOOKVIEW_H
