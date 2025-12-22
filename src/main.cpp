#include <QApplication>
#include <QStyleFactory>
#include <kddockwidgets/KDDockWidgets.h>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Initiallize KDDockWidgets front end
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);

#ifdef Q_OS_WINDOWS
    // Set the Windows Vista style only on Windows
    app.setStyle(QStyleFactory::create("windowsvista"));
#endif

    FlySight::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
