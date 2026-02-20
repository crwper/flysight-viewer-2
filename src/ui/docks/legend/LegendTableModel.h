#ifndef LEGENDTABLEMODEL_H
#define LEGENDTABLEMODEL_H

#include <QAbstractTableModel>
#include <QColor>
#include <QVector>
#include <QString>

namespace FlySight {

class LegendTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Mode {
        PointDataMode,
        RangeStatsMode,
        MeasureMode
    };

    struct Row {
        QString name;
        QColor  color;

        // PointDataMode & MeasureMode
        QString value;

        // MeasureMode only
        QString deltaValue;

        // RangeStatsMode & MeasureMode
        QString minValue;
        QString avgValue;
        QString maxValue;
    };

    explicit LegendTableModel(QObject *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setRows(const QVector<Row> &rows);
    void clear();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    Mode m_mode = PointDataMode;
    QVector<Row> m_rows;
};

} // namespace FlySight

#endif // LEGENDTABLEMODEL_H
