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

    QString xVariable() const;
    QString referenceMarkerKey() const;
    QString xAxisLabel() const;

public slots:
    void setXVariable(const QString &xVariable);
    void setReferenceMarkerKey(const QString &key);

signals:
    void xVariableChanged(const QString &xVariable);
    void referenceMarkerKeyChanged(const QString &oldKey, const QString &newKey);

private:
    QSettings *m_settings = nullptr;
    QString m_xVariable;
    QString m_referenceMarkerKey;
};

} // namespace FlySight

#endif // PLOTVIEWSETTINGSMODEL_H
