#include <QApplication>
#include <QStyleFactory>
#include <kddockwidgets/KDDockWidgets.h>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Initiallize KDDockWidgets front end
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);

    // Fusion looks better in general
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    FlySight::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
