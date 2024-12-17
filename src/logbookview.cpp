// logbookview.cpp
#include "logbookview.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDebug>
#include <QItemSelectionModel>
#include <QHeaderView>

namespace FlySight {

LogbookView::LogbookView(SessionModel *model, QWidget *parent)
    : QWidget(parent),
      treeView(new QTreeView(this)),
      model(model)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(treeView);
    setLayout(layout);

    setupView();
}

void LogbookView::setupView()
{
    treeView->setModel(model);
    treeView->setRootIsDecorated(false);
    treeView->header()->setDefaultSectionSize(100);
    treeView->setMouseTracking(true); // Enable mouse tracking

    // Ensure native hover highlighting is enabled
    treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    treeView->setUniformRowHeights(true);

    // Install event filter to detect hover events on treeView
    treeView->viewport()->installEventFilter(this);
}

bool LogbookView::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == treeView->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            QPoint pos = mouseEvent->pos();
            QModelIndex index = treeView->indexAt(pos);
            if (index.isValid()) {
                QString sessionId = model->getAllSessions().at(index.row()).getAttribute(SessionKeys::SessionId);
                if (model->hoveredSessionId() != sessionId) {
                    model->setHoveredSessionId(sessionId);
                }
            } else {
                if (!model->hoveredSessionId().isEmpty()) {
                    model->setHoveredSessionId(QString());
                }
            }
        } else if (event->type() == QEvent::Leave) {
            if (!model->hoveredSessionId().isEmpty()) {
                model->setHoveredSessionId(QString());
            }
        }
    }

    // Allow the base class to process other events
    return QWidget::eventFilter(obj, event);
}


} // namespace FlySight
