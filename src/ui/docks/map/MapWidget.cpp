#include "MapWidget.h"

#include "MapBridge.h"
#include "TrackMapModel.h"
#include "MapCursorDotModel.h"
#include "MapCursorProxy.h"
#include "MapPreferencesBridge.h"
#include "sessionmodel.h"
#include "plotrangemodel.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebChannel>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QUrl>
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>

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

    // Model that converts visible sessions into polyline paths
    // Range model is used to filter tracks to only show the visible plot range
    m_trackModel = new TrackMapModel(sessionModel, rangeModel, this);

    // Model that converts CursorModel ("mouse") state into per-session map dots
    m_cursorDotModel = new MapCursorDotModel(sessionModel, m_cursorModel, this);

    // Proxy for JS to drive CursorModel from map hover
    m_cursorProxy = new MapCursorProxy(sessionModel, m_cursorModel, this);

    // Create preferences bridge
    m_preferencesBridge = new MapPreferencesBridge(this);

    // Create the bridge object for QWebChannel communication
    m_bridge = new MapBridge(m_cursorProxy, m_preferencesBridge, this);

    // Create QWebChannel and register the bridge as "bridge"
    m_channel = new QWebChannel(this);
    m_channel->registerObject(QStringLiteral("bridge"), m_bridge);

    // Create QWebEngineView, set the channel on its page, and load map.html
    m_webView = new QWebEngineView(this);
    m_webView->page()->setWebChannel(m_channel);

#ifdef Q_OS_MACOS
    const QString htmlPath = QCoreApplication::applicationDirPath()
                           + QStringLiteral("/../Resources/resources/map.html");
#else
    const QString htmlPath = QCoreApplication::applicationDirPath()
                           + QStringLiteral("/resources/map.html");
#endif
    m_webView->load(QUrl::fromLocalFile(htmlPath));

    layout->addWidget(m_webView);

    // Connect model reset signals to serialization slots
    connect(m_trackModel, &QAbstractItemModel::modelReset,
            this, &MapWidget::onTracksReset);
    connect(m_cursorDotModel, &QAbstractItemModel::modelReset,
            this, &MapWidget::onCursorDotsReset);
    connect(m_trackModel, &TrackMapModel::boundsChanged,
            this, &MapWidget::onBoundsChanged);

    // When JS requests initial data (after page load), push everything
    connect(m_bridge, &MapBridge::initialDataRequested,
            this, &MapWidget::pushAllData);

    // Connect preference change signals to bridge push calls
    connect(m_preferencesBridge, &MapPreferencesBridge::lineThicknessChanged, this, [this]() {
        m_bridge->pushPreference(QStringLiteral("map/lineThickness"),
                                 m_preferencesBridge->lineThickness());
    });
    connect(m_preferencesBridge, &MapPreferencesBridge::markerSizeChanged, this, [this]() {
        m_bridge->pushPreference(QStringLiteral("map/markerSize"),
                                 m_preferencesBridge->markerSize());
    });
    connect(m_preferencesBridge, &MapPreferencesBridge::trackOpacityChanged, this, [this]() {
        m_bridge->pushPreference(QStringLiteral("map/trackOpacity"),
                                 m_preferencesBridge->trackOpacity());
    });
    connect(m_preferencesBridge, &MapPreferencesBridge::mapTypeIndexChanged, this, [this]() {
        m_bridge->pushPreference(QStringLiteral("map/type"),
                                 m_preferencesBridge->mapTypeIndex());
    });
}

void MapWidget::onTracksReset()
{
    const int n = m_trackModel->rowCount();
    QJsonArray tracks;

    for (int i = 0; i < n; ++i) {
        const QModelIndex idx = m_trackModel->index(i, 0);

        const QString sessionId =
            m_trackModel->data(idx, TrackMapModel::SessionIdRole).toString();
        const QVariantList pointsList =
            m_trackModel->data(idx, TrackMapModel::TrackPointsRole).toList();
        const QColor color =
            m_trackModel->data(idx, TrackMapModel::TrackColorRole).value<QColor>();

        QJsonArray points;
        for (const QVariant &pt : pointsList) {
            const QVariantMap m = pt.toMap();
            QJsonObject jpt;
            jpt.insert(QStringLiteral("lat"), m.value(QStringLiteral("lat")).toDouble());
            jpt.insert(QStringLiteral("lon"), m.value(QStringLiteral("lon")).toDouble());
            jpt.insert(QStringLiteral("t"),   m.value(QStringLiteral("t")).toDouble());
            points.append(jpt);
        }

        QJsonObject track;
        track.insert(QStringLiteral("sessionId"), sessionId);
        track.insert(QStringLiteral("points"), points);
        track.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
        tracks.append(track);
    }

    m_bridge->pushTracks(tracks);
}

void MapWidget::onCursorDotsReset()
{
    const int n = m_cursorDotModel->rowCount();
    QJsonArray dots;

    for (int i = 0; i < n; ++i) {
        const QModelIndex idx = m_cursorDotModel->index(i, 0);

        const QString sessionId =
            m_cursorDotModel->data(idx, MapCursorDotModel::SessionIdRole).toString();
        const double lat =
            m_cursorDotModel->data(idx, MapCursorDotModel::LatRole).toDouble();
        const double lon =
            m_cursorDotModel->data(idx, MapCursorDotModel::LonRole).toDouble();
        const QColor color =
            m_cursorDotModel->data(idx, MapCursorDotModel::ColorRole).value<QColor>();

        QJsonObject dot;
        dot.insert(QStringLiteral("sessionId"), sessionId);
        dot.insert(QStringLiteral("lat"), lat);
        dot.insert(QStringLiteral("lon"), lon);
        dot.insert(QStringLiteral("color"), color.name(QColor::HexArgb));
        dots.append(dot);
    }

    m_bridge->pushCursorDots(dots);
}

void MapWidget::onBoundsChanged()
{
    if (!m_trackModel->hasData())
        return;

    m_bridge->pushFitBounds(m_trackModel->boundsSouth(),
                            m_trackModel->boundsWest(),
                            m_trackModel->boundsNorth(),
                            m_trackModel->boundsEast());
}

void MapWidget::pushAllData()
{
    onTracksReset();
    onCursorDotsReset();
    onBoundsChanged();
    m_bridge->pushPreference(QStringLiteral("map/lineThickness"),
                             m_preferencesBridge->lineThickness());
    m_bridge->pushPreference(QStringLiteral("map/markerSize"),
                             m_preferencesBridge->markerSize());
    m_bridge->pushPreference(QStringLiteral("map/trackOpacity"),
                             m_preferencesBridge->trackOpacity());
    m_bridge->pushPreference(QStringLiteral("map/type"),
                             m_preferencesBridge->mapTypeIndex());
}

} // namespace FlySight
