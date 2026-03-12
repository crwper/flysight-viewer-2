#ifndef ADDCOLUMNDIALOG_H
#define ADDCOLUMNDIALOG_H

#include <QDialog>

#include "logbookcolumn.h"

class QComboBox;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QDialogButtonBox;

namespace FlySight {

class AddColumnDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddColumnDialog(QWidget *parent = nullptr);

    LogbookColumn result() const;

private slots:
    void onTypeChanged(int index);
    void onSelectionChanged();

private:
    QComboBox       *m_typeCombo;
    QStackedWidget  *m_stack;
    QDialogButtonBox *m_buttonBox;

    // Page 0: Session Attribute
    QTreeWidget *m_attributeTree;

    // Page 1: Measurement at Marker
    QTreeWidget *m_measurementTree1;
    QTreeWidget *m_markerTree1;

    // Page 2: Delta
    QTreeWidget *m_measurementTree2;
    QTreeWidget *m_fromMarkerTree;
    QTreeWidget *m_toMarkerTree;

    void buildAttributePage(QWidget *page);
    void buildMeasurementAtMarkerPage(QWidget *page);
    void buildDeltaPage(QWidget *page);

    void populateAttributeTree(QTreeWidget *tree);
    void populateMeasurementTree(QTreeWidget *tree);
    void populateMarkerTree(QTreeWidget *tree);

    bool isSelectionComplete() const;
};

} // namespace FlySight

#endif // ADDCOLUMNDIALOG_H
