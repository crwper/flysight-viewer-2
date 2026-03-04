#include "momentmodel.h"

#include "dependencykey.h"
#include "sessionmodel.h"

namespace FlySight {

MomentModel::MomentModel(QObject *parent)
    : QObject(parent)
{
}

// ─────────────────────────────── Registration

void MomentModel::registerMoment(const QString &id, const QString &label,
                                  const MomentTraits &traits,
                                  const QString &groupId)
{
    if (id.isEmpty())
        return;

    // No-op if id already exists
    if (m_indexById.contains(id))
        return;

    Moment m;
    m.id = id;
    m.label = label;
    m.traits = traits;
    m.groupId = groupId;

    const int newIndex = m_moments.size();
    m_moments.push_back(m);
    m_indexById.insert(id, newIndex);

    rebuildWatchedKeys();
    emit momentsChanged();
}

void MomentModel::registerMoments(const QVector<MomentRegistration> &moments)
{
    bool added = false;

    for (const MomentRegistration &reg : moments) {
        if (reg.id.isEmpty())
            continue;

        // Skip duplicates
        if (m_indexById.contains(reg.id))
            continue;

        Moment m;
        m.id = reg.id;
        m.label = reg.label;
        m.traits = reg.traits;
        m.groupId = reg.groupId;

        const int newIndex = m_moments.size();
        m_moments.push_back(m);
        m_indexById.insert(reg.id, newIndex);
        added = true;
    }

    if (added) {
        rebuildWatchedKeys();
        emit momentsChanged();
    }
}

void MomentModel::unregisterMoment(const QString &id)
{
    const int idx = indexForId(id);
    if (idx < 0)
        return;

    m_moments.removeAt(idx);
    rebuildIndex();
    rebuildWatchedKeys();
    emit momentsChanged();
}

void MomentModel::unregisterMomentGroup(const QString &groupId)
{
    if (groupId.isEmpty())
        return;

    bool removed = false;
    for (int i = m_moments.size() - 1; i >= 0; --i) {
        if (m_moments.at(i).groupId == groupId) {
            m_moments.removeAt(i);
            removed = true;
        }
    }

    if (removed) {
        rebuildIndex();
        rebuildWatchedKeys();
        emit momentsChanged();
    }
}

// ─────────────────────────────── Queries

bool MomentModel::hasMoment(const QString &id) const
{
    return indexForId(id) >= 0;
}

MomentModel::Moment MomentModel::momentById(const QString &id) const
{
    const int idx = indexForId(id);
    if (idx < 0)
        return Moment{};
    return m_moments.at(idx);
}

QVector<MomentModel::Moment> MomentModel::allMoments() const
{
    return m_moments;
}

QVector<MomentModel::Moment> MomentModel::enabledMoments() const
{
    QVector<Moment> result;
    result.reserve(m_moments.size());
    for (const Moment &m : m_moments) {
        if (m.enabled)
            result.push_back(m);
    }
    return result;
}

// ─────────────────────────────── Position updates

void MomentModel::setMomentPosition(const QString &id, double utcSeconds,
                                     const QSet<QString> &targetSessions, bool active)
{
    // Change-detection: only emit if something actually changed
    const int idx = indexForId(id);
    if (idx < 0)
        return;

    Moment &m = m_moments[idx];

    const bool changedPos     = (m.positionUtc != utcSeconds);
    const bool changedTargets = (m.targetSessions != targetSessions);
    const bool changedActive  = (m.active != active);

    if (!changedPos && !changedTargets && !changedActive)
        return;

    m.positionUtc = utcSeconds;
    m.targetSessions = targetSessions;
    m.active = active;

    emit momentsChanged();
}

void MomentModel::setMomentActive(const QString &id, bool active)
{
    // Guard: only emit if the value actually changed
    const int idx = indexForId(id);
    if (idx < 0)
        return;

    Moment &m = m_moments[idx];
    if (m.active == active)
        return;

    m.active = active;
    emit momentsChanged();
}

// ─────────────────────────────── Enablement

void MomentModel::setMomentEnabled(const QString &id, bool enabled)
{
    // Mouse moment is always enabled
    if (id == QLatin1String("mouse") && !enabled)
        return;

    const int idx = indexForId(id);
    if (idx < 0)
        return;

    Moment &m = m_moments[idx];
    if (m.enabled == enabled)
        return;

    m.enabled = enabled;
    emit momentsChanged();
}

bool MomentModel::isMomentEnabled(const QString &id) const
{
    const int idx = indexForId(id);
    if (idx < 0)
        return false;
    return m_moments.at(idx).enabled;
}

// ─────────────────────────────── Dependency wiring

void MomentModel::setSessionModel(SessionModel *sessionModel)
{
    if (m_sessionModel == sessionModel)
        return;

    if (m_sessionModel) {
        disconnect(m_sessionModel, &SessionModel::dependencyChanged,
                   this, &MomentModel::onDependencyChanged);
    }

    m_sessionModel = sessionModel;

    if (m_sessionModel) {
        connect(m_sessionModel, &SessionModel::dependencyChanged,
                this, &MomentModel::onDependencyChanged);
    }
}

void MomentModel::onDependencyChanged(const QString &/*sessionId*/,
                                       const DependencyKey &key)
{
    // Only react to attribute changes that match a watched key
    if (key.type != DependencyKey::Type::Attribute)
        return;

    if (m_watchedAttributeKeys.contains(key.attributeKey))
        emit momentsChanged();
}

// ─────────────────────────────── Private helpers

int MomentModel::indexForId(const QString &id) const
{
    auto it = m_indexById.constFind(id);
    if (it == m_indexById.constEnd())
        return -1;

    const int idx = it.value();
    if (idx < 0 || idx >= m_moments.size())
        return -1;

    // Sanity check
    if (m_moments.at(idx).id != id)
        return -1;

    return idx;
}

void MomentModel::rebuildIndex()
{
    m_indexById.clear();
    m_indexById.reserve(m_moments.size());
    for (int i = 0; i < m_moments.size(); ++i) {
        m_indexById.insert(m_moments.at(i).id, i);
    }
}

void MomentModel::rebuildWatchedKeys()
{
    m_watchedAttributeKeys.clear();
    for (const Moment &m : std::as_const(m_moments)) {
        if (m.traits.positionSource == PositionSource::Attribute
            && !m.traits.attributeKey.isEmpty()) {
            m_watchedAttributeKeys.insert(m.traits.attributeKey);
        }
    }
}

} // namespace FlySight
