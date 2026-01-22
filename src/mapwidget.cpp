#include "mapwidget.h"

#include "trackmapmodel.h"
#include "mapcursordotmodel.h"
#include "mapcursorproxy.h"
#include "mappreferencesbridge.h"
#include "sessionmodel.h"
#include "plotrangemodel.h"

#include <QQuickWidget>
#include <QQmlContext>
#include <QQmlEngine>
#include <QVBoxLayout>
#include <QDebug>
#include <QCoreApplication>

namespace FlySight {

MapWidget::MapWidget(SessionModel *sessionModel,
                     CursorModel *cursorModel,
                     PlotRangeModel *rangeModel,
                     QWidget *parent)
    : QWidget(parent)
    , m_cursorModel(cursorModel)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Model that converts visible sessions into QML-friendly polyline paths
    // Range model is used to filter tracks to only show the visible plot range
    m_trackModel = new TrackMapModel(sessionModel, rangeModel, this);

    // Model that converts CursorModel ("mouse") state into per-session map dots
    m_cursorDotModel = new MapCursorDotModel(sessionModel, m_cursorModel, this);

    // Proxy for QML to drive CursorModel from map hover
    m_cursorProxy = new MapCursorProxy(sessionModel, m_cursorModel, this);

    // Create preferences bridge for QML
    m_preferencesBridge = new MapPreferencesBridge(this);

    auto *view = new QQuickWidget(this);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Add QML import path for deployed modules (QtLocation, QtPositioning)
    view->engine()->addImportPath(QCoreApplication::applicationDirPath() + "/qml");

    // Expose the model(s) + proxy to QML as context properties
    view->engine()->rootContext()->setContextProperty(QStringLiteral("trackModel"), m_trackModel);
    view->engine()->rootContext()->setContextProperty(QStringLiteral("mapCursorDots"), m_cursorDotModel);
    view->engine()->rootContext()->setContextProperty(QStringLiteral("mapCursorProxy"), m_cursorProxy);
    view->engine()->rootContext()->setContextProperty(QStringLiteral("mapPreferences"), m_preferencesBridge);

    view->setSource(QUrl(QStringLiteral("qrc:/qml/MapDock.qml")));

    if (view->status() == QQuickWidget::Error) {
        const auto errors = view->errors();
        for (const auto &e : errors) {
            qWarning().noquote() << "Map QML error:" << e.toString();
        }
    }

    layout->addWidget(view);
}

} // namespace FlySight
