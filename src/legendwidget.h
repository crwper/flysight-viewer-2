#ifndef LEGENDWIDGET_H
#define LEGENDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QColor>
#include <QVector>
#include <QString>

namespace FlySight {

class LegendWidget : public QWidget
{
    Q_OBJECT
public:
    enum Mode {
        PointDataMode,
        RangeStatsMode
    };

    struct Row {
        QString name;
        QColor  color;

        // PointDataMode
        QString value;

        // RangeStatsMode
        QString minValue;
        QString avgValue;
        QString maxValue;
    };

    explicit LegendWidget(QWidget *parent = nullptr);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setHeaderVisible(bool visible);
    void setHeader(const QString &sessionDesc,
                   const QString &utcText,
                   const QString &coordsText);

    void setRows(const QVector<Row> &rows);

    void clear();

private:
    void configureTableForMode(Mode mode);

    Mode m_mode = PointDataMode;

    QWidget *m_headerWidget = nullptr;
    QLabel  *m_sessionLabel = nullptr;
    QLabel  *m_utcLabel     = nullptr;
    QLabel  *m_coordsLabel  = nullptr;

    QTableWidget *m_table = nullptr;
};

} // namespace FlySight

#endif // LEGENDWIDGET_H
