#ifndef SESSIONMODEL_H
#define SESSIONMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "sessiondata.h"

class SessionModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Columns {
        Description = 0,
        NumberOfSensors,
        ColumnCount
    };

    SessionModel(QObject *parent = nullptr);

    // Data management
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void mergeSessionData(const SessionData& newSession);

    // Add more methods as needed (e.g., removeItem, clear, etc.)

signals:
    void modelChanged();

private:
    QVector<SessionData> m_sessionData;
};

#endif // SESSIONMODEL_H
