#ifndef LEGENDWIDGET_H
#define LEGENDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTableView>
#include <QColor>
#include <QVector>
#include <QString>

#include "preferences/preferencesmanager.h"
#include "preferences/preferencekeys.h"

namespace FlySight {

class LegendTableModel;

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

    void setHeader(const QString &sessionDesc,
                   const QString &utcText,
                   const QString &coordsText);

    void setRows(const QVector<Row> &rows);
    void clear();

private slots:
    void onPreferenceChanged(const QString &key, const QVariant &value);

private:
    void configureTableForMode(Mode mode);
    void applyLegendPreferences();

    void clearHeader();
    void updateHeaderVisibility() const;
    bool headerAllowed() const;

    Mode m_mode = PointDataMode;

    QWidget *m_headerWidget = nullptr;
    QLabel  *m_sessionLabel = nullptr;
    QLabel  *m_utcLabel     = nullptr;
    QLabel  *m_coordsLabel  = nullptr;

    QTableView       *m_table = nullptr;
    LegendTableModel *m_tableModel = nullptr;

    int m_textSize = 9;
};

} // namespace FlySight

#endif // LEGENDWIDGET_H
