#include "sessionmodel.h"
#include <QMessageBox>

namespace FlySight {

SessionModel::SessionModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_sessionData.size();
}

int SessionModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sessionData.size() || index.column() >= ColumnCount)
        return QVariant();

    const SessionData &item = m_sessionData.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == Description)
            return item.getVar(SessionKeys::Description);
        if (index.column() == NumberOfSensors)
            return item.sensorKeys().size();
        break;
    case Qt::CheckStateRole:
        if (index.column() == Description) {
            bool visible = item.isVisible();
            return visible ? Qt::Checked : Qt::Unchecked;
        }
        break;
    case Qt::EditRole:
        if (index.column() == Description)
            return item.getVar(SessionKeys::Description);
        if (index.column() == NumberOfSensors)
            return item.sensorKeys().size();
        break;
    // Handle BackgroundRole for highlighting
    case Qt::BackgroundRole: {
        QString currentSessionId = item.getVar(SessionKeys::SessionId);
        if (currentSessionId == m_hoveredSessionId) {
            return QBrush(Qt::yellow); // Highlight color
        }
        break;
    }
    // Handle IsHoveredRole
    case CustomRoles::IsHoveredRole: {
        QString currentSessionId = item.getVar(SessionKeys::SessionId);
        return (currentSessionId == m_hoveredSessionId) ? true : false;
    }
    default:
        break;
    }
    return QVariant();
}

QVariant SessionModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case Description:
            return tr("Description");
        case NumberOfSensors:
            return tr("Number of Sensors");
        default:
            return QVariant();
        }
    }
    return QVariant();
}

Qt::ItemFlags SessionModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == Description)
        flags |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    return flags;
}

bool SessionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= m_sessionData.size() || index.column() >= ColumnCount)
        return false;

    SessionData &item = m_sessionData[index.row()];
    bool somethingChanged = false;

    if (role == Qt::EditRole) {
        if (index.column() == Description) {
            QString newDescription = value.toString();
            if (item.getVar(SessionKeys::Description) != newDescription) {
                item.setVar(SessionKeys::Description, newDescription);
                somethingChanged = true;
            }
        }
    } else if (role == Qt::CheckStateRole && index.column() == Description) {
        bool visible = item.isVisible();
        bool newVisible = (value.toInt() == Qt::Checked);
        if (visible != newVisible) {
            item.setVisible(newVisible);
            somethingChanged = true;
        }
    }

    if (somethingChanged) {
        emit dataChanged(index, index, {role});
        emit modelChanged(); // Custom signal for external views
        return true;
    }
    return false;
}

void SessionModel::mergeSessionData(const SessionData& newSession)
{
    // Ensure SESSION_ID exists
    if (!newSession.hasVar(SessionKeys::SessionId)) {
        QMessageBox::critical(nullptr, tr("Import failed"), tr("No session ID found"));
        return;
    }

    QString newSessionID = newSession.getVar(SessionKeys::SessionId);

    // Check if SESSION_ID exists in m_sessionData
    auto sessionIt = std::find_if(
        m_sessionData.begin(), m_sessionData.end(),
        [&newSessionID](const SessionData &item) {
            return item.getVar(SessionKeys::SessionId) == newSessionID;
        });

    if (sessionIt != m_sessionData.end()) {
        SessionData &existingSession = *sessionIt;

        QStringList existingKeys = existingSession.varKeys();
        QStringList newKeys = newSession.varKeys();

        bool varsMatch = false;
        if (existingKeys.size() == newKeys.size()) {
            varsMatch = true; // Assume match until proven otherwise

            // Iterate over the new session's keys (excluding SessionKeys::Description)
            for (const QString &key : newKeys) {
                if (key == SessionKeys::Description) {
                    continue;
                }

                // Check both existence and equality of values
                bool existingHasKey = existingSession.hasVar(key);
                bool newHasKey = newSession.hasVar(key);

                // If either session doesn't have the key, or the values don't match, break
                if (!existingHasKey || !newHasKey ||
                    existingSession.getVar(key) != newSession.getVar(key)) {
                    varsMatch = false;
                    break;
                }
            }
        }

        if (!varsMatch) {
            qWarning() << "Variable sets do not match for SESSION_ID:" << newSessionID;
            // Assign a unique SESSION_ID by appending a suffix
            int suffix = 1;
            QString uniqueSessionID = newSessionID;
            while (std::any_of(m_sessionData.begin(), m_sessionData.end(),
                               [&uniqueSessionID](const SessionData &item) {
                                   return item.getVar(SessionKeys::SessionId) == uniqueSessionID;
                               })) {
                uniqueSessionID = QString("%1_%2").arg(newSessionID).arg(suffix++);
            }
            SessionData uniqueSession = newSession;
            uniqueSession.setVar(SessionKeys::SessionId, uniqueSessionID);

            beginInsertRows(QModelIndex(), m_sessionData.size(), m_sessionData.size());
            m_sessionData.append(uniqueSession);
            endInsertRows();
            emit modelChanged();

            qDebug() << "Added new SessionData with unique SESSION_ID:" << uniqueSessionID;
            return;
        }

        // Retrieve sensor names from both sessions
        QStringList newSensorKeys = newSession.sensorKeys();

        // Check for overlapping sensors
        bool noOverlap = true;
        for (const QString &sensorKey : newSensorKeys) {
            // Get the new session's measurements for this sensor
            QStringList newMeasurementKeys = newSession.measurementKeys(sensorKey);

            // Check if the existing session has this sensor
            if (existingSession.hasSensor(sensorKey)) {
                // Check each measurement to see if it already exists
                for (const QString &measurementKey : newMeasurementKeys) {
                    if (existingSession.hasMeasurement(sensorKey, measurementKey)) {
                        // Overlapping sensor/measurement key found
                        noOverlap = false;
                        qWarning() << "Overlapping sensor/measurement key:" << sensorKey << "/" << measurementKey
                                   << "for SESSION_ID:" << newSessionID;
                        break;
                    }
                }
            }

            if (!noOverlap) break;
        }

        if (!noOverlap) {
            qWarning() << "Cannot merge SessionData due to overlapping sensor/measurement keys for SESSION_ID:" << newSessionID;
            // Assign a unique SESSION_ID by appending a suffix
            int suffix = 1;
            QString uniqueSessionID = newSessionID;
            while (std::any_of(m_sessionData.begin(), m_sessionData.end(),
                               [&uniqueSessionID](const SessionData &item) {
                                   return item.getVar(SessionKeys::SessionId) == uniqueSessionID;
                               })) {
                uniqueSessionID = QString("%1_%2").arg(newSessionID).arg(suffix++);
            }
            SessionData uniqueSession = newSession;
            uniqueSession.setVar(SessionKeys::SessionId, uniqueSessionID);

            beginInsertRows(QModelIndex(), m_sessionData.size(), m_sessionData.size());
            m_sessionData.append(uniqueSession);
            endInsertRows();
            emit modelChanged();

            qDebug() << "Added new SessionData with unique SESSION_ID:" << uniqueSessionID;
            return;
        }

        // Merge sensors and measurements
        for (const QString &sensorName : newSensorKeys) {
            QStringList newMeasurements = newSession.measurementKeys(sensorName);

            // Simply set the measurements, regardless of whether the sensor exists.
            for (const QString &measurementKey : newMeasurements) {
                QVector<double> data = newSession.getMeasurement(sensorName, measurementKey);
                existingSession.setMeasurement(sensorName, measurementKey, data);
            }
        }

        // Update views
        const int row = sessionIt - m_sessionData.begin();
        emit dataChanged(index(row, 0), index(row, ColumnCount));
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

const QVector<SessionData>& SessionModel::getAllSessions() const
{
    return m_sessionData;
}


QString SessionModel::hoveredSessionId() const
{
    return m_hoveredSessionId;
}

int SessionModel::getSessionRow(const QString& sessionId) const
{
    for(int row = 0; row < m_sessionData.size(); ++row){
        if(m_sessionData[row].getVar(SessionKeys::SessionId) == sessionId){
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
            QModelIndex bottomRight = this->index(oldRow, ColumnCount - 1);
            emit dataChanged(topLeft, bottomRight, {Qt::BackgroundRole, CustomRoles::IsHoveredRole});
        }
    }

    // Notify dataChanged for new hovered session (all columns)
    if (!sessionId.isEmpty()) {
        int newRow = getSessionRow(sessionId);
        if (newRow != -1) {
            QModelIndex topLeft = this->index(newRow, 0);
            QModelIndex bottomRight = this->index(newRow, ColumnCount - 1);
            emit dataChanged(topLeft, bottomRight, {Qt::BackgroundRole, CustomRoles::IsHoveredRole});
        }
    }
}

} // namespace FlySight
