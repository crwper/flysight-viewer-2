#ifndef PREFERENCESMANAGER_H
#define PREFERENCESMANAGER_H

#include <QObject>
#include <QSettings>
#include <QVariant>

namespace FlySight {

struct Preference {
    QVariant defaultValue;
};

class PreferencesManager : public QObject {
    Q_OBJECT

public:
    static PreferencesManager& instance() {
        static PreferencesManager instance;
        return instance;
    }

    void registerPreference(const QString &key, const QVariant &defaultValue) {
        m_preferences[key] = Preference{defaultValue};

        // Set the default value in QSettings if the key doesn't exist yet
        if (!m_settings.contains(key)) {
            m_settings.setValue(key, defaultValue);
        }
    }

    QVariant getValue(const QString &key) const {
        if (!m_preferences.contains(key)) {
            qWarning() << "Requested value for an unregistered preference:" << key;
            Q_ASSERT(false && "Requested value for an unregistered preference!");
        }
        return m_settings.value(key, m_preferences.value(key).defaultValue);
    }

    QVariant getDefaultValue(const QString &key) const {
        if (!m_preferences.contains(key)) {
            qWarning() << "Requested default value for an unregistered preference:" << key;
            return QVariant();
        }
        return m_preferences.value(key).defaultValue;
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
    PreferencesManager() : m_settings("FlySight", "Viewer 2") {}

    QSettings m_settings;
    QMap<QString, Preference> m_preferences;

    Q_DISABLE_COPY(PreferencesManager)
};

} // namespace FlySight

#endif // PREFERENCESMANAGER_H
