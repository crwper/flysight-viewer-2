#include "profilemanager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>

using namespace FlySight;

static const QString kOrderSettingsKey = QStringLiteral("profiles/order");
static const QString kProfileExtension = QStringLiteral(".fvprofile");

// ============================================================================
// Singleton
// ============================================================================

ProfileManager& ProfileManager::instance()
{
    static ProfileManager manager;
    return manager;
}

ProfileManager::ProfileManager()
    : QObject(nullptr)
{
}

// ============================================================================
// Directory
// ============================================================================

QString ProfileManager::profileDirectory() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        + QStringLiteral("/FlySight Viewer/profiles");
    QDir().mkpath(dir);
    return dir;
}

// ============================================================================
// Filename helpers
// ============================================================================

QString ProfileManager::filenameForProfile(const Profile &profile) const
{
    // Slugify the display name
    QString slug = profile.displayName.trimmed();
    if (slug.isEmpty())
        slug = QStringLiteral("untitled");

    // Replace non-alphanumeric characters with underscores
    QString result;
    result.reserve(slug.size());
    bool lastWasUnderscore = false;
    for (const QChar &ch : slug) {
        if (ch.isLetterOrNumber()) {
            result.append(ch);
            lastWasUnderscore = false;
        } else {
            if (!lastWasUnderscore) {
                result.append(QLatin1Char('_'));
                lastWasUnderscore = true;
            }
        }
    }

    // Trim trailing underscores
    while (result.endsWith(QLatin1Char('_')))
        result.chop(1);

    if (result.isEmpty())
        result = QStringLiteral("untitled");

    // Append 4-hex-char UUID suffix
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString suffix = uuid.left(4);
    result += QLatin1Char('_') + suffix;

    return result + kProfileExtension;
}

QString ProfileManager::findFileForId(const QString &id) const
{
    if (id.isEmpty())
        return QString();

    const QDir dir(profileDirectory());
    const QString expected = id + kProfileExtension;

    if (dir.exists(expected))
        return dir.absoluteFilePath(expected);

    return QString();
}

// ============================================================================
// Listing
// ============================================================================

QVector<Profile> ProfileManager::listProfiles() const
{
    QVector<Profile> profiles;
    const QDir dir(profileDirectory());
    const QStringList files = dir.entryList(
        QStringList() << QStringLiteral("*.fvprofile"),
        QDir::Files, QDir::Name);

    for (const QString &filename : files) {
        const QString filePath = dir.absoluteFilePath(filename);
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning("ProfileManager: failed to open %s", qPrintable(filePath));
            continue;
        }

        const QByteArray data = file.readAll();
        file.close();

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning("ProfileManager: failed to parse %s: %s",
                     qPrintable(filePath),
                     qPrintable(parseError.errorString()));
            continue;
        }

        // ID is the filename stem (without extension)
        const QString id = QFileInfo(filename).completeBaseName();
        Profile profile = profileFromJson(doc.object(), id);
        profiles.append(profile);
    }

    return profiles;
}

// ============================================================================
// Loading
// ============================================================================

std::optional<Profile> ProfileManager::loadProfile(const QString &id) const
{
    const QString filePath = findFileForId(id);
    if (filePath.isEmpty())
        return std::nullopt;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("ProfileManager: failed to open %s for reading", qPrintable(filePath));
        return std::nullopt;
    }

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning("ProfileManager: failed to parse %s: %s",
                 qPrintable(filePath),
                 qPrintable(parseError.errorString()));
        return std::nullopt;
    }

    return profileFromJson(doc.object(), id);
}

// ============================================================================
// Saving
// ============================================================================

bool ProfileManager::saveProfile(Profile &profile)
{
    const QString dir = profileDirectory();

    QString filePath;
    if (profile.id.isEmpty()) {
        // New profile — generate filename and assign ID
        const QString filename = filenameForProfile(profile);
        profile.id = QFileInfo(filename).completeBaseName();
        filePath = dir + QLatin1Char('/') + filename;
    } else {
        // Existing profile — overwrite the existing file
        filePath = findFileForId(profile.id);
        if (filePath.isEmpty()) {
            // File was deleted externally; recreate using ID as filename
            filePath = dir + QLatin1Char('/') + profile.id + kProfileExtension;
        }
    }

    const QJsonObject obj = profileToJson(profile);
    const QJsonDocument doc(obj);

    // Atomic write via QSaveFile
    QSaveFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        qWarning("ProfileManager: failed to open %s for writing", qPrintable(filePath));
        return false;
    }

    saveFile.write(doc.toJson(QJsonDocument::Indented));
    if (!saveFile.commit()) {
        qWarning("ProfileManager: failed to commit write to %s", qPrintable(filePath));
        return false;
    }

    emit profilesChanged();
    return true;
}

// ============================================================================
// Deleting
// ============================================================================

bool ProfileManager::deleteProfile(const QString &id)
{
    const QString filePath = findFileForId(id);
    if (filePath.isEmpty()) {
        qWarning("ProfileManager: no file found for profile ID '%s'", qPrintable(id));
        return false;
    }

    if (!QFile::remove(filePath)) {
        qWarning("ProfileManager: failed to delete %s", qPrintable(filePath));
        return false;
    }

    // Remove from the stored order
    QSettings settings;
    QStringList order = settings.value(kOrderSettingsKey).toStringList();
    order.removeAll(id);
    settings.setValue(kOrderSettingsKey, order);

    emit profilesChanged();
    return true;
}

// ============================================================================
// Ordering
// ============================================================================

QStringList ProfileManager::profileOrder() const
{
    QSettings settings;
    return settings.value(kOrderSettingsKey).toStringList();
}

void ProfileManager::setProfileOrder(const QStringList &orderedIds)
{
    QSettings settings;
    settings.setValue(kOrderSettingsKey, orderedIds);
    emit profilesChanged();
}

// ============================================================================
// Built-in Default Profiles
// ============================================================================

QStringList ProfileManager::defaultProfileResourcePaths()
{
    return {
        QStringLiteral(":/resources/profiles/Basic_Flight.fvprofile"),
        QStringLiteral(":/resources/profiles/Canopy_Piloting.fvprofile"),
        QStringLiteral(":/resources/profiles/Wingsuit_Performance.fvprofile")
    };
}

int ProfileManager::copyDefaultProfiles(bool overwrite)
{
    const QString dir = profileDirectory();
    QDir().mkpath(dir);

    int count = 0;
    const QStringList resourcePaths = defaultProfileResourcePaths();

    for (const QString &resourcePath : resourcePaths) {
        QFile resourceFile(resourcePath);
        if (!resourceFile.open(QIODevice::ReadOnly)) {
            qWarning("ProfileManager: failed to open resource %s", qPrintable(resourcePath));
            continue;
        }

        const QByteArray contents = resourceFile.readAll();
        resourceFile.close();

        // Derive target filename from the resource path
        const QString filename = QFileInfo(resourcePath).fileName();
        const QString targetPath = dir + QLatin1Char('/') + filename;

        if (QFile::exists(targetPath)) {
            if (!overwrite)
                continue;
            QFile::remove(targetPath);
        }

        QSaveFile saveFile(targetPath);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            qWarning("ProfileManager: failed to open %s for writing", qPrintable(targetPath));
            continue;
        }

        saveFile.write(contents);
        if (!saveFile.commit()) {
            qWarning("ProfileManager: failed to commit write to %s", qPrintable(targetPath));
            continue;
        }

        ++count;
    }

    if (count > 0)
        emit profilesChanged();

    return count;
}

void ProfileManager::ensureDefaultProfilesExist()
{
    const QString dir = profileDirectory();
    const QDir profileDir(dir);

    // Check if there are any .fvprofile files already
    const QStringList existing = profileDir.entryList(
        QStringList() << QStringLiteral("*.fvprofile"),
        QDir::Files);

    if (existing.isEmpty())
        copyDefaultProfiles(false);
}
