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

    // Enable sorting on the view
    treeView->setSortingEnabled(true);
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
    QAction *hideSelectedAction = menu.addAction(tr("Hide Selected Tracks"));
    QAction *hideOthersAction = menu.addAction(tr("Hide Others"));
    menu.addSeparator();
    QAction *deleteAction= menu.addAction(tr("Delete Selected Tracks"));

    QAction *chosenAction = menu.exec(treeView->viewport()->mapToGlobal(pos));
    if (chosenAction == showSelectedAction) {
        emit showSelectedRequested();
    } else if (chosenAction == hideSelectedAction) {
        emit hideSelectedRequested();
    } else if (chosenAction == hideOthersAction) {
        emit hideOthersRequested();
    } else if (chosenAction == deleteAction) {
        emit deleteRequested();
    }
}

void LogbookView::selectSessions(const QList<QString> &sessionIds)
{
    QItemSelection selection;

    // Iterate over the session IDs and find corresponding rows
    for (const QString &sessionId : sessionIds) {
        int row = model->getSessionRow(sessionId);
        if (row >= 0) {
            // Select the entire row by selecting from column 0 to the last column
            QModelIndex topLeft = model->index(row, 0);
            QModelIndex bottomRight = model->index(row, model->columnCount() - 1);
            selection.select(topLeft, bottomRight);
        } else {
            qWarning() << "LogbookView::selectSessions: SESSION_ID not found -" << sessionId;
        }
    }

    // Apply the selection
    QItemSelectionModel *selectionModel = treeView->selectionModel();
    if (!selectionModel) {
        qWarning() << "LogbookView::selectSessions: No selection model available.";
        return;
    }

    // Clear existing selection and select the new sessions
    selectionModel->clearSelection();
    selectionModel->select(selection, QItemSelectionModel::Select | QItemSelectionModel::Rows);

    // Optionally, scroll to the first selected session
    if (!sessionIds.isEmpty()) {
        QString firstSessionId = sessionIds.first();
        int firstRow = model->getSessionRow(firstSessionId);
        if (firstRow >= 0) {
            QModelIndex firstIndex = model->index(firstRow, 0);
            treeView->scrollTo(firstIndex, QAbstractItemView::PositionAtCenter);
        }
    }
}

} // namespace FlySight
