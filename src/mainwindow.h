#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
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

private slots:
    void on_action_Import_triggered();
    void on_actionImportFolder_triggered();
    void on_action_Delete_triggered();

private:
    QSettings *m_settings;
    Ui::MainWindow *ui;

    SessionModel *model;

    // Color selection components
    QDockWidget *colorDock;
    QTreeView *colorTreeView;
    QStandardItemModel *colorModel;

    void setupColorSelection();

    // Helper function for importing files
    void importFiles(const QStringList &fileNames, bool showProgress);
};
#endif // MAINWINDOW_H
