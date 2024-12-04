#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include "sessiondata.h"

class QSettings;

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

private slots:
    void on_actionImport_triggered();
    void on_actionDelete_triggered();
    void updateDeleteActionState(const QItemSelection &selected, const QItemSelection &deselected);

private:
    Ui::MainWindow *ui;

    QSettings *m_settings;

    // Member variable to store all SessionData objects
    QMap<QString, SessionData> m_sessionDataMap;

    // Helper methods
    void mergeSessionData(const SessionData& newSession);
    void populateLogbookTreeWidget();
    void filterLogbookTree(const QString &filterText);
};
#endif // MAINWINDOW_H
