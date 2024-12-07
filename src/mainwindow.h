#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QTreeView>
#include "sessionmodel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void colorSelected(const QColor &color);

private:
    Ui::MainWindow *ui;

    SessionModel *model;

    // Color selection components
    QDockWidget *colorDock;
    QTreeView *colorTreeView;
    QStandardItemModel *colorModel;

    void setupColorSelection();
};
#endif // MAINWINDOW_H
