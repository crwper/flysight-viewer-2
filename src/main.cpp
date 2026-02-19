#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <kddockwidgets/KDDockWidgets.h>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
#ifdef Q_OS_MACOS
    // V8 fails to reserve virtual memory for its JIT CodeRange on Apple Silicon.
    // Jitless mode uses the interpreter only, avoiding that allocation.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--js-flags=--jitless");
#endif

    QApplication app(argc, argv);
#ifndef Q_OS_MACOS
    app.setWindowIcon(QIcon(":/resources/icons/FlySightViewer.png"));
#endif

    // Initiallize KDDockWidgets front end
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);

    // Fusion looks better in general
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    FlySight::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
