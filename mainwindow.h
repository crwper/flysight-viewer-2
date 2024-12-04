#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include "sessiondata.h"

class QCPGraph;
class QSettings;
class QTreeWidgetItem;

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
    void onSessionItemChanged(QTreeWidgetItem *item, int column);

private:
    Ui::MainWindow *ui;

    QSettings *m_settings;

    // Member variable to store all SessionData objects
    QMap<QString, SessionData> m_sessionDataMap;

    // Map to keep track of which sessions are plotted
    QMap<QString, QCPGraph*> m_plottedSessions;

    // Map to keep track of session colors
    QMap<QString, QColor> m_sessionColors;

    // Helper methods
    void mergeSessionData(const SessionData& newSession);
    void populateLogbookTreeWidget();
    void filterLogbookTree(const QString &filterText);

    // Helper methods for plotting
    void addSessionToPlot(const QString &sessionID);
    void removeSessionFromPlot(const QString &sessionID);
    void rebuildPlot();
};
#endif // MAINWINDOW_H
