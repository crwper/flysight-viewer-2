#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QStandardItemModel>
#include <QTreeView>
#include "sessionmodel.h"
#include "logbookview.h"

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

signals:
    void plotValueSelected(const QModelIndex &selectedIndex);
    void newTimeRange(double min, double max);

private slots:
    void on_action_Import_triggered();
    void on_actionImportFolder_triggered();
    void on_action_ShowSelected_triggered();
    void on_action_HideOthers_triggered();
    void on_action_Delete_triggered();

private:
    // Plot values
    struct PlotValue {
        QString category;          // Category name
        QString plotName;          // Display name of the plot
        QString plotUnits;         // Units for the y-axis
        QColor defaultColor;       // Default color for the plot
        QString sensorID;          // Sensor name (e.g., "GNSS")
        QString measurementID;     // Measurement name (e.g., "hMSL")
    };

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

    // Helper functions for plot values
    void setupPlotValues();
    void populatePlotModel(QStandardItemModel* plotModel, const QVector<PlotValue>& plotValues);

    // Helper function for importing files
    void importFiles(const QStringList &fileNames, bool showProgress, const QString &baseDir = QString());

    // Helper methods for calculated values
    void initializeCalculatedAttributes();
    void initializeCalculatedMeasurements();

    // Helper methods for plot menu
    void initializePlotsMenu();
    void togglePlot(const QString &sensorID, const QString &measurementID);
};

} // namespace FlySight

#endif // MAINWINDOW_H
