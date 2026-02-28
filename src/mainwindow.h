#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <kddockwidgets/MainWindow.h>
#include <kddockwidgets/DockWidget.h>
#include <QSettings>
#include <QTreeView>
#include "sessionmodel.h"
#include "plotregistry.h"
#include "ui/docks/plot/PlotWidget.h"

QT_BEGIN_NAMESPACE
class QCloseEvent;
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace FlySight {

class DockFeature;
class PlotViewSettingsModel;
class PlotModel;
class MarkerModel;
class CursorModel;
class PlotRangeModel;
class MeasureModel;

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

signals:
    void plotValueSelected(const QModelIndex &selectedIndex);
    void newTimeRange(double min, double max);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_action_Import_triggered();
    void on_action_ImportFolder_triggered();
    void on_action_ImportVideo_triggered();
    void on_action_Pan_triggered();
    void on_action_Zoom_triggered();
    void on_action_Measure_triggered();
    void on_action_Select_triggered();
    void on_action_SetExit_triggered();
    void on_action_SetGround_triggered();
    void on_action_ShowSelected_triggered();
    void on_action_HideSelected_triggered();
    void on_action_HideOthers_triggered();
    void on_action_Delete_triggered();
    void on_action_Preferences_triggered();
    void on_action_Exit_triggered();
    void on_action_About_triggered();
    void on_action_ToggleUnits_triggered();

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

    // All dock features
    QList<DockFeature*> m_features;

    // Models
    PlotModel *plotModel;
    MarkerModel *markerModel = nullptr;

    // Pointer to QActionGroup for tools
    QActionGroup *toolActionGroup;

    // Plot view settings
    PlotViewSettingsModel *m_plotViewSettingsModel;

    // Cursor model
    CursorModel *m_cursorModel = nullptr;

    // Range model for synchronizing plot x-axis range with other docks
    PlotRangeModel *m_rangeModel = nullptr;

    // Measure model for measure tool data
    MeasureModel *m_measureModel = nullptr;

    // Helper functions for plot values
    static void registerBuiltInPlots();
    static void registerBuiltInMarkers();

    // Find a specific dock feature by type
    template<typename T>
    T* findFeature() const {
        for (auto* f : m_features) {
            if (auto* t = qobject_cast<T*>(f))
                return t;
        }
        return nullptr;
    }

    // Helper function for importing files
    void importFiles(const QStringList &fileNames, bool showProgress, const QString &baseDir = QString());

    // Helper function for preferences
    void initializePreferences();

    // Helper methods for menus
    void initializeXAxisMenu();
    void initializePlotsMenu();
    void initializeWindowMenu();
    void togglePlot(const QString &sensorID, const QString &measurementID);

    // Helper functions for tracks
    void setSelectedTrackCheckState(Qt::CheckState state);

    // Plot tools
    void setupPlotTools();

    // Save/restore dock layout between app launches
    void restoreDockLayout();
    void saveDockLayout();

};

} // namespace FlySight

#endif // MAINWINDOW_H
