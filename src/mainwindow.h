#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>
#include <QSettings>
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

class LegendWidget;
class PlotViewSettingsModel;
class PlotModel;
class CursorModel;

class MainWindow : public KDDockWidgets::QtWidgets::MainWindow
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
    void xAxisKeyChanged(const QString &newKey, const QString &newLabel);

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
    KDDockWidgets::QtWidgets::DockWidget *plotSelectionDock;
    QTreeView *plotTreeView;
    PlotModel *plotModel;
    LogbookView *logbookView;
    PlotWidget *plotWidget;

    // Legend dock/widget (new)
    KDDockWidgets::QtWidgets::DockWidget *legendDock = nullptr;
    LegendWidget *legendWidget = nullptr;

    // Pointer to QActionGroup for tools
    QActionGroup *toolActionGroup;

    // Plot view settings
    PlotViewSettingsModel *m_plotViewSettingsModel;

    // Cursor model
    CursorModel *m_cursorModel = nullptr;

    // Helper functions for plot values
    static void registerBuiltInPlots();
    void setupPlotValues();

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
    void setXAxisKey(const QString &key, const QString &label);
};

} // namespace FlySight

#endif // MAINWINDOW_H
