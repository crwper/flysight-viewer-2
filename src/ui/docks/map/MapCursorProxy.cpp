#include "MapCursorProxy.h"

#include "sessionmodel.h"
#include "momentmodel.h"

#include <QSet>
#include <cmath>

namespace FlySight {

static const QString kMouseMomentId = QStringLiteral("mouse");

MapCursorProxy::MapCursorProxy(SessionModel *sessionModel,
                               MomentModel *momentModel,
                               QObject *parent)
    : QObject(parent)
    , m_sessionModel(sessionModel)
    , m_momentModel(momentModel)
{
}

void MapCursorProxy::setMapHover(const QString &sessionId, double utcSeconds)
{
    if (!m_momentModel)
        return;

    if (sessionId.isEmpty() || !std::isfinite(utcSeconds)) {
        clearMapHover();
        return;
    }

    m_momentModel->setMomentPosition(kMouseMomentId,
                                      utcSeconds,
                                      QSet<QString>{ sessionId },
                                      true);

    // Keep the hovered-session highlight consistent
    if (m_sessionModel)
        m_sessionModel->setHoveredSessionId(sessionId);
}

void MapCursorProxy::clearMapHover()
{
    if (m_momentModel) {
        m_momentModel->setMomentPosition(kMouseMomentId,
                                          0.0,
                                          QSet<QString>{},
                                          false);
    }

    if (m_sessionModel)
        m_sessionModel->setHoveredSessionId(QString());
}

} // namespace FlySight
