#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <optional>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "profile.h"

namespace FlySight {

class ProfileManager : public QObject {
    Q_OBJECT

public:
    static ProfileManager& instance();

    // Directory where .fvprofile files are stored
    QString profileDirectory() const;

    // CRUD operations
    QVector<Profile> listProfiles() const;
    std::optional<Profile> loadProfile(const QString &id) const;
    bool saveProfile(Profile &profile);
    bool deleteProfile(const QString &id);

    // Ordering (stored in QSettings, not on disk)
    QStringList profileOrder() const;
    void setProfileOrder(const QStringList &orderedIds);

    // Built-in default profiles
    static QStringList defaultProfileResourcePaths();
    int copyDefaultProfiles(bool overwrite = false);
    void ensureDefaultProfilesExist();

signals:
    void profilesChanged();

private:
    ProfileManager();
    Q_DISABLE_COPY(ProfileManager)

    QString filenameForProfile(const Profile &profile) const;
    QString findFileForId(const QString &id) const;
};

} // namespace FlySight

#endif // PROFILEMANAGER_H
