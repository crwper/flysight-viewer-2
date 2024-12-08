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

    enum PlotRoles {
        DefaultColorRole = Qt::UserRole + 1,
        SensorIDRole,
        MeasurementIDRole,
        PlotUnitsRole
    };

signals:
    void plotValueSelected(const QModelIndex &selectedIndex);

private slots:
    void on_action_Import_triggered();
    void on_actionImportFolder_triggered();
    void on_action_Delete_triggered();

private:
    // Plot values
    typedef struct {
        QString category;          // Category name
        QString plotName;          // Display name of the plot
        QString plotUnits;         // Units for the y-axis
        QColor defaultColor;       // Default color for the plot
        QString sensorID;          // Sensor name (e.g., "GNSS")
        QString measurementID;     // Measurement name (e.g., "hMSL")
    } PlotValue;

    QSettings *m_settings;
    Ui::MainWindow *ui;

    SessionModel *model;

    // Plot value selection components
    QDockWidget *plotDock;
    QTreeView *plotTreeView;
    QStandardItemModel *plotModel;

    // Helper functions for plot values
    void setupPlotValues();
    void populatePlotModel(QStandardItemModel* plotModel, const QVector<PlotValue>& plotValues,
                           QStandardItem** firstCheckedItem);

    // Helper function for importing files
    void importFiles(const QStringList &fileNames, bool showProgress);
};
#endif // MAINWINDOW_H
