#ifndef LOGBOOKSETTINGSPAGE_H
#define LOGBOOKSETTINGSPAGE_H

#include <QWidget>
#include <QVector>

#include "logbookcolumn.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QGroupBox;
class QSpinBox;

namespace FlySight {

class LogbookSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit LogbookSettingsPage(QWidget *parent = nullptr);

public slots:
    void saveSettings();

private slots:
    void onAddColumn();
    void onRemoveColumn();
    void onMoveUp();
    void onMoveDown();
    void onItemChanged(QListWidgetItem *item);

private:
    QListWidget *m_columnList;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QVector<LogbookColumn> m_columns;  // parallel vector synced with list widget
    QSpinBox *m_cacheSizeSpinBox;

    QGroupBox* createColumnsGroup();
    QGroupBox* createCacheGroup();
    void loadSettings();
    void populateList();
    void updateButtonStates();
};

} // namespace FlySight

#endif // LOGBOOKSETTINGSPAGE_H
