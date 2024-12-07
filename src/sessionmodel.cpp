#include "sessionmodel.h"

SessionModel::SessionModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}

int SessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return items.size();
}

int SessionModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items.size() || index.column() >= ColumnCount)
        return QVariant();

    const Item &item = items.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        if (index.column() == Description)
            return item.description;
        if (index.column() == NumberOfCorners)
            return item.numCorners;
        break;
    case Qt::CheckStateRole:
        if (index.column() == Description)
            return item.visible ? Qt::Checked : Qt::Unchecked;
        break;
    case Qt::EditRole:
        if (index.column() == Description)
            return item.description;
        if (index.column() == NumberOfCorners)
            return item.numCorners;
        break;    case Qt::UserRole:
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
        case NumberOfCorners:
            return tr("Number of Corners");
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
    if (index.column() == NumberOfCorners)
        flags |= Qt::ItemIsEditable; // Allow editing for the new column
    return flags;
}

bool SessionModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() >= items.size() || index.column() >= ColumnCount)
        return false;

    Item &item = items[index.row()];
    bool somethingChanged = false;

    if (role == Qt::EditRole) {
        if (index.column() == Description) {
            QString newDescription = value.toString();
            if (item.description != newDescription) {
                item.description = newDescription;
                somethingChanged = true;
            }
        }
        if (index.column() == NumberOfCorners) {
            int newCorners = value.toInt();
            if (item.numCorners != newCorners) {
                item.numCorners = newCorners;
                somethingChanged = true;
            }
        }
    } else if (role == Qt::CheckStateRole && index.column() == Description) {
        bool newVisible = (value.toInt() == Qt::Checked);
        if (item.visible != newVisible) {
            item.visible = newVisible;
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

void SessionModel::addItem(const Item &item)
{
    beginInsertRows(QModelIndex(), items.size(), items.size());
    items.append(item);
    endInsertRows();
    emit modelChanged();
}
