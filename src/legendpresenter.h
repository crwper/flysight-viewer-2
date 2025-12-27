#ifndef LEGENDPRESENTER_H
#define LEGENDPRESENTER_H

#include <QObject>
#include <QPointer>
#include <QTimer>

namespace FlySight {

class SessionModel;
class PlotModel;
class CursorModel;
class PlotViewSettingsModel;
class LegendWidget;

class LegendPresenter : public QObject
{
    Q_OBJECT
public:
    explicit LegendPresenter(SessionModel *sessionModel,
                             PlotModel *plotModel,
                             CursorModel *cursorModel,
                             PlotViewSettingsModel *plotViewSettings,
                             LegendWidget *legendWidget,
                             QObject *parent = nullptr);

private slots:
    void scheduleUpdate();
    void recompute();

private:
    QPointer<SessionModel> m_sessionModel;
    QPointer<PlotModel> m_plotModel;
    QPointer<CursorModel> m_cursorModel;
    QPointer<PlotViewSettingsModel> m_plotViewSettings;
    QPointer<LegendWidget> m_legendWidget;

    QTimer m_updateTimer;
};

} // namespace FlySight

#endif // LEGENDPRESENTER_H
