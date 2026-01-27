#include "LegendTableModel.h"
#include <QBrush>

namespace FlySight {

LegendTableModel::LegendTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void LegendTableModel::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    beginResetModel();
    m_mode = mode;
    m_rows.clear();   // matches old behavior: switching mode clears contents
    endResetModel();
}

void LegendTableModel::setRows(const QVector<Row> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void LegendTableModel::clear()
{
    if (m_rows.isEmpty())
        return;

    beginResetModel();
    m_rows.clear();
    endResetModel();
}

int LegendTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int LegendTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return (m_mode == PointDataMode) ? 2 : 4;
}

QVariant LegendTableModel::headerData(int section,
                                      Qt::Orientation orientation,
                                      int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    if (section == 0)
        return QString(); // blank header for series names

    if (m_mode == PointDataMode) {
        if (section == 1) return tr("Value");
        return QVariant();
    }

    switch (section) {
    case 1: return tr("Min");
    case 2: return tr("Avg");
    case 3: return tr("Max");
    default: return QVariant();
    }
}

QVariant LegendTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const int r = index.row();
    const int c = index.column();
    if (r < 0 || r >= m_rows.size())
        return QVariant();

    const Row &row = m_rows.at(r);

    if (role == Qt::DisplayRole) {
        if (c == 0)
            return row.name;

        if (m_mode == PointDataMode) {
            if (c == 1)
                return row.value.isEmpty() ? QStringLiteral("--") : row.value;
            return QVariant();
        }

        switch (c) {
        case 1: return row.minValue.isEmpty() ? QStringLiteral("--") : row.minValue;
        case 2: return row.avgValue.isEmpty() ? QStringLiteral("--") : row.avgValue;
        case 3: return row.maxValue.isEmpty() ? QStringLiteral("--") : row.maxValue;
        default: return QVariant();
        }
    }

    if (role == Qt::ForegroundRole) {
        if (c == 0)
            return QBrush(row.color); // colored series name
        return QVariant();
    }

    if (role == Qt::TextAlignmentRole) {
        if (c == 0)
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }

    return QVariant();
}

Qt::ItemFlags LegendTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    // Old behavior: items were enabled but not editable. View selection is disabled anyway.
    return Qt::ItemIsEnabled;
}

} // namespace FlySight
