#include "addcolumndialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "attributeregistry.h"
#include "markerregistry.h"
#include "plotregistry.h"

namespace FlySight {

AddColumnDialog::AddColumnDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Column"));
    resize(500, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Column type combo
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Session Attribute"));
    m_typeCombo->addItem(tr("Measurement at Marker"));
    m_typeCombo->addItem(tr("Delta between Markers"));
    mainLayout->addWidget(m_typeCombo);

    // Stacked widget with three pages
    m_stack = new QStackedWidget(this);

    QWidget *page0 = new QWidget(this);
    buildAttributePage(page0);
    m_stack->addWidget(page0);

    QWidget *page1 = new QWidget(this);
    buildMeasurementAtMarkerPage(page1);
    m_stack->addWidget(page1);

    QWidget *page2 = new QWidget(this);
    buildDeltaPage(page2);
    m_stack->addWidget(page2);

    mainLayout->addWidget(m_stack);

    // Button box
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(m_buttonBox);

    // Connect type combo to stacked widget
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddColumnDialog::onTypeChanged);

    // Initial selection check
    onSelectionChanged();
}

void AddColumnDialog::buildAttributePage(QWidget *page)
{
    QVBoxLayout *layout = new QVBoxLayout(page);

    m_attributeTree = new QTreeWidget(page);
    m_attributeTree->setHeaderHidden(true);
    m_attributeTree->setRootIsDecorated(true);

    populateAttributeTree(m_attributeTree);

    connect(m_attributeTree, &QTreeWidget::currentItemChanged,
            this, [this]() { onSelectionChanged(); });

    layout->addWidget(m_attributeTree);
}

void AddColumnDialog::buildMeasurementAtMarkerPage(QWidget *page)
{
    QHBoxLayout *layout = new QHBoxLayout(page);

    // Left: Measurement tree
    QWidget *leftWidget = new QWidget(page);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel(tr("Measurement"), leftWidget));

    m_measurementTree1 = new QTreeWidget(leftWidget);
    m_measurementTree1->setHeaderHidden(true);
    m_measurementTree1->setRootIsDecorated(true);
    populateMeasurementTree(m_measurementTree1);
    leftLayout->addWidget(m_measurementTree1);

    connect(m_measurementTree1, &QTreeWidget::currentItemChanged,
            this, [this]() { onSelectionChanged(); });

    // Right: Marker tree
    QWidget *rightWidget = new QWidget(page);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(new QLabel(tr("Marker"), rightWidget));

    m_markerTree1 = new QTreeWidget(rightWidget);
    m_markerTree1->setHeaderHidden(true);
    m_markerTree1->setRootIsDecorated(true);
    populateMarkerTree(m_markerTree1);
    rightLayout->addWidget(m_markerTree1);

    connect(m_markerTree1, &QTreeWidget::currentItemChanged,
            this, [this]() { onSelectionChanged(); });

    layout->addWidget(leftWidget);
    layout->addWidget(rightWidget);
}

void AddColumnDialog::buildDeltaPage(QWidget *page)
{
    QHBoxLayout *layout = new QHBoxLayout(page);

    // Left: Measurement tree
    QWidget *measWidget = new QWidget(page);
    QVBoxLayout *measLayout = new QVBoxLayout(measWidget);
    measLayout->setContentsMargins(0, 0, 0, 0);
    measLayout->addWidget(new QLabel(tr("Measurement"), measWidget));

    m_measurementTree2 = new QTreeWidget(measWidget);
    m_measurementTree2->setHeaderHidden(true);
    m_measurementTree2->setRootIsDecorated(true);
    populateMeasurementTree(m_measurementTree2, true);
    measLayout->addWidget(m_measurementTree2);

    connect(m_measurementTree2, &QTreeWidget::currentItemChanged,
            this, [this]() { onSelectionChanged(); });

    // Middle: From Marker tree
    QWidget *fromWidget = new QWidget(page);
    QVBoxLayout *fromLayout = new QVBoxLayout(fromWidget);
    fromLayout->setContentsMargins(0, 0, 0, 0);
    fromLayout->addWidget(new QLabel(tr("From Marker"), fromWidget));

    m_fromMarkerTree = new QTreeWidget(fromWidget);
    m_fromMarkerTree->setHeaderHidden(true);
    m_fromMarkerTree->setRootIsDecorated(true);
    populateMarkerTree(m_fromMarkerTree);
    fromLayout->addWidget(m_fromMarkerTree);

    connect(m_fromMarkerTree, &QTreeWidget::currentItemChanged,
            this, [this]() { onSelectionChanged(); });

    // Right: To Marker tree
    QWidget *toWidget = new QWidget(page);
    QVBoxLayout *toLayout = new QVBoxLayout(toWidget);
    toLayout->setContentsMargins(0, 0, 0, 0);
    toLayout->addWidget(new QLabel(tr("To Marker"), toWidget));

    m_toMarkerTree = new QTreeWidget(toWidget);
    m_toMarkerTree->setHeaderHidden(true);
    m_toMarkerTree->setRootIsDecorated(true);
    populateMarkerTree(m_toMarkerTree);
    toLayout->addWidget(m_toMarkerTree);

    connect(m_toMarkerTree, &QTreeWidget::currentItemChanged,
            this, [this]() { onSelectionChanged(); });

    layout->addWidget(measWidget);
    layout->addWidget(fromWidget);
    layout->addWidget(toWidget);
}

void AddColumnDialog::populateAttributeTree(QTreeWidget *tree)
{
    const QVector<AttributeDefinition> attrs = AttributeRegistry::instance().allAttributes();

    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const AttributeDefinition &def : attrs) {
        QTreeWidgetItem *categoryItem = categoryItems.value(def.category, nullptr);
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(tree);
            categoryItem->setText(0, def.category);
            categoryItem->setFlags(Qt::ItemIsEnabled);  // Not selectable
            categoryItem->setExpanded(true);

            QFont boldFont = categoryItem->font(0);
            boldFont.setBold(true);
            categoryItem->setFont(0, boldFont);

            categoryItems[def.category] = categoryItem;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(categoryItem);
        item->setText(0, def.displayName);
        item->setData(0, Qt::UserRole, def.attributeKey);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
}

void AddColumnDialog::populateMeasurementTree(QTreeWidget *tree, bool includeIndependent)
{
    QVector<PlotValue> plots = PlotRegistry::instance().allPlots();
    if (!includeIndependent) {
        plots.erase(std::remove_if(plots.begin(), plots.end(),
                                   [](const PlotValue &pv) { return pv.role == PlotRole::Independent; }),
                    plots.end());
    }

    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const PlotValue &pv : plots) {
        QTreeWidgetItem *categoryItem = categoryItems.value(pv.category, nullptr);
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(tree);
            categoryItem->setText(0, pv.category);
            categoryItem->setFlags(Qt::ItemIsEnabled);  // Not selectable
            categoryItem->setExpanded(true);

            QFont boldFont = categoryItem->font(0);
            boldFont.setBold(true);
            categoryItem->setFont(0, boldFont);

            categoryItems[pv.category] = categoryItem;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(categoryItem);
        QString displayName = pv.plotName;
        if (!pv.plotUnits.isEmpty()) {
            displayName += QString(" (%1)").arg(pv.plotUnits);
        }
        item->setText(0, displayName);
        item->setData(0, Qt::UserRole, pv.sensorID);
        item->setData(0, Qt::UserRole + 1, pv.measurementID);
        item->setData(0, Qt::UserRole + 2, pv.measurementType);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
}

void AddColumnDialog::populateMarkerTree(QTreeWidget *tree)
{
    const QVector<MarkerDefinition> markers = MarkerRegistry::instance()->allMarkers();

    QMap<QString, QTreeWidgetItem*> categoryItems;

    for (const MarkerDefinition &marker : markers) {
        // Skip markers with non-empty groupId
        if (!marker.groupId.isEmpty())
            continue;

        QTreeWidgetItem *categoryItem = categoryItems.value(marker.category, nullptr);
        if (!categoryItem) {
            categoryItem = new QTreeWidgetItem(tree);
            categoryItem->setText(0, marker.category);
            categoryItem->setFlags(Qt::ItemIsEnabled);  // Not selectable
            categoryItem->setExpanded(true);

            QFont boldFont = categoryItem->font(0);
            boldFont.setBold(true);
            categoryItem->setFont(0, boldFont);

            categoryItems[marker.category] = categoryItem;
        }

        QTreeWidgetItem *item = new QTreeWidgetItem(categoryItem);
        item->setText(0, marker.displayName);
        item->setData(0, Qt::UserRole, marker.attributeKey);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
}

void AddColumnDialog::onTypeChanged(int index)
{
    m_stack->setCurrentIndex(index);
    onSelectionChanged();
}

void AddColumnDialog::onSelectionChanged()
{
    m_buttonBox->button(QDialogButtonBox::Ok)->setEnabled(isSelectionComplete());
}

bool AddColumnDialog::isSelectionComplete() const
{
    int page = m_typeCombo->currentIndex();

    switch (page) {
    case 0: {
        // Session Attribute: need a leaf selected in attribute tree
        QTreeWidgetItem *item = m_attributeTree->currentItem();
        return item && item->parent() != nullptr;  // must be a child, not a category
    }
    case 1: {
        // Measurement at Marker: need a leaf in both trees
        QTreeWidgetItem *meas = m_measurementTree1->currentItem();
        QTreeWidgetItem *marker = m_markerTree1->currentItem();
        return meas && meas->parent() != nullptr
               && marker && marker->parent() != nullptr;
    }
    case 2: {
        // Delta: need a leaf in all three trees
        QTreeWidgetItem *meas = m_measurementTree2->currentItem();
        QTreeWidgetItem *from = m_fromMarkerTree->currentItem();
        QTreeWidgetItem *to = m_toMarkerTree->currentItem();
        return meas && meas->parent() != nullptr
               && from && from->parent() != nullptr
               && to && to->parent() != nullptr;
    }
    }

    return false;
}

LogbookColumn AddColumnDialog::result() const
{
    LogbookColumn col;
    int page = m_typeCombo->currentIndex();

    switch (page) {
    case 0: {
        col.type = ColumnType::SessionAttribute;
        QTreeWidgetItem *item = m_attributeTree->currentItem();
        if (item) {
            col.attributeKey = item->data(0, Qt::UserRole).toString();
        }
        break;
    }
    case 1: {
        col.type = ColumnType::MeasurementAtMarker;
        QTreeWidgetItem *meas = m_measurementTree1->currentItem();
        if (meas) {
            col.sensorID = meas->data(0, Qt::UserRole).toString();
            col.measurementID = meas->data(0, Qt::UserRole + 1).toString();
            col.measurementType = meas->data(0, Qt::UserRole + 2).toString();
        }
        QTreeWidgetItem *marker = m_markerTree1->currentItem();
        if (marker) {
            col.markerAttributeKey = marker->data(0, Qt::UserRole).toString();
        }
        break;
    }
    case 2: {
        col.type = ColumnType::Delta;
        QTreeWidgetItem *meas = m_measurementTree2->currentItem();
        if (meas) {
            col.sensorID = meas->data(0, Qt::UserRole).toString();
            col.measurementID = meas->data(0, Qt::UserRole + 1).toString();
            col.measurementType = meas->data(0, Qt::UserRole + 2).toString();
        }
        QTreeWidgetItem *from = m_fromMarkerTree->currentItem();
        if (from) {
            col.markerAttributeKey = from->data(0, Qt::UserRole).toString();
        }
        QTreeWidgetItem *to = m_toMarkerTree->currentItem();
        if (to) {
            col.marker2AttributeKey = to->data(0, Qt::UserRole).toString();
        }
        break;
    }
    }

    col.enabled = true;
    return col;
}

} // namespace FlySight
