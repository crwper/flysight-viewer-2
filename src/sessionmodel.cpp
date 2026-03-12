#include "sessionmodel.h"

#include <QDateTime>
#include <QMessageBox>
#include <QTimeZone>

#include "attributeregistry.h"
#include "logbookcolumn.h"
#include "units/unitconverter.h"

namespace FlySight {

SessionModel::SessionModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(&LogbookColumnStore::instance(), &LogbookColumnStore::columnsChanged,
            this, &SessionModel::rebuildColumns);

    connect(&UnitConverter::instance(), &UnitConverter::systemChanged, this, [this]() {
        if (m_sessionData.isEmpty() || m_columns.isEmpty()) return;
        emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::DisplayRole});
    });

    rebuildColumns();
}

void SessionModel::rebuildColumns()
{
    beginResetModel();
    m_columns = LogbookColumnStore::instance().enabledColumns();
    endResetModel();
}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_sessionData.size();
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
    const auto attrs = AttributeRegistry::instance().allAttributes();
    AttributeFormatType formatType = AttributeFormatType::Text; // default
    for (const auto &def : attrs) {
        if (def.attributeKey == col.attributeKey) {
            formatType = def.formatType;
            break;
        }
    }

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

    return UnitConverter::instance().format(value.toDouble(), col.measurementType);
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
    return UnitConverter::instance().format(delta, col.measurementType);
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_sessionData.size() || index.column() >= m_columns.size())
        return QVariant();

    const SessionData &item = m_sessionData.at(index.row());
    const LogbookColumn &col = m_columns[index.column()];

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        switch (col.type) {
        case ColumnType::SessionAttribute:
            return formatAttributeValue(item, col);
        case ColumnType::MeasurementAtMarker:
            return formatMeasurementValue(item, col);
        case ColumnType::Delta:
            return formatDeltaValue(item, col);
        }
        break;

    case Qt::CheckStateRole:
        // Handle checkbox only for the first column
        if (index.column() == 0) {
            bool visible = item.isVisible();
            return visible ? Qt::Checked : Qt::Unchecked;
        }
        break;

    case CustomRoles::IsHoveredRole:
    {
        QString currentSessionId = item.getAttribute(SessionKeys::SessionId).toString();
        return (currentSessionId == m_hoveredSessionId);
    }
    default:
        break;
    }

    return QVariant();
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < m_columns.size()) {
        return logbookColumnLabel(m_columns[section]);
    }
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
        // Check if this attribute is editable
        const auto attrs = AttributeRegistry::instance().allAttributes();
        for (const auto &def : attrs) {
            if (def.attributeKey == col.attributeKey) {
                if (def.editable)
                    flags |= Qt::ItemIsEditable;
                break;
            }
        }
    }
    // MeasurementAtMarker and Delta are never editable

    return flags;
}

bool SessionModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.row() >= m_sessionData.size() || index.column() >= m_columns.size())
        return false;

    SessionData &item = m_sessionData[index.row()];
    const LogbookColumn &col = m_columns[index.column()];

    bool somethingChanged = false;

    if (role == Qt::CheckStateRole && index.column() == 0) {
        // Update visibility based on the checkbox
        bool newVisible = (value.toInt() == Qt::Checked);
        if (item.isVisible() != newVisible) {
            item.setVisible(newVisible);
            somethingChanged = true;
        }
    } else if (role == Qt::EditRole && col.type == ColumnType::SessionAttribute) {
        // Look up the attribute definition
        const auto attrs = AttributeRegistry::instance().allAttributes();
        AttributeFormatType formatType = AttributeFormatType::Text;
        bool editable = false;
        for (const auto &def : attrs) {
            if (def.attributeKey == col.attributeKey) {
                formatType = def.formatType;
                editable = def.editable;
                break;
            }
        }

        if (!editable)
            return false;

        switch (formatType) {
        case AttributeFormatType::Text: {
            QString newVal = value.toString();
            QString oldVal = item.getAttribute(col.attributeKey).toString();
            if (oldVal != newVal) {
                item.setAttribute(col.attributeKey, newVal);
                somethingChanged = true;
            }
            break;
        }
        case AttributeFormatType::Double: {
            double newVal = value.toDouble();
            double oldVal = item.getAttribute(col.attributeKey).toDouble();
            if (oldVal != newVal) {
                item.setAttribute(col.attributeKey, newVal);
                somethingChanged = true;
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
        return true;
    }

    return false;
}

void SessionModel::mergeSessionData(const SessionData& newSession)
{
    // Ensure SESSION_ID exists
    if (!newSession.hasAttribute(SessionKeys::SessionId)) {
        QMessageBox::critical(nullptr, tr("Import failed"), tr("No session ID found"));
        return;
    }

    QString newSessionID = newSession.getAttribute(SessionKeys::SessionId).toString();

    // Check if SESSION_ID exists in m_sessionData
    auto sessionIt = std::find_if(
        m_sessionData.begin(), m_sessionData.end(),
        [&newSessionID](const SessionData &item) {
            return item.getAttribute(SessionKeys::SessionId) == newSessionID;
        });

    if (sessionIt != m_sessionData.end()) {
        SessionData &existingSession = *sessionIt;

        // Retrieve attribute and sensor names from new session
        QStringList newAttributeKeys = newSession.attributeKeys();
        QStringList newSensorKeys = newSession.sensorKeys();

        // Merge attributes
        for (const QString &attributeKey : newAttributeKeys) {
            const QVariant &value = newSession.getAttribute(attributeKey);
            existingSession.setAttribute(attributeKey, value);
        }

        // Merge sensors and measurements
        for (const QString &sensorKey : newSensorKeys) {
            QStringList newMeasurements = newSession.measurementKeys(sensorKey);

            // Simply set the measurements, regardless of whether the sensor exists.
            for (const QString &measurementKey : newMeasurements) {
                QVector<double> data = newSession.getMeasurement(sensorKey, measurementKey);
                existingSession.setMeasurement(sensorKey, measurementKey, data);
            }
        }

        // Update views
        const int row = sessionIt - m_sessionData.begin();
        emit dataChanged(index(row, 0), index(row, columnCount()));
        emit modelChanged();

        // Log successful merge
        qDebug() << "Merged SessionData with SESSION_ID:" << newSessionID;
    } else {
        // SESSION_ID does not exist, add as new SessionData
        beginInsertRows(QModelIndex(), m_sessionData.size(), m_sessionData.size());
        m_sessionData.append(newSession);
        endInsertRows();
        emit modelChanged();

        qDebug() << "Added new SessionData with SESSION_ID:" << newSessionID;
    }
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

        auto sessionIt = std::find_if(
            m_sessionData.begin(), m_sessionData.end(),
            [&newSessionID](const SessionData &item) {
                return item.getAttribute(SessionKeys::SessionId) == newSessionID;
            });

        if (sessionIt != m_sessionData.end()) {
            // Merge into existing session
            SessionData &existingSession = *sessionIt;

            for (const QString &attributeKey : newSession.attributeKeys()) {
                existingSession.setAttribute(attributeKey, newSession.getAttribute(attributeKey));
            }

            for (const QString &sensorKey : newSession.sensorKeys()) {
                for (const QString &measurementKey : newSession.measurementKeys(sensorKey)) {
                    existingSession.setMeasurement(sensorKey, measurementKey,
                        newSession.getMeasurement(sensorKey, measurementKey));
                }
            }

            qDebug() << "Merged SessionData with SESSION_ID:" << newSessionID;
        } else {
            // Add as new session
            m_sessionData.append(newSession);
            qDebug() << "Added new SessionData with SESSION_ID:" << newSessionID;
        }
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

    for (auto it = rowVisibility.constBegin(); it != rowVisibility.constEnd(); ++it) {
        int row = it.key();
        bool visible = it.value();
        if (row >= 0 && row < m_sessionData.size()) {
            m_sessionData[row].setVisible(visible);
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

    // Iterate over the list of SESSION_IDs to remove
    for (const QString &sessionId : sessionIds) {
        // Find the session in m_sessionData
        auto it = std::find_if(
            m_sessionData.begin(),
            m_sessionData.end(),
            [&sessionId](const SessionData &item) {
                return item.getAttribute(SessionKeys::SessionId).toString() == sessionId;
            });

        if (it != m_sessionData.end()) {
            int row = it - m_sessionData.begin();

            beginRemoveRows(QModelIndex(), row, row);
            m_sessionData.erase(it);
            endRemoveRows();

            anyRemoved = true;

            qDebug() << "Removed session with SESSION_ID:" << sessionId;
        } else {
            qWarning() << "SessionModel::removeSessions: SESSION_ID not found:" << sessionId;
        }
    }

    if (anyRemoved) {
        emit modelChanged();
    }

    return anyRemoved;
}

const QVector<SessionData>& SessionModel::getAllSessions() const
{
    return m_sessionData;
}

SessionData &SessionModel::sessionRef(int row)
{
    Q_ASSERT(row >= 0 && row < m_sessionData.size());
    return m_sessionData[row];
}

QString SessionModel::hoveredSessionId() const
{
    return m_hoveredSessionId;
}

int SessionModel::getSessionRow(const QString& sessionId) const
{
    for(int row = 0; row < m_sessionData.size(); ++row){
        if(m_sessionData[row].getAttribute(SessionKeys::SessionId) == sessionId){
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

    // 2. Retrieve the existing value
    SessionData &session = m_sessionData[row];
    QVariant oldValue = session.getAttribute(attributeKey);

    // 3. Check if there's actually a change
    if (oldValue == newValue) {
        return false;  // Nothing to update
    }

    // 4. Update the attribute in SessionData (captures all BFS-visited keys)
    QSet<DependencyKey> visitedKeys = session.setAttribute(attributeKey, newValue);

    // 5. Notify views that data has changed
    //    (Emit dataChanged for all columns in this row to ensure full refresh.)
    QModelIndex topLeft = index(row, 0);
    QModelIndex bottomRight = index(row, columnCount() - 1);
    emit dataChanged(topLeft, bottomRight,
                     {Qt::DisplayRole, Qt::EditRole, Qt::CheckStateRole});

    // 6. Emit fine-grained dependencyChanged for each key visited during BFS invalidation
    for (const DependencyKey &key : visitedKeys) {
        emit dependencyChanged(sessionId, key);
    }

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

    // 2. Get a reference to the SessionData
    SessionData &session = m_sessionData[row];

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

    return true;
}

void SessionModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= m_columns.size())
        return;

    const LogbookColumn &col = m_columns[column];

    // Determine the comparison function based on column type
    auto getRawValue = [&col](const SessionData &s) -> QVariant {
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
        return QVariant();
    };

    // Determine whether to use string or numeric comparison
    bool useStringCompare = false;
    if (col.type == ColumnType::SessionAttribute) {
        const auto attrs = AttributeRegistry::instance().allAttributes();
        for (const auto &def : attrs) {
            if (def.attributeKey == col.attributeKey) {
                if (def.formatType == AttributeFormatType::Text)
                    useStringCompare = true;
                break;
            }
        }
    }

    beginResetModel();

    std::sort(m_sessionData.begin(), m_sessionData.end(),
              [&getRawValue, useStringCompare, order](const SessionData &a, const SessionData &b) {
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

} // namespace FlySight
