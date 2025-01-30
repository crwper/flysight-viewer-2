#ifndef PREFERENCESMANAGER_H
#define PREFERENCESMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariant>

class PreferencesManager : public QObject {
    Q_OBJECT

public:
    static PreferencesManager& instance() {
        static PreferencesManager instance;
        return instance;
    }

    QVariant getValue(const QString &key, const QVariant &defaultValue = QVariant()) {
        return m_settings.value(key, defaultValue);
    }

    void setValue(const QString &key, const QVariant &value) {
        if (m_settings.value(key) != value) {
            m_settings.setValue(key, value);
            emit preferenceChanged(key, value);
        }
    }

signals:
    void preferenceChanged(const QString &key, const QVariant &value);

private:
    PreferencesManager() : m_settings("MyCompany", "MyApp") {}
    QSettings m_settings;

    Q_DISABLE_COPY(PreferencesManager)
};

#endif // PREFERENCESMANAGER_H
