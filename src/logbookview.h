// logbookview.h
#ifndef LOGBOOKVIEW_H
#define LOGBOOKVIEW_H

#include <QWidget>
#include <QTreeView>
#include "sessionmodel.h"

namespace FlySight {

class LogbookView : public QWidget
{
    Q_OBJECT
public:
    LogbookView(SessionModel *model, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QTreeView *treeView;
    SessionModel *model;

    void setupView();
};

} // namespace FlySight

#endif // LOGBOOKVIEW_H
