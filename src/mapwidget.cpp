#include "mapwidget.h"

#include "trackmapmodel.h"
#include "sessionmodel.h"

#include <QQuickWidget>
#include <QQmlContext>
#include <QQmlEngine>
#include <QVBoxLayout>
#include <QDebug>

namespace FlySight {

MapWidget::MapWidget(SessionModel *sessionModel, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Model that converts visible sessions into QML-friendly polyline paths
    m_trackModel = new TrackMapModel(sessionModel, this);

    auto *view = new QQuickWidget(this);
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Expose the model to QML as a context property
    view->engine()->rootContext()->setContextProperty(QStringLiteral("trackModel"), m_trackModel);

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
