#include "sessionmodel.h"

#include <algorithm>

#include <QDateTime>
#include <QMessageBox>
#include <QTimeZone>

#include "attributeregistry.h"
#include "logbookcolumn.h"
#include "logbookmanager.h"
#include "units/unitconverter.h"

namespace FlySight {

SessionModel::SessionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(&LogbookColumnStore::instance(), &LogbookColumnStore::columnsChanged,
            this, &SessionModel::rebuildColumns);

    connect(&UnitConverter::instance(), &UnitConverter::systemChanged, this, [this]() {
        if (m_rows.isEmpty() || m_columns.isEmpty()) return;
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole});
        emit headerDataChanged(Qt::Horizontal, 0, columnCount() - 1);
    });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(0);
    connect(&m_saveTimer, &QTimer::timeout, this, &SessionModel::saveNextSession);

    m_loadTimer.setSingleShot(true);
    m_loadTimer.setInterval(0);
    connect(&m_loadTimer, &QTimer::timeout, this, &SessionModel::loadNextSession);

    rebuildColumns();
}

void SessionModel::rebuildColumns()
{
    beginResetModel();
    m_columns = LogbookColumnStore::instance().enabledColumns();
    endResetModel();

    // Update cached values for all loaded sessions to reflect new column set
    LogbookManager &logbook = LogbookManager::instance();
    for (const SessionRow &row : std::as_const(m_rows)) {
        if (row.isLoaded()) {
            logbook.setCachedValues(row.sessionId, computeColumnValues(row.session.value()));
        }
    }
    if (!m_rows.isEmpty()) {
        logbook.flushIndex();
    }
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_rows.size();
}

int SessionModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return m_columns.size();
}

QVariant SessionModel::formatAttributeValue(const SessionData &session, const LogbookColumn &col) const
{
    QVariant value = session.getAttribute(col.attributeKey);
    if (!value.isValid())
        return QVariant();

    // Look up the attribute definition to determine formatting
    const auto *def = AttributeRegistry::instance().findByKey(col.attributeKey);
    AttributeFormatType formatType = def ? def->formatType : AttributeFormatType::Text;

    switch (formatType) {
    case AttributeFormatType::Text:
        return value.toString();

    case AttributeFormatType::DateTime: {
        bool ok = false;
        double utcSeconds = value.toDouble(&ok);
        if (!ok)
            return QVariant();
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(
            qint64(utcSeconds * 1000.0), QTimeZone::utc());
        return dt.toString("yyyy/MM/dd HH:mm:ss");
    }

    case AttributeFormatType::Duration: {
        bool ok = false;
        double durationSec = value.toDouble(&ok);
        if (!ok) return QVariant();
        int totalSec = static_cast<int>(durationSec);
        int minutes = totalSec / 60;
        int seconds = totalSec % 60;
        return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }

    case AttributeFormatType::Double: {
        bool ok = false;
        double val = value.toDouble(&ok);
        if (!ok) return QVariant();
        if (def && !def->measurementType.isEmpty())
            return UnitConverter::instance().formatValue(val, def->measurementType);
        return QString::number(val);
    }
    }

    return QVariant();
}

QVariant SessionModel::formatMeasurementValue(const SessionData &session, const LogbookColumn &col) const
{
    QString interpKey = SessionData::interpolationKey(
        col.markerAttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
    QVariant value = session.getAttribute(interpKey);
    if (!value.isValid())
        return QVariant();

    return UnitConverter::instance().formatValue(value.toDouble(), col.measurementType);
}

QVariant SessionModel::formatDeltaValue(const SessionData &session, const LogbookColumn &col) const
{
    QString interpKey1 = SessionData::interpolationKey(
        col.markerAttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
    QString interpKey2 = SessionData::interpolationKey(
        col.marker2AttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);

    QVariant value1 = session.getAttribute(interpKey1);
    QVariant value2 = session.getAttribute(interpKey2);

    if (!value1.isValid() || !value2.isValid())
        return QVariant();

    double delta = value2.toDouble() - value1.toDouble();
    return UnitConverter::instance().formatValue(delta, col.measurementType);
}

QVariant SessionModel::formatRawValue(const QVariant &rawValue, const LogbookColumn &col) const
{
    if (!rawValue.isValid())
        return QVariant();

    switch (col.type) {
    case ColumnType::SessionAttribute: {
        const auto *def = AttributeRegistry::instance().findByKey(col.attributeKey);
        AttributeFormatType formatType = def ? def->formatType : AttributeFormatType::Text;

        switch (formatType) {
        case AttributeFormatType::Text:
            return rawValue.toString();

        case AttributeFormatType::DateTime: {
            bool ok = false;
            double utcSeconds = rawValue.toDouble(&ok);
            if (!ok) return QVariant();
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(
                qint64(utcSeconds * 1000.0), QTimeZone::utc());
            return dt.toString("yyyy/MM/dd HH:mm:ss");
        }

        case AttributeFormatType::Duration: {
            bool ok = false;
            double durationSec = rawValue.toDouble(&ok);
            if (!ok) return QVariant();
            int totalSec = static_cast<int>(durationSec);
            int minutes = totalSec / 60;
            int seconds = totalSec % 60;
            return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
        }

        case AttributeFormatType::Double: {
            bool ok = false;
            double val = rawValue.toDouble(&ok);
            if (!ok) return QVariant();
            if (def && !def->measurementType.isEmpty())
                return UnitConverter::instance().formatValue(val, def->measurementType);
            return QString::number(val);
        }
        }
        break;
    }
    case ColumnType::MeasurementAtMarker:
    case ColumnType::Delta:
        return UnitConverter::instance().formatValue(rawValue.toDouble(), col.measurementType);
    }

    return QVariant();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size() || index.column() >= m_columns.size())
        return QVariant();

    const SessionRow &row = m_rows.at(index.row());
    const LogbookColumn &col = m_columns[index.column()];

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        if (row.isLoaded()) {
            // Loaded row: use existing formatting logic
            const SessionData &item = row.session.value();
            switch (col.type) {
            case ColumnType::SessionAttribute:
                return formatAttributeValue(item, col);
            case ColumnType::MeasurementAtMarker:
                return formatMeasurementValue(item, col);
            case ColumnType::Delta:
                return formatDeltaValue(item, col);
            }
        } else {
            // Stub row: use cached values
            QVariant cached = row.cachedValues.value(index.column());
            if (!cached.isValid())
                return QVariant();
            return formatRawValue(cached, col);
        }
        break;

    case Qt::CheckStateRole:
        // Handle checkbox only for the first column
        if (index.column() == 0) {
            return row.visible ? Qt::Checked : Qt::Unchecked;
        }
        break;

    case CustomRoles::IsHoveredRole:
        return (row.sessionId == m_hoveredSessionId);

    default:
        break;
    }

    return QVariant();
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || section >= m_columns.size())
        return QVariant();

    if (role == Qt::DisplayRole) {
        QString label = logbookColumnLabel(m_columns[section]);
        QString unit = columnUnitLabel(m_columns[section]);
        if (!unit.isEmpty())
            return label + QStringLiteral("\n(") + unit + QStringLiteral(")");
        return label;
    }
    if (role == Qt::TextAlignmentRole)
        return QVariant::fromValue(Qt::AlignCenter);
    return QVariant();
}

Qt::ItemFlags SessionModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == 0) {
        // The first column has a checkbox
        flags |= Qt::ItemIsUserCheckable;
    }

    const LogbookColumn &col = m_columns[index.column()];
    if (col.type == ColumnType::SessionAttribute) {
        const auto *def = AttributeRegistry::instance().findByKey(col.attributeKey);
        if (def && def->editable)
            flags |= Qt::ItemIsEditable;
    }
    // MeasurementAtMarker and Delta are never editable

    return flags;
}

bool SessionModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.row() >= m_rows.size() || index.column() >= m_columns.size())
        return false;

    SessionRow &sr = m_rows[index.row()];
    const LogbookColumn &col = m_columns[index.column()];

    bool somethingChanged = false;
    bool attributeChanged = false;

    if (role == Qt::CheckStateRole && index.column() == 0) {
        // Update visibility based on the checkbox
        bool newVisible = (value.toInt() == Qt::Checked);
        if (sr.visible != newVisible) {
            sr.visible = newVisible;
            // Force-load if setting visible on a stub
            if (newVisible && !sr.isLoaded()) {
                sessionRef(index.row());
            }
            // Sync to loaded SessionData if present
            if (sr.isLoaded()) {
                sr.session->setVisible(newVisible);
            }
            // Update lastAccessed when toggling visibility to true
            if (newVisible) {
                double now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() / 1000.0;
                LogbookManager::instance().setLastAccessed(sr.sessionId, now);
            }
            somethingChanged = true;
        }
    } else if (role == Qt::EditRole && col.type == ColumnType::SessionAttribute) {
        // Force-load before editing
        SessionData &item = sessionRef(index.row());

        // Look up the attribute definition
        const auto *def = AttributeRegistry::instance().findByKey(col.attributeKey);
        if (!def || !def->editable)
            return false;

        switch (def->formatType) {
        case AttributeFormatType::Text: {
            QString newVal = value.toString();
            QString oldVal = item.getAttribute(col.attributeKey).toString();
            if (oldVal != newVal) {
                item.setAttribute(col.attributeKey, newVal);
                somethingChanged = true;
                attributeChanged = true;
            }
            break;
        }
        case AttributeFormatType::Double: {
            double displayVal = value.toDouble();
            double newVal = displayVal;
            if (def && !def->measurementType.isEmpty())
                newVal = UnitConverter::instance().reverseConvert(displayVal, def->measurementType);
            double oldVal = item.getAttribute(col.attributeKey).toDouble();
            if (oldVal != newVal) {
                item.setAttribute(col.attributeKey, newVal);
                somethingChanged = true;
                attributeChanged = true;
            }
            break;
        }
        case AttributeFormatType::DateTime:
        case AttributeFormatType::Duration:
            // DateTime and Duration are not editable
            return false;
        }
    }

    if (somethingChanged) {
        emit dataChanged(index, index, {role});
        emit modelChanged();
        if (attributeChanged) {
            if (!sr.sessionId.isEmpty())
                scheduleSave(sr.sessionId);
        }
        return true;
    }

    return false;
}

void SessionModel::mergeSessions(const QList<SessionData>& sessions)
{
    if (sessions.isEmpty())
        return;

    beginResetModel();
    for (const SessionData& newSession : sessions) {
        if (!newSession.hasAttribute(SessionKeys::SessionId)) {
            qWarning() << "Skipping session with no SESSION_ID";
            continue;
        }

        QString newSessionID = newSession.getAttribute(SessionKeys::SessionId).toString();

        auto rowIt = std::find_if(
            m_rows.begin(), m_rows.end(),
            [&newSessionID](const SessionRow &row) {
                return row.sessionId == newSessionID;
            });

        if (rowIt != m_rows.end()) {
            // Found existing row
            if (rowIt->isLoaded()) {
                // Merge into existing loaded session
                SessionData &existingSession = rowIt->session.value();

                for (const QString &attributeKey : newSession.attributeKeys()) {
                    existingSession.setAttribute(attributeKey, newSession.getAttribute(attributeKey));
                }

                for (const QString &sensorKey : newSession.sensorKeys()) {
                    for (const QString &measurementKey : newSession.measurementKeys(sensorKey)) {
                        existingSession.setMeasurement(sensorKey, measurementKey,
                            newSession.getMeasurement(sensorKey, measurementKey));
                    }
                }

                if (!rowIt->dirty) {
                    rowIt->dirty = true;
                    m_saveHighWater++;
                    m_saveRemaining++;
                }
                qDebug() << "Merged SessionData into loaded row with SESSION_ID:" << newSessionID;
            } else {
                // Replace stub with loaded data
                rowIt->session = newSession;
                rowIt->visible = newSession.isVisible();
                rowIt->session->setVisible(rowIt->visible);
                // Remove from background load queue if present (stub no longer needs loading)
                if (m_loadQueue.removeOne(newSessionID)) {
                    m_loadRemaining--;
                }

                rowIt->dirty = true;
                m_saveHighWater++;
                m_saveRemaining++;
                qDebug() << "Replaced stub with loaded SessionData for SESSION_ID:" << newSessionID;
            }
        } else {
            // Add as new loaded row
            SessionRow newRow;
            newRow.sessionId = newSessionID;
            newRow.session = newSession;
            newRow.visible = newSession.isVisible();
            newRow.session->setVisible(newRow.visible);
            newRow.dirty = true;
            m_saveHighWater++;
            m_saveRemaining++;
            m_rows.append(std::move(newRow));
            qDebug() << "Added new SessionData with SESSION_ID:" << newSessionID;
        }
    }
    endResetModel();

    // Cache column values for all loaded sessions so the index is populated
    LogbookManager &logbook = LogbookManager::instance();
    for (const SessionRow &row : std::as_const(m_rows)) {
        if (row.isLoaded()) {
            logbook.setCachedValues(row.sessionId, computeColumnValues(row.session.value()));
        }
    }

    // Start saver worker for newly dirty rows
    bool hasDirty = std::any_of(m_rows.cbegin(), m_rows.cend(),
                                [](const SessionRow &r) { return r.dirty; });
    if (hasDirty && !m_saveTimer.isActive()) {
        m_loadTimer.stop();  // saver has priority
        m_saveTimer.start();
    }

    // Emit initial save progress so the UI can show the saver bar immediately
    if (m_saveRemaining > 0) {
        emit saveProgressChanged(m_saveRemaining, m_saveHighWater);
    }

    // Emit updated load progress (stubs may have been replaced by the merge)
    if (m_loadRemaining <= 0) {
        m_loadHighWater = 0;
        m_loadRemaining = 0;
    }
    emit loadProgressChanged(m_loadRemaining, m_loadHighWater);

    emit modelChanged();
}

void SessionModel::populateFromIndex(const QMap<QString, QMap<int, QVariant>> &cachedValues,
                                     const QMap<QString, double> &lastAccessed)
{
    Q_UNUSED(lastAccessed);

    beginResetModel();
    m_rows.clear();

    for (auto it = cachedValues.constBegin(); it != cachedValues.constEnd(); ++it) {
        SessionRow row;
        row.sessionId = it.key();
        row.cachedValues = it.value();
        row.session = std::nullopt;
        row.visible = false;
        m_rows.append(std::move(row));
    }

    endResetModel();
    emit modelChanged();
}

void SessionModel::setRowsVisibility(const QMap<int, bool>& rowVisibility)
{
    if (rowVisibility.isEmpty())
        return;

    int minRow = INT_MAX;
    int maxRow = 0;
    double now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch() / 1000.0;

    for (auto it = rowVisibility.constBegin(); it != rowVisibility.constEnd(); ++it) {
        int row = it.key();
        bool visible = it.value();
        if (row >= 0 && row < m_rows.size()) {
            m_rows[row].visible = visible;
            // Force-load if setting visible on a stub
            if (visible && !m_rows[row].isLoaded()) {
                sessionRef(row);
            }
            // Sync to loaded SessionData if present
            if (m_rows[row].isLoaded()) {
                m_rows[row].session->setVisible(visible);
            }
            // Update lastAccessed when toggling visibility to true
            if (visible) {
                LogbookManager::instance().setLastAccessed(m_rows[row].sessionId, now);
            }
            minRow = std::min(minRow, row);
            maxRow = std::max(maxRow, row);
        }
    }

    if (minRow <= maxRow) {
        emit dataChanged(index(minRow, 0), index(maxRow, columnCount() - 1), {Qt::CheckStateRole});
        emit modelChanged();
    }
}

bool SessionModel::removeSessions(const QList<QString> &sessionIds)
{
    if (sessionIds.isEmpty())
        return false;

    bool anyRemoved = false;

    for (const QString &sessionId : sessionIds) {
        auto it = std::find_if(
            m_rows.begin(),
            m_rows.end(),
            [&sessionId](const SessionRow &row) {
                return row.sessionId == sessionId;
            });

        if (it != m_rows.end()) {
            int row = it - m_rows.begin();

            // Update save counters if this session is dirty
            if (it->dirty) {
                m_saveHighWater--;
                m_saveRemaining--;
            }

            // Update load counters if this session is in the load queue
            if (m_loadQueue.removeOne(sessionId)) {
                m_loadRemaining--;
                m_loadHighWater--;
            }

            beginRemoveRows(QModelIndex(), row, row);
            m_rows.erase(it);
            endRemoveRows();

            anyRemoved = true;

            qDebug() << "Removed session with SESSION_ID:" << sessionId;
        } else {
            qWarning() << "SessionModel::removeSessions: SESSION_ID not found:" << sessionId;
        }
    }

    if (anyRemoved) {
        // Clamp counters to zero (should never go negative)
        if (m_saveHighWater <= 0) {
            m_saveHighWater = 0;
            m_saveRemaining = 0;
        }
        if (m_loadHighWater <= 0) {
            m_loadHighWater = 0;
            m_loadRemaining = 0;
        }

        emit saveProgressChanged(m_saveRemaining, m_saveHighWater);
        emit loadProgressChanged(m_loadRemaining, m_loadHighWater);
        emit modelChanged();
    }

    return anyRemoved;
}

const SessionRow& SessionModel::rowAt(int row) const
{
    Q_ASSERT(row >= 0 && row < m_rows.size());
    return m_rows.at(row);
}

SessionRow& SessionModel::rowAt(int row)
{
    Q_ASSERT(row >= 0 && row < m_rows.size());
    return m_rows[row];
}

SessionData &SessionModel::sessionRef(int row)
{
    Q_ASSERT(row >= 0 && row < m_rows.size());
    SessionRow &sr = m_rows[row];
    if (!sr.isLoaded()) {
        auto loaded = LogbookManager::instance().loadSession(sr.sessionId);
        if (loaded.has_value()) {
            sr.session = std::move(loaded.value());
        } else {
            qWarning() << "SessionModel::sessionRef: failed to load session"
                        << sr.sessionId << "- creating empty SessionData";
            sr.session = SessionData();
        }
        // Sync visibility from SessionRow to the newly loaded SessionData
        sr.session->setVisible(sr.visible);

        // Remove from background queue to avoid double-loading
        bool removed = m_loadQueue.removeOne(sr.sessionId);
        if (removed) {
            m_loadRemaining--;
            emit loadProgressChanged(m_loadRemaining, m_loadHighWater);
        }

        // Emit sessionLoaded for consistency
        emit sessionLoaded(sr.sessionId);
    }
    return sr.session.value();
}

QString SessionModel::hoveredSessionId() const
{
    return m_hoveredSessionId;
}

int SessionModel::getSessionRow(const QString& sessionId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        if (m_rows[row].sessionId == sessionId) {
            return row;
        }
    }
    return -1;
}

void SessionModel::setHoveredSessionId(const QString& sessionId)
{
    if (m_hoveredSessionId == sessionId)
        return; // No change

    QString oldSessionId = m_hoveredSessionId;
    m_hoveredSessionId = sessionId;

    qDebug() << "SessionModel: hoveredSessionId changed from" << oldSessionId << "to" << m_hoveredSessionId;

    emit hoveredSessionChanged(sessionId);

    // Notify dataChanged for old hovered session (all columns)
    if (!oldSessionId.isEmpty()) {
        int oldRow = getSessionRow(oldSessionId);
        if (oldRow != -1) {
            QModelIndex topLeft = this->index(oldRow, 0);
            QModelIndex bottomRight = this->index(oldRow, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight, {Qt::BackgroundRole, CustomRoles::IsHoveredRole});
        }
    }

    // Notify dataChanged for new hovered session (all columns)
    if (!sessionId.isEmpty()) {
        int newRow = getSessionRow(sessionId);
        if (newRow != -1) {
            QModelIndex topLeft = this->index(newRow, 0);
            QModelIndex bottomRight = this->index(newRow, columnCount() - 1);
            emit dataChanged(topLeft, bottomRight, {Qt::BackgroundRole, CustomRoles::IsHoveredRole});
        }
    }
}

bool SessionModel::updateAttribute(const QString &sessionId,
                                   const QString &attributeKey,
                                   const QVariant &newValue)
{
    // 1. Locate the row for the given session ID
    int row = getSessionRow(sessionId);
    if (row < 0) {
        qWarning() << "SessionModel::updateAttribute: No session found with ID:" << sessionId;
        return false;
    }

    // 2. Force-load before mutating
    SessionData &session = sessionRef(row);

    // 3. Retrieve the existing value
    QVariant oldValue = session.getAttribute(attributeKey);

    // 4. Check if there's actually a change
    if (oldValue == newValue) {
        return false;  // Nothing to update
    }

    // 5. Update the attribute in SessionData (captures all BFS-visited keys)
    QSet<DependencyKey> visitedKeys = session.setAttribute(attributeKey, newValue);

    // 6. Notify views that data has changed
    QModelIndex topLeft = index(row, 0);
    QModelIndex bottomRight = index(row, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight,
                     {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole});

    // 7. Emit fine-grained dependencyChanged for each key visited during BFS invalidation
    for (const DependencyKey &key : visitedKeys) {
        emit dependencyChanged(sessionId, key);
    }

    // 8. Schedule deferred logbook save
    scheduleSave(sessionId);

    return true;
}

bool SessionModel::removeAttribute(const QString &sessionId,
                                   const QString &attributeKey)
{
    // 1. Locate the row for the given session ID
    int row = getSessionRow(sessionId);
    if (row < 0) {
        qWarning() << "SessionModel::removeAttribute: No session found with ID:" << sessionId;
        return false;
    }

    // 2. Force-load before mutating
    SessionData &session = sessionRef(row);

    // 3. If the attribute is not stored, there is nothing to remove
    if (!session.hasAttribute(attributeKey)) {
        return false;
    }

    // 4. Remove the attribute and capture all BFS-visited keys
    QSet<DependencyKey> visitedKeys = session.removeAttribute(attributeKey);

    // 5. Notify views that data has changed
    QModelIndex topLeft = index(row, 0);
    QModelIndex bottomRight = index(row, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight,
                     {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole});

    // 6. Emit fine-grained dependencyChanged for each key visited during BFS invalidation
    for (const DependencyKey &key : visitedKeys) {
        emit dependencyChanged(sessionId, key);
    }

    // 7. Schedule deferred logbook save
    scheduleSave(sessionId);

    return true;
}

// ---- Deferred logbook persistence ------------------------------------

void SessionModel::scheduleSave(const QString &sessionId)
{
    int row = getSessionRow(sessionId);
    if (row < 0) return;

    SessionRow &sr = m_rows[row];
    if (!sr.dirty) {
        sr.dirty = true;
        m_saveHighWater++;
        m_saveRemaining++;
    }
    // else: row is already dirty, data is updated in memory,
    // saver will save the current (modified) version when it reaches this row

    if (!m_saveTimer.isActive()) {
        m_loadTimer.stop();  // saver has priority over loader
        m_saveTimer.start();
    }
}

void SessionModel::flushDirtySessions()
{
    m_saveTimer.stop();
    m_loadTimer.stop();

    LogbookManager &logbook = LogbookManager::instance();
    bool anySaved = false;
    for (int i = 0; i < m_rows.size(); ++i) {
        SessionRow &sr = m_rows[i];
        if (!sr.dirty)
            continue;
        logbook.saveSession(sessionRef(i));
        if (sr.isLoaded()) {
            logbook.setCachedValues(sr.sessionId, computeColumnValues(sr.session.value()));
        }
        sr.dirty = false;
        anySaved = true;
    }
    if (anySaved) {
        logbook.flushIndex();
    }

    // Reset high-water-mark counters (UI is about to be destroyed during shutdown)
    m_saveHighWater = 0;
    m_saveRemaining = 0;
    m_loadHighWater = 0;
    m_loadRemaining = 0;
}

void SessionModel::saveNextSession()
{
    // Find the first dirty row
    int dirtyIdx = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].dirty) {
            dirtyIdx = i;
            break;
        }
    }

    if (dirtyIdx < 0) {
        // No dirty rows remain
        LogbookManager::instance().flushIndex();
        m_saveHighWater = 0;
        m_saveRemaining = 0;
        emit saveProgressChanged(0, 0);
        // Resume loader if it has work
        if (!m_loadQueue.isEmpty()) {
            m_loadTimer.start();
        }
        return;
    }

    SessionRow &sr = m_rows[dirtyIdx];
    LogbookManager &logbook = LogbookManager::instance();

    logbook.saveSession(sessionRef(dirtyIdx));
    if (sr.isLoaded()) {
        logbook.setCachedValues(sr.sessionId, computeColumnValues(sr.session.value()));
    }
    sr.dirty = false;

    m_saveRemaining--;
    emit saveProgressChanged(m_saveRemaining, m_saveHighWater);

    // Check if more dirty rows remain
    bool hasDirty = std::any_of(m_rows.cbegin(), m_rows.cend(),
                                [](const SessionRow &r) { return r.dirty; });
    if (hasDirty) {
        m_saveTimer.start();
    } else {
        // All saves complete; flush index and resume loader
        logbook.flushIndex();
        m_saveHighWater = 0;
        m_saveRemaining = 0;
        emit saveProgressChanged(0, 0);
        if (!m_loadQueue.isEmpty()) {
            m_loadTimer.start();
        }
    }
}

int SessionModel::remainingStubCount() const
{
    int count = 0;
    for (const SessionRow &row : std::as_const(m_rows)) {
        if (!row.isLoaded())
            ++count;
    }
    return count;
}

// ---- Background loader ------------------------------------------------

void SessionModel::startBackgroundLoader(const QMap<QString, double> &lastAccessed)
{
    m_loadQueue.clear();

    // Build queue from all stub rows
    for (const SessionRow &row : std::as_const(m_rows)) {
        if (!row.isLoaded()) {
            m_loadQueue.append(row.sessionId);
        }
    }

    // Sort by lastAccessed descending (most recently used first)
    std::sort(m_loadQueue.begin(), m_loadQueue.end(),
              [&lastAccessed](const QString &a, const QString &b) {
        double ta = lastAccessed.value(a, 0.0);
        double tb = lastAccessed.value(b, 0.0);
        return ta > tb;
    });

    m_loadHighWater = m_loadQueue.size();
    m_loadRemaining = m_loadQueue.size();

    if (m_loadHighWater > 0) {
        emit loadProgressChanged(m_loadRemaining, m_loadHighWater);
    }

    if (!m_loadQueue.isEmpty()) {
        // Only start if saver is idle
        bool hasDirty = std::any_of(m_rows.cbegin(), m_rows.cend(),
                                    [](const SessionRow &r) { return r.dirty; });
        if (!hasDirty) {
            m_loadTimer.start();
        }
    }
}

void SessionModel::loadNextSession()
{
    // Saver has priority over loader
    if (m_saveTimer.isActive()) {
        return;
    }

    if (m_loadQueue.isEmpty()) {
        LogbookManager::instance().flushIndex();
        m_loadHighWater = 0;
        m_loadRemaining = 0;
        emit loadProgressChanged(0, 0);
        return;
    }

    QString sessionId = m_loadQueue.takeFirst();

    // Find the row index
    int rowIdx = getSessionRow(sessionId);
    if (rowIdx < 0) {
        // Session was removed; skip and continue
        m_loadRemaining--;
        emit loadProgressChanged(m_loadRemaining, m_loadHighWater);
        if (!m_loadQueue.isEmpty()) {
            m_loadTimer.start();
        } else {
            LogbookManager::instance().flushIndex();
            m_loadHighWater = 0;
            m_loadRemaining = 0;
            emit loadProgressChanged(0, 0);
        }
        return;
    }

    SessionRow &sr = m_rows[rowIdx];

    // Already loaded (e.g. via on-demand sessionRef())
    if (sr.isLoaded()) {
        m_loadRemaining--;
        emit loadProgressChanged(m_loadRemaining, m_loadHighWater);
        if (!m_loadQueue.isEmpty()) {
            m_loadTimer.start();
        } else {
            LogbookManager::instance().flushIndex();
            m_loadHighWater = 0;
            m_loadRemaining = 0;
            emit loadProgressChanged(0, 0);
        }
        return;
    }

    // Load the session
    auto loaded = LogbookManager::instance().loadSession(sessionId);
    if (loaded.has_value()) {
        sr.session = std::move(loaded.value());
        sr.session->setVisible(sr.visible);
    } else {
        qWarning() << "Background loader: failed to load session" << sessionId;
        // Leave as stub, flush or continue
        m_loadRemaining--;
        emit loadProgressChanged(m_loadRemaining, m_loadHighWater);
        if (!m_loadQueue.isEmpty()) {
            m_loadTimer.start();
        } else {
            LogbookManager::instance().flushIndex();
            m_loadHighWater = 0;
            m_loadRemaining = 0;
            emit loadProgressChanged(0, 0);
        }
        return;
    }

    // Update cached column values for this session
    LogbookManager::instance().setCachedValues(sessionId, computeColumnValues(sr.session.value()));

    // Emit dataChanged for the entire row
    QModelIndex topLeft = index(rowIdx, 0);
    QModelIndex bottomRight = index(rowIdx, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight, {Qt::DisplayRole});

    m_loadRemaining--;
    emit loadProgressChanged(m_loadRemaining, m_loadHighWater);

    // Emit sessionLoaded (NOT modelChanged)
    emit sessionLoaded(sessionId);

    // Re-arm if queue non-empty, otherwise flush index
    if (!m_loadQueue.isEmpty()) {
        m_loadTimer.start();
    } else {
        LogbookManager::instance().flushIndex();
        m_loadHighWater = 0;
        m_loadRemaining = 0;
        emit loadProgressChanged(0, 0);
    }
}

// ---- Column value extraction ------------------------------------------

QMap<LogbookColumn, QVariant> SessionModel::computeColumnValues(const SessionData &session) const
{
    QMap<LogbookColumn, QVariant> result;
    for (const LogbookColumn &col : std::as_const(m_columns)) {
        QVariant value;
        switch (col.type) {
        case ColumnType::SessionAttribute:
            value = session.getAttribute(col.attributeKey);
            break;
        case ColumnType::MeasurementAtMarker: {
            QString interpKey = SessionData::interpolationKey(
                col.markerAttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
            value = session.getAttribute(interpKey);
            break;
        }
        case ColumnType::Delta: {
            QString interpKey1 = SessionData::interpolationKey(
                col.markerAttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
            QString interpKey2 = SessionData::interpolationKey(
                col.marker2AttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
            QVariant v1 = session.getAttribute(interpKey1);
            QVariant v2 = session.getAttribute(interpKey2);
            if (v1.isValid() && v2.isValid())
                value = v2.toDouble() - v1.toDouble();
            break;
        }
        }
        if (value.isValid())
            result[col] = value;
    }
    return result;
}

// ----------------------------------------------------------------------

void SessionModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= m_columns.size())
        return;

    const LogbookColumn &col = m_columns[column];

    // Lambda to extract raw value from a SessionRow (handles both stubs and loaded rows)
    auto getRawValue = [&col, column](const SessionRow &sr) -> QVariant {
        if (sr.isLoaded()) {
            const SessionData &s = sr.session.value();
            switch (col.type) {
            case ColumnType::SessionAttribute:
                return s.getAttribute(col.attributeKey);
            case ColumnType::MeasurementAtMarker: {
                QString interpKey = SessionData::interpolationKey(
                    col.markerAttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
                return s.getAttribute(interpKey);
            }
            case ColumnType::Delta: {
                QString interpKey1 = SessionData::interpolationKey(
                    col.markerAttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
                QString interpKey2 = SessionData::interpolationKey(
                    col.marker2AttributeKey, col.sensorID, SessionKeys::Time, col.measurementID);
                QVariant v1 = s.getAttribute(interpKey1);
                QVariant v2 = s.getAttribute(interpKey2);
                if (!v1.isValid() || !v2.isValid())
                    return QVariant();
                return v2.toDouble() - v1.toDouble();
            }
            }
        } else {
            // Stub: use cached values
            return sr.cachedValues.value(column);
        }
        return QVariant();
    };

    // Determine whether to use string or numeric comparison
    bool useStringCompare = false;
    if (col.type == ColumnType::SessionAttribute) {
        const auto *def = AttributeRegistry::instance().findByKey(col.attributeKey);
        if (def && def->formatType == AttributeFormatType::Text)
            useStringCompare = true;
    }

    beginResetModel();

    std::sort(m_rows.begin(), m_rows.end(),
              [&getRawValue, useStringCompare, order](const SessionRow &a, const SessionRow &b) {
        QVariant va = getRawValue(a);
        QVariant vb = getRawValue(b);

        bool aValid = va.isValid();
        bool bValid = vb.isValid();

        // Missing values sort to bottom in both ascending and descending order
        if (!aValid && !bValid) return false;
        if (!aValid) return false; // a goes to bottom
        if (!bValid) return true;  // b goes to bottom

        int result;
        if (useStringCompare) {
            result = QString::compare(va.toString(), vb.toString(), Qt::CaseInsensitive);
        } else {
            bool okA = false, okB = false;
            double da = va.toDouble(&okA);
            double db = vb.toDouble(&okB);
            if (!okA && !okB) result = 0;
            else if (!okA) result = -1;
            else if (!okB) result = 1;
            else result = (da < db) ? -1 : (da == db ? 0 : 1);
        }

        if (order == Qt::AscendingOrder)
            return result < 0;
        else
            return result > 0;
    });

    endResetModel();
}

QString SessionModel::columnUnitLabel(const LogbookColumn &col) const
{
    switch (col.type) {
    case ColumnType::MeasurementAtMarker:
    case ColumnType::Delta:
        return UnitConverter::instance().getUnitLabel(col.measurementType);
    case ColumnType::SessionAttribute: {
        const auto *def = AttributeRegistry::instance().findByKey(col.attributeKey);
        if (def && !def->measurementType.isEmpty())
            return UnitConverter::instance().getUnitLabel(def->measurementType);
        return QString();
    }
    }
    return QString();
}

} // namespace FlySight
