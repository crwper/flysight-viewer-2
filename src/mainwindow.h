#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QStandardItemModel>
#include <QTreeView>
#include "sessionmodel.h"
#include "logbookview.h"
#include "plotwidget.h"
#include "plotregistry.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace FlySight {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum PlotRoles {
        DefaultColorRole = Qt::UserRole + 1,
        SensorIDRole,
        MeasurementIDRole,
        PlotUnitsRole
    };

    // Accessors for persisting the current x-axis measurement key
    QString currentXAxisKey() const;

signals:
    void plotValueSelected(const QModelIndex &selectedIndex);
    void newTimeRange(double min, double max);

private slots:
    void on_action_Import_triggered();
    void on_action_ImportFolder_triggered();
    void on_action_Pan_triggered();
    void on_action_Zoom_triggered();
    void on_action_Select_triggered();
    void on_action_SetExit_triggered();
    void on_action_SetGround_triggered();
    void on_action_ShowSelected_triggered();
    void on_action_HideSelected_triggered();
    void on_action_HideOthers_triggered();
    void on_action_Delete_triggered();
    void on_action_Preferences_triggered();
    void on_action_Exit_triggered();

    void onPlotWidgetToolChanged(PlotWidget::Tool t);

private:
    enum class PlotMenuItemType {
        Regular,
        Separator
    };

    struct PlotMenuItem {
        QString menuText;
        QKeySequence shortcut;
        QString sensorID;
        QString measurementID;
        PlotMenuItemType type;

        // Constructor for regular plot items
        PlotMenuItem(const QString &text, const QKeySequence &key, const QString &sensor, const QString &measurement)
            : menuText(text), shortcut(key), sensorID(sensor), measurementID(measurement), type(PlotMenuItemType::Regular) {}

        // Constructor for separators
        PlotMenuItem(PlotMenuItemType itemType)
            : menuText(QString()), shortcut(QKeySequence()), sensorID(QString()), measurementID(QString()), type(itemType) {}
    };

    QSettings *m_settings;
    Ui::MainWindow *ui;

    SessionModel *model;

    // Plot value selection components
    QDockWidget *plotDock;
    QTreeView *plotTreeView;
    QStandardItemModel *plotModel;
    LogbookView *logbookView;
    PlotWidget *plotWidget;

    // Pointer to QActionGroup for tools
    QActionGroup *toolActionGroup;

    // Helper functions for plot values
    static void registerBuiltInPlots();
    void setupPlotValues();
    void populatePlotModel(QStandardItemModel* plotModel, const QVector<PlotValue>& plotValues);

    // Helper function for importing files
    void importFiles(const QStringList &fileNames, bool showProgress, const QString &baseDir = QString());

    // Helper function for preferences
    void initializePreferences();

    // Helper methods for calculated values
    void initializeCalculatedAttributes();
    void initializeCalculatedMeasurements();

    // Helper methods for plot menu
    void initializeXAxisMenu();
    void initializePlotsMenu();
    void togglePlot(const QString &sensorID, const QString &measurementID);

    // Helper functions for tracks
    void setSelectedTrackCheckState(Qt::CheckState state);

    // Plot tools
    void setupPlotTools();

    // Accessors for persisting the current x-axis measurement key
    void setXAxisKey(const QString &key);
};

} // namespace FlySight

#endif // MAINWINDOW_H
