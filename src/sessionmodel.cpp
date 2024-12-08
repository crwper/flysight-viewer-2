#include "sessionmodel.h"
#include <QMessageBox>

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
            return item.getVars().value("DESCRIPTION");
        if (index.column() == NumberOfSensors)
            return item.getSensors().size();
        break;
    case Qt::CheckStateRole:
        if (index.column() == Description) {
            bool visible = item.getVars().value("VISIBLE", "true").toLower() == "true";
            return visible ? Qt::Checked : Qt::Unchecked;
        }
        break;
    case Qt::EditRole:
        if (index.column() == Description)
            return item.getVars().value("DESCRIPTION");
        if (index.column() == NumberOfSensors)
            return item.getSensors().size();
        break;
    case Qt::UserRole:
        // Optionally, return additional data here
        break;
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
            if (item.getVars().value("DESCRIPTION") != newDescription) {
                item.getVars()["DESCRIPTION"] = newDescription;
                somethingChanged = true;
            }
        }
    } else if (role == Qt::CheckStateRole && index.column() == Description) {
        bool visible = item.getVars().value("VISIBLE", "true").toLower() == "true";
        bool newVisible = (value.toInt() == Qt::Checked);
        if (visible != newVisible) {
            item.getVars()["VISIBLE"] = newVisible ? "true" : "false";
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
    if (!newSession.getVars().contains("SESSION_ID")) {
        QMessageBox::critical(nullptr, tr("Import failed"), tr("No session ID found"));
        return;
    }

    QString newSessionID = newSession.getVars().value("SESSION_ID");

    // Check if SESSION_ID exists in m_sessionData
    auto sessionIt = std::find_if(
        m_sessionData.begin(), m_sessionData.end(),
        [&newSessionID](const SessionData &item) {
            return item.getVars().value("SESSION_ID") == newSessionID;
        });

    if (sessionIt != m_sessionData.end()) {
        SessionData &existingSession = *sessionIt;

        // Retrieve variables from both sessions
        QMap<QString, QString> existingVars = existingSession.getVars();
        QMap<QString, QString> newVars = newSession.getVars();

        // Check for exact match of variable sets
        bool varsMatch = false;
        if (existingVars.size() == newVars.size()) {
            varsMatch = true; // Assume match until proven otherwise
            for (auto it = newVars.constBegin(); it != newVars.constEnd(); ++it) {
                if (it.key() == "DESCRIPTION") {
                    continue;
                }

                if (!existingVars.contains(it.key()) || existingVars.value(it.key()) != it.value()) {
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
                                   return item.getVars().value("SESSION_ID") == uniqueSessionID;
                               })) {
                uniqueSessionID = QString("%1_%2").arg(newSessionID).arg(suffix++);
            }
            SessionData uniqueSession = newSession;
            uniqueSession.setVar("SESSION_ID", uniqueSessionID);

            beginInsertRows(QModelIndex(), m_sessionData.size(), m_sessionData.size());
            m_sessionData.append(uniqueSession);
            endInsertRows();
            emit modelChanged();

            qDebug() << "Added new SessionData with unique SESSION_ID:" << uniqueSessionID;
            return;
        }

        // Retrieve sensors from both sessions
        const QMap<QString, QMap<QString, QVector<double>>> &existingSensors = existingSession.getSensors();
        const QMap<QString, QMap<QString, QVector<double>>> &newSensors = newSession.getSensors();

        // Check for overlapping sensors
        bool noOverlap = true;
        for (auto sensorIt = newSensors.constBegin(); sensorIt != newSensors.constEnd(); ++sensorIt) {
            QString sensorName = sensorIt.key();
            const QMap<QString, QVector<double>> &newMeasurements = sensorIt.value();

            if (existingSensors.contains(sensorName)) {
                const QMap<QString, QVector<double>> &existingMeasurements = existingSensors.value(sensorName);

                for (auto measureIt = newMeasurements.constBegin(); measureIt != newMeasurements.constEnd(); ++measureIt) {
                    QString measurementKey = measureIt.key();
                    if (existingMeasurements.contains(measurementKey)) {
                        // Overlapping sensor/measurement key found
                        noOverlap = false;
                        qWarning() << "Overlapping sensor/measurement key:" << sensorName << "/" << measurementKey
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
                                   return item.getVars().value("SESSION_ID") == uniqueSessionID;
                               })) {
                uniqueSessionID = QString("%1_%2").arg(newSessionID).arg(suffix++);
            }
            SessionData uniqueSession = newSession;
            uniqueSession.setVar("SESSION_ID", uniqueSessionID);

            beginInsertRows(QModelIndex(), m_sessionData.size(), m_sessionData.size());
            m_sessionData.append(uniqueSession);
            endInsertRows();
            emit modelChanged();

            qDebug() << "Added new SessionData with unique SESSION_ID:" << uniqueSessionID;
            return;
        }

        // Merge sensors and measurements
        for (auto sensorIt = newSensors.constBegin(); sensorIt != newSensors.constEnd(); ++sensorIt) {
            QString sensorName = sensorIt.key();
            const QMap<QString, QVector<double>> &newMeasurements = sensorIt.value();

            if (!existingSession.getSensors().contains(sensorName)) {
                // Add the entire sensor if it doesn't exist
                existingSession.getSensors().insert(sensorName, newMeasurements);
            } else {
                // Merge new measurements into existing sensor
                QMap<QString, QVector<double>> &existingMeasurements = existingSession.getSensors()[sensorName];
                for (auto measureIt = newMeasurements.constBegin(); measureIt != newMeasurements.constEnd(); ++measureIt) {
                    QString measurementKey = measureIt.key();
                    const QVector<double> &data = measureIt.value();
                    existingMeasurements.insert(measurementKey, data);
                }
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
