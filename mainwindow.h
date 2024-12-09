#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include <QVector>
#include "calculatedvaluemanager.h"
#include "plotspec.h"
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
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionImport_triggered();
    void on_actionImportFolder_triggered();
    void on_actionDelete_triggered();
    void updateDeleteActionState(const QItemSelection &selected, const QItemSelection &deselected);
    void onSessionItemChanged(QTreeWidgetItem *item, int column);
    void onPlotSelectionChanged(QTreeWidgetItem *item, int column);

private:
    Ui::MainWindow *ui;

    QSettings *m_settings;

    // Member variable to store all SessionData objects
    QMap<QString, SessionData> m_sessionDataMap;

    // Map to keep track of which sessions are plotted
    QMap<QString, QCPGraph*> m_plottedSessions;

    // Map to keep track of session colors
    QMap<QString, QColor> m_sessionColors;

    // Plot specifications
    QVector<PlotSpec> m_plotSpecs;

    // Currently selected plot specification
    PlotSpec m_currentPlotSpec;

    // Calculated Value Manager
    CalculatedValueManager *m_calculatedValueManager;

    // Helper methods
    void mergeSessionData(const SessionData& newSession);
    void populateLogbookTreeWidget();
    void filterLogbookTree(const QString &filterText);

    // Methods to initialize plot specifications and UI
    void initializePlotSpecs();
    void initializePlotSelectionDock();
    void populatePlotSelectionTree();
    void applyCurrentPlotSpec();

    // Helper methods for calculated values
    void initializeCalculatedValues();
};

#endif // MAINWINDOW_H
