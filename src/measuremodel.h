#ifndef MEASUREMODEL_H
#define MEASUREMODEL_H

#include <QObject>
#include <QColor>
#include <QString>
#include <QVector>

namespace FlySight {

class MeasureModel : public QObject
{
    Q_OBJECT
public:
    struct Row {
        QString name;
        QColor  color;
        QString deltaValue;
        QString finalValue;
        QString minValue;
        QString avgValue;
        QString maxValue;
    };

    explicit MeasureModel(QObject *parent = nullptr)
        : QObject(parent) {}

    bool isActive() const { return m_active; }
    bool isMultiTrack() const { return m_multiTrack; }

    QString sessionDesc() const { return m_sessionDesc; }
    QString utcText()     const { return m_utcText; }
    QString coordsText()  const { return m_coordsText; }
    const QVector<Row>& rows() const { return m_rows; }

    void setData(const QString &sessionDesc,
                 const QString &utcText,
                 const QString &coordsText,
                 const QVector<Row> &rows,
                 bool multiTrack = false)
    {
        m_active      = true;
        m_multiTrack  = multiTrack;
        m_sessionDesc = sessionDesc;
        m_utcText     = utcText;
        m_coordsText  = coordsText;
        m_rows        = rows;
        emit dataChanged();
    }

    void clear()
    {
        if (!m_active)
            return;
        m_active = false;
        m_multiTrack = false;
        m_sessionDesc.clear();
        m_utcText.clear();
        m_coordsText.clear();
        m_rows.clear();
        emit dataChanged();
    }

signals:
    void dataChanged();

private:
    bool m_active = false;
    bool m_multiTrack = false;
    QString m_sessionDesc;
    QString m_utcText;
    QString m_coordsText;
    QVector<Row> m_rows;
};

} // namespace FlySight

#endif // MEASUREMODEL_H
