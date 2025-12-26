#ifndef PLOTVIEWSETTINGSMODEL_H
#define PLOTVIEWSETTINGSMODEL_H

#include <QObject>
#include <QSettings>

namespace FlySight {

class PlotViewSettingsModel : public QObject
{
    Q_OBJECT

public:
    explicit PlotViewSettingsModel(QSettings *settings, QObject *parent = nullptr);

    QString xAxisKey() const;
    QString xAxisLabel() const;

public slots:
    void setXAxis(const QString &key, const QString &label);

signals:
    void xAxisChanged(const QString &key, const QString &label);

private:
    QString defaultLabelForKey(const QString &key) const;

    QSettings *m_settings = nullptr;
    QString m_xAxisKey;
    QString m_xAxisLabel;
};

} // namespace FlySight

#endif // PLOTVIEWSETTINGSMODEL_H
