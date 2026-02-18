#include "cursormodel.h"

#include <QStringList>

namespace FlySight {

CursorModel::CursorModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CursorModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_cursors.size();
}

QVariant CursorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return {};

    const int row = index.row();
    if (row < 0 || row >= m_cursors.size())
        return {};

    const Cursor &c = m_cursors.at(row);

    switch (role) {
    case Qt::DisplayRole:
        return c.label.isEmpty() ? c.id : c.label;

    case IdRole:
        return c.id;
    case LabelRole:
        return c.label;
    case TypeRole:
        return static_cast<int>(c.type);
    case ActiveRole:
        return c.active;

    case PositionSpaceRole:
        return static_cast<int>(c.positionSpace);
    case PositionValueRole:
        return c.positionValue;
    case AxisKeyRole:
        return c.axisKey;

    case TargetPolicyRole:
        return static_cast<int>(c.targetPolicy);

    case TargetSessionsRole: {
        QStringList ids = QStringList(c.targetSessions.values());
        ids.sort();
        return ids;
    }

    default:
        return {};
    }
}

QHash<int, QByteArray> CursorModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
    roles[IdRole] = "id";
    roles[LabelRole] = "label";
    roles[TypeRole] = "type";
    roles[ActiveRole] = "active";
    roles[PositionSpaceRole] = "positionSpace";
    roles[PositionValueRole] = "positionValue";
    roles[AxisKeyRole] = "axisKey";
    roles[TargetPolicyRole] = "targetPolicy";
    roles[TargetSessionsRole] = "targetSessions";
    return roles;
}

int CursorModel::rowForId(const QString &id) const
{
    auto it = m_rowById.constFind(id);
    if (it == m_rowById.constEnd())
        return -1;

    const int row = it.value();
    if (row < 0 || row >= m_cursors.size())
        return -1;

    // Sanity check
    if (m_cursors.at(row).id != id)
        return -1;

    return row;
}

bool CursorModel::hasCursor(const QString &id) const
{
    return rowForId(id) >= 0;
}

CursorModel::Cursor CursorModel::cursorById(const QString &id) const
{
    const int row = rowForId(id);
    if (row < 0)
        return Cursor{};
    return m_cursors.at(row);
}

int CursorModel::ensureCursor(const Cursor &initial)
{
    if (initial.id.isEmpty())
        return -1;

    const int existing = rowForId(initial.id);
    if (existing >= 0)
        return existing;

    const int newRow = m_cursors.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_cursors.push_back(initial);
    m_rowById.insert(initial.id, newRow);
    endInsertRows();

    emit cursorsChanged();
    return newRow;
}

void CursorModel::updateCursor(const Cursor &updated)
{
    if (updated.id.isEmpty())
        return;

    const int row = rowForId(updated.id);
    if (row < 0) {
        ensureCursor(updated);
        return;
    }

    m_cursors[row] = updated;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
    emit cursorsChanged();
}

void CursorModel::setCursorActive(const QString &id, bool active)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Cursor &c = m_cursors[row];
    if (c.active == active)
        return;

    c.active = active;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {ActiveRole});
    emit cursorsChanged();
}

void CursorModel::setCursorPositionPlotAxis(const QString &id, const QString &axisKey, double x)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Cursor &c = m_cursors[row];

    const bool changedSpace = (c.positionSpace != PositionSpace::PlotAxisCoord);
    const bool changedAxis  = (c.axisKey != axisKey);
    const bool changedVal   = (c.positionValue != x);

    if (!changedSpace && !changedAxis && !changedVal)
        return;

    c.positionSpace = PositionSpace::PlotAxisCoord;
    c.axisKey = axisKey;
    c.positionValue = x;

    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {PositionSpaceRole, AxisKeyRole, PositionValueRole});
    emit cursorsChanged();
}

void CursorModel::setCursorPositionUtc(const QString &id, double utcSeconds)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Cursor &c = m_cursors[row];

    const bool changedSpace = (c.positionSpace != PositionSpace::UtcSeconds);
    const bool changedAxis  = (!c.axisKey.isEmpty());
    const bool changedVal   = (c.positionValue != utcSeconds);

    if (!changedSpace && !changedAxis && !changedVal)
        return;

    c.positionSpace = PositionSpace::UtcSeconds;
    c.axisKey.clear();
    c.positionValue = utcSeconds;

    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {PositionSpaceRole, AxisKeyRole, PositionValueRole});
    emit cursorsChanged();
}

void CursorModel::setCursorTargetsExplicit(const QString &id, const QSet<QString> &sessionIds)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Cursor &c = m_cursors[row];

    const bool changedPolicy = (c.targetPolicy != TargetPolicy::Explicit);
    const bool changedIds = (c.targetSessions != sessionIds);

    if (!changedPolicy && !changedIds)
        return;

    c.targetPolicy = TargetPolicy::Explicit;
    c.targetSessions = sessionIds;

    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {TargetPolicyRole, TargetSessionsRole});
    emit cursorsChanged();
}

void CursorModel::setCursorTargetPolicy(const QString &id, TargetPolicy policy)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Cursor &c = m_cursors[row];
    if (c.targetPolicy == policy)
        return;

    c.targetPolicy = policy;

    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {TargetPolicyRole});
    emit cursorsChanged();
}

void CursorModel::setCursorState(const QString &id,
                                  const QSet<QString> &targetSessions,
                                  double utcSeconds,
                                  bool active)
{
    const int row = rowForId(id);
    if (row < 0)
        return;

    Cursor &c = m_cursors[row];

    // Check each field group for changes
    const bool changedTargetPolicy = (c.targetPolicy != TargetPolicy::Explicit);
    const bool changedTargetIds    = (c.targetSessions != targetSessions);
    const bool changedPosSpace     = (c.positionSpace != PositionSpace::UtcSeconds);
    const bool changedPosAxis      = (!c.axisKey.isEmpty());
    const bool changedPosValue     = (c.positionValue != utcSeconds);
    const bool changedActive       = (c.active != active);

    if (!changedTargetPolicy && !changedTargetIds
        && !changedPosSpace && !changedPosAxis && !changedPosValue
        && !changedActive) {
        return; // nothing changed -- skip emission
    }

    // Apply all mutations
    c.targetPolicy   = TargetPolicy::Explicit;
    c.targetSessions = targetSessions;
    c.positionSpace  = PositionSpace::UtcSeconds;
    c.axisKey.clear();
    c.positionValue  = utcSeconds;
    c.active         = active;

    // Emit dataChanged once with the union of all affected roles
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {TargetPolicyRole, TargetSessionsRole,
                                PositionSpaceRole, AxisKeyRole,
                                PositionValueRole, ActiveRole});
    emit cursorsChanged();
}

} // namespace FlySight
