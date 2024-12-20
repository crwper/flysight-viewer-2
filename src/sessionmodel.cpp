#include "sessionmodel.h"
#include <QMessageBox>
#include <QDateTime>
#include <QTimeZone>

namespace FlySight {

struct SessionColumn {
    QString name;                                 // Display name of the column
    std::function<QVariant(const SessionData&)> getter;   // Gets the displayed value
    std::function<bool(SessionData&, const QVariant&)> setter; // Updates the session data
    std::function<int(const SessionData&, const SessionData&)> comparator; // Compares two sessions
    bool editable;                                // Whether the column is editable
};

inline int compareStrings(const QVariant &a, const QVariant &b) {
    QString sa = a.toString();
    QString sb = b.toString();
    return QString::compare(sa, sb, Qt::CaseInsensitive);
}

inline int compareDateTimes(const QVariant &a, const QVariant &b) {
    if (!a.canConvert<QDateTime>() && !b.canConvert<QDateTime>())
        return 0; // Both invalid, consider them equal
    if (!a.canConvert<QDateTime>())
        return -1;
    if (!b.canConvert<QDateTime>())
        return 1;
    QDateTime da = a.toDateTime();
    QDateTime db = b.toDateTime();
    return da < db ? -1 : (da == db ? 0 : 1);
}

inline int compareDoubles(const QVariant &a, const QVariant &b) {
    bool okA = false, okB = false;
    double da = a.toDouble(&okA);
    double db = b.toDouble(&okB);
    if (!okA && !okB) return 0;
    if (!okA) return -1;
    if (!okB) return 1;
    return (da < db) ? -1 : (da == db ? 0 : 1);
}

auto getStringAttribute = [](const SessionData &s, const char *key) -> QVariant {
    return s.getAttribute(key);
};

auto getFormattedDateTime = [](const SessionData &s, const char *key) -> QVariant {
    QVariant var = s.getAttribute(key);
    if (!var.canConvert<QDateTime>()) {
        return QVariant();
    }
    QDateTime dt = var.toDateTime();
    return dt.toString("yyyy/MM/dd HH:mm:ss");
};

auto getFormattedDuration = [](const SessionData &s, const char *key) -> QVariant {
    bool ok = false;
    double durationSec = s.getAttribute(key).toDouble(&ok);
    if (!ok) return QVariant();
    int totalSec = static_cast<int>(durationSec);
    int minutes = totalSec / 60;
    int seconds = totalSec % 60;
    return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
};

auto setStringAttribute = [](SessionData &s, const char *key, const QVariant &value) -> bool {
    QString newVal = value.toString();
    QString oldVal = s.getAttribute(key).toString();
    if (oldVal != newVal) {
        s.setAttribute(key, newVal);
        return true;
    }
    return false;
};

static const QVector<SessionColumn>& columns()
{
    static const QVector<SessionColumn> s_columns = {
        {
            "Description",
            [](const SessionData &s) { return getStringAttribute(s, SessionKeys::Description); },
            [](SessionData &s, const QVariant &value) { return setStringAttribute(s, SessionKeys::Description, value); },
            [](const SessionData &a, const SessionData &b) {
                return compareStrings(a.getAttribute(SessionKeys::Description),
                                      b.getAttribute(SessionKeys::Description));
            },
            true
        },
        {
            "Device Name",
            [](const SessionData &s) { return getStringAttribute(s, SessionKeys::DeviceId); },
            nullptr,
            [](const SessionData &a, const SessionData &b) {
                return compareStrings(a.getAttribute(SessionKeys::DeviceId),
                                      b.getAttribute(SessionKeys::DeviceId));
            },
            true
        },
        {
            "Start Time",
            [](const SessionData &s) { return getFormattedDateTime(s, SessionKeys::StartTime); },
            nullptr,
            [](const SessionData &a, const SessionData &b) {
                return compareDateTimes(a.getAttribute(SessionKeys::StartTime),
                                        b.getAttribute(SessionKeys::StartTime));
            },
            false
        },
        {
            "Duration",
            [](const SessionData &s) { return getFormattedDuration(s, SessionKeys::Duration); },
            nullptr,
            [](const SessionData &a, const SessionData &b) {
                return compareDoubles(a.getAttribute(SessionKeys::Duration),
                                      b.getAttribute(SessionKeys::Duration));
            },
            false
        },
        {
            "Exit Time",
            [](const SessionData &s) { return getFormattedDateTime(s, SessionKeys::ExitTime); },
            nullptr,
            [](const SessionData &a, const SessionData &b) {
                return compareDateTimes(a.getAttribute(SessionKeys::ExitTime),
                                        b.getAttribute(SessionKeys::ExitTime));
            },
            false
        },
    };
    return s_columns;
}

SessionModel::SessionModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_sessionData.size();
}

int SessionModel::columnCount(const QModelIndex &parent) const {
    Q_UNUSED(parent);
    return columns().size();
}

QVariant SessionModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_sessionData.size() || index.column() >= columns().size())
        return QVariant();

    const SessionData &item = m_sessionData.at(index.row());
    const SessionColumn &col = columns()[index.column()];

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return col.getter(item);

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
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < columns().size()) {
        return columns()[section].name;
    }
    return QVariant();
}

Qt::ItemFlags SessionModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;

    const SessionColumn &col = columns()[index.column()];

    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    if (index.column() == 0) {
        // The first column has a checkbox
        flags |= Qt::ItemIsUserCheckable;
    }
    if (col.editable) {
        flags |= Qt::ItemIsEditable;
    }
    return flags;
}

bool SessionModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.row() >= m_sessionData.size() || index.column() >= columns().size())
        return false;

    SessionData &item = m_sessionData[index.row()];
    const SessionColumn &col = columns()[index.column()];

    bool somethingChanged = false;

    if (role == Qt::CheckStateRole && index.column() == 0) {
        // Update visibility based on the checkbox
        bool newVisible = (value.toInt() == Qt::Checked);
        if (item.isVisible() != newVisible) {
            item.setVisible(newVisible);
            somethingChanged = true;
        }
    } else if (role == Qt::EditRole && col.editable && col.setter) {
        // If setter returns true, something changed
        somethingChanged = col.setter(item, value);
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

void SessionModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= columns().size())
        return;

    auto getter = columns()[column].getter;
    auto cmp = columns()[column].comparator; // Our custom comparator

    std::sort(m_sessionData.begin(), m_sessionData.end(), [getter, cmp, order](const SessionData &a, const SessionData &b) {
        int result = cmp(a, b);
        if (order == Qt::AscendingOrder) {
            // ascending: a < b if result < 0
            return result < 0;
        } else {
            // descending: a < b if a > b in ascending terms, so result > 0
            return result > 0;
        }
    });

    beginResetModel();
    endResetModel();
}

} // namespace FlySight
