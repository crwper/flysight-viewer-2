#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <kddockwidgets/KDDockWidgets.h>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/FlySightViewer.png"));

    // Initiallize KDDockWidgets front end
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);

    // Fusion looks better in general
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    FlySight::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
