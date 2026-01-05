#include "mapwidget.h"

#include "trackmapmodel.h"
#include "mapcursordotmodel.h"
#include "mapcursorproxy.h"
#include "sessionmodel.h"

#include <QQuickWidget>
#include <QQmlContext>
#include <QQmlEngine>
#include <QVBoxLayout>
#include <QDebug>

namespace FlySight {

MapWidget::MapWidget(SessionModel *sessionModel, CursorModel *cursorModel, QWidget *parent)
    : QWidget(parent)
    , m_cursorModel(cursorModel)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Model that converts visible sessions into QML-friendly polyline paths
    m_trackModel = new TrackMapModel(sessionModel, this);

    // Model that converts CursorModel ("mouse") state into per-session map dots
    m_cursorDotModel = new MapCursorDotModel(sessionModel, m_cursorModel, this);

    // Proxy for QML to drive CursorModel from map hover
    m_cursorProxy = new MapCursorProxy(sessionModel, m_cursorModel, this);

    auto *view = new QQuickWidget(this);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Expose the model(s) + proxy to QML as context properties
    view->engine()->rootContext()->setContextProperty(QStringLiteral("trackModel"), m_trackModel);
    view->engine()->rootContext()->setContextProperty(QStringLiteral("mapCursorDots"), m_cursorDotModel);
    view->engine()->rootContext()->setContextProperty(QStringLiteral("mapCursorProxy"), m_cursorProxy);

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
