#ifndef MAPCURSORPROXY_H
#define MAPCURSORPROXY_H

#include <QObject>
#include <QString>

namespace FlySight {

class SessionModel;
class MomentModel;

/**
 * Thin QObject that relays map hover events from JavaScript
 * (via MapBridge) to MomentModel.
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
                            MomentModel *momentModel,
                            QObject *parent = nullptr);

    Q_INVOKABLE void setMapHover(const QString &sessionId, double utcSeconds);
    Q_INVOKABLE void clearMapHover();

private:
    SessionModel *m_sessionModel = nullptr;
    MomentModel  *m_momentModel = nullptr;
};

} // namespace FlySight

#endif // MAPCURSORPROXY_H
