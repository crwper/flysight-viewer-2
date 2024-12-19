// logbookview.cpp
#include "logbookview.h"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDebug>
#include <QItemSelectionModel>
#include <QHeaderView>
#include <QMenu>

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

    treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(treeView, &QTreeView::customContextMenuRequested, this, &LogbookView::onContextMenuRequested);
}

QList<QModelIndex> LogbookView::selectedRows() const {
    return treeView->selectionModel()->selectedRows();
}

void LogbookView::setupView()
{
    treeView->setModel(model);
    treeView->setRootIsDecorated(false);
    treeView->header()->setDefaultSectionSize(100);
    treeView->setMouseTracking(true); // Enable mouse tracking

    // Set selection mode to ExtendedSelection to allow multiple selections
    treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Set selection behavior to select entire rows
    treeView->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Optimize performance for large models
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
                QString sessionId = model->getAllSessions().at(index.row()).getAttribute(SessionKeys::SessionId).toString();
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

void LogbookView::onContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    QAction *showSelectedAction = menu.addAction(tr("Show Selected Tracks"));
    QAction *hideOthersAction = menu.addAction(tr("Hide Others"));

    QAction *chosenAction = menu.exec(treeView->viewport()->mapToGlobal(pos));
    if (chosenAction == showSelectedAction) {
        emit showSelectedRequested();
    } else if (chosenAction == hideOthersAction) {
        emit hideOthersRequested();
    }
}

} // namespace FlySight
