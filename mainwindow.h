#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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

private:
    Ui::MainWindow *ui;

    QSettings *m_settings;

    // Member variable to store all SessionData objects
    QMap<QString, SessionData> m_sessionDataMap;

    // Helper method to merge SessionData
    void mergeSessionData(const SessionData& newSession);

    // Function to populate the logbookTreeWidget
    void populateLogbookTreeWidget();

    // Function to filter the tree based on search input
    void filterLogbookTree(const QString &filterText);
};
#endif // MAINWINDOW_H
