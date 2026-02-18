#ifndef MAPCURSORPROXY_H
#define MAPCURSORPROXY_H

#include <QObject>
#include <QString>

namespace FlySight {

class SessionModel;
class CursorModel;

/**
 * Thin QObject that relays map hover events from JavaScript
 * (via MapBridge) to CursorModel.
 *
 * Called by MapBridge:
 *   - setMapHover(sessionId, utcSeconds)
 *   - clearMapHover()
 */
class MapCursorProxy : public QObject
{
    Q_OBJECT
public:
    explicit MapCursorProxy(SessionModel *sessionModel,
                            CursorModel *cursorModel,
                            QObject *parent = nullptr);

    Q_INVOKABLE void setMapHover(const QString &sessionId, double utcSeconds);
    Q_INVOKABLE void clearMapHover();

private:
    SessionModel *m_sessionModel = nullptr;
    CursorModel  *m_cursorModel = nullptr;
};

} // namespace FlySight

#endif // MAPCURSORPROXY_H
