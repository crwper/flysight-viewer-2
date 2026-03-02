#ifndef ALTITUDEMARKERFEATURE_H
#define ALTITUDEMARKERFEATURE_H

#include <QObject>
#include <QStringList>

namespace FlySight {

class SessionModel;

class AltitudeMarkerManager : public QObject
{
    Q_OBJECT

public:
    explicit AltitudeMarkerManager(SessionModel *sessionModel, QObject *parent = nullptr);
    ~AltitudeMarkerManager() override;

    void registerAll();
    void refresh();

private:
    SessionModel *m_sessionModel;
    QStringList   m_registeredKeys;
};

} // namespace FlySight

#endif // ALTITUDEMARKERFEATURE_H
