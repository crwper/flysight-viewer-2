#include "MapCursorProxy.h"

#include "sessionmodel.h"
#include "cursormodel.h"

#include <QSet>
#include <cmath>

namespace FlySight {

static const QString kMouseCursorId = QStringLiteral("mouse");

MapCursorProxy::MapCursorProxy(SessionModel *sessionModel,
                               CursorModel *cursorModel,
                               QObject *parent)
    : QObject(parent)
    , m_sessionModel(sessionModel)
    , m_cursorModel(cursorModel)
{
}

void MapCursorProxy::setMapHover(const QString &sessionId, double utcSeconds)
{
    if (!m_cursorModel)
        return;

    if (sessionId.isEmpty() || !std::isfinite(utcSeconds)) {
        clearMapHover();
        return;
    }

    m_cursorModel->setCursorState(kMouseCursorId,
                                   QSet<QString>{ sessionId },
                                   utcSeconds,
                                   true);

    // Optional (spec): keep the hovered-session highlight consistent
    if (m_sessionModel)
        m_sessionModel->setHoveredSessionId(sessionId);
}

void MapCursorProxy::clearMapHover()
{
    if (m_cursorModel) {
        m_cursorModel->setCursorState(kMouseCursorId,
                                      QSet<QString>{},
                                      0.0,
                                      false);
    }

    if (m_sessionModel)
        m_sessionModel->setHoveredSessionId(QString());
}

} // namespace FlySight
