#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDockWidget>
#include <QHeaderView>
#include <QStandardItem>
#include <QTreeView>
#include "plotwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , model(new SessionModel(this))
{
    ui->setupUi(this);

    // Add logbook view
    QDockWidget *logbookDock = new QDockWidget(tr("Logbook"), this);
    QTreeView *logbookView = new QTreeView(logbookDock);
    logbookDock->setWidget(logbookView);
    addDockWidget(Qt::RightDockWidgetArea, logbookDock);

    // Add plot view
    PlotWidget *plotWidget = new PlotWidget(model, this);
    setCentralWidget(plotWidget);

    // Populate the model with data
    model->addItem({true, "Circle", 0});
    model->addItem({false, "Square", 4});
    model->addItem({true, "Triangle", 3});

    logbookView->setModel(model);
    logbookView->setRootIsDecorated(false);
    logbookView->header()->setDefaultSectionSize(100);

    // Setup color selection
    setupColorSelection();

    // Connect color selection to plot widget
    connect(this, &MainWindow::colorSelected, plotWidget, &PlotWidget::setPlotColor);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupColorSelection()
{
    // Create the dock widget
    colorDock = new QDockWidget(tr("Colors"), this);
    colorTreeView = new QTreeView(colorDock);
    colorDock->setWidget(colorTreeView);
    addDockWidget(Qt::LeftDockWidgetArea, colorDock);

    // Create the model
    colorModel = new QStandardItemModel(this);
    colorTreeView->setModel(colorModel);
    colorTreeView->setHeaderHidden(true); // Hide the header

    // Populate the model with categories and colors
    // "Cool colours" categoryq
    QStandardItem *coolItem = new QStandardItem("Cool colours");
    coolItem->setFlags(Qt::ItemIsEnabled); // Non-checkable
    colorModel->appendRow(coolItem);

    QStandardItem *blueItem = new QStandardItem("Blue");
    blueItem->setCheckable(true);
    blueItem->setData(QColor(Qt::blue), Qt::UserRole + 1); // Store color data
    coolItem->appendRow(blueItem);

    QStandardItem *greenItem = new QStandardItem("Green");
    greenItem->setCheckable(true);
    greenItem->setData(QColor(Qt::green), Qt::UserRole + 1);
    coolItem->appendRow(greenItem);

    // "Warm colours" category
    QStandardItem *warmItem = new QStandardItem("Warm colours");
    warmItem->setFlags(Qt::ItemIsEnabled); // Non-checkable
    colorModel->appendRow(warmItem);

    QStandardItem *redItem = new QStandardItem("Red");
    redItem->setCheckable(true);
    redItem->setData(QColor(Qt::red), Qt::UserRole + 1);
    warmItem->appendRow(redItem);

    QStandardItem *orangeItem = new QStandardItem("Magenta");
    orangeItem->setCheckable(true);
    orangeItem->setData(QColor(Qt::magenta), Qt::UserRole + 1);
    warmItem->appendRow(orangeItem);

    // Optionally, set a default checked color
    blueItem->setCheckState(Qt::Checked);

    // Connect to itemChanged signal to enforce mutual exclusivity
    connect(colorModel, &QStandardItemModel::itemChanged, this, [=](QStandardItem *item){
        if (item->isCheckable() && item->checkState() == Qt::Checked) {
            // Uncheck all other items
            QList<QStandardItem*> allItems = colorModel->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive);
            for(auto &otherItem : allItems) {
                if(otherItem != item && otherItem->isCheckable() && otherItem->checkState() == Qt::Checked){
                    otherItem->setCheckState(Qt::Unchecked);
                }
            }
            // Emit the selected color
            QColor selectedColor = item->data(Qt::UserRole + 1).value<QColor>();
            emit colorSelected(selectedColor);
        }
    });

    // Emit the initial selected color based on the default checked item
    QList<QStandardItem*> checkedItems = colorModel->findItems("*", Qt::MatchWildcard | Qt::MatchRecursive, Qt::CheckStateRole);
    for(auto &item : checkedItems){
        if(item->checkState() == Qt::Checked){
            QColor selectedColor = item->data(Qt::UserRole + 1).value<QColor>();
            emit colorSelected(selectedColor);
            break;
        }
    }
}
