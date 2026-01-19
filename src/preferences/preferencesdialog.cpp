#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include "plotssettingspage.h"
#include "markerssettingspage.h"
#include "legendsettingspage.h"
#include "mapsettingspage.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>

namespace FlySight {

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Preferences"));
    resize(600, 500);  // Enlarged from 400x300 to accommodate new pages

    QHBoxLayout *mainLayout = new QHBoxLayout();
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);

    // Sidebar list with all categories
    categoryList = new QListWidget(this);
    categoryList->addItem(tr("General"));
    categoryList->addItem(tr("Import"));
    categoryList->addItem(tr("Plots"));
    categoryList->addItem(tr("Markers"));
    categoryList->addItem(tr("Legend"));
    categoryList->addItem(tr("Map"));
    categoryList->setFixedWidth(120);

    // Stacked widget for pages - ORDER MUST MATCH categoryList
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));    // Index 0: General
    stackedWidget->addWidget(new ImportSettingsPage(this));     // Index 1: Import
    stackedWidget->addWidget(new PlotsSettingsPage(this));      // Index 2: Plots
    stackedWidget->addWidget(new MarkersSettingsPage(this));    // Index 3: Markers
    stackedWidget->addWidget(new LegendSettingsPage(this));     // Index 4: Legend
    stackedWidget->addWidget(new MapSettingsPage(this));        // Index 5: Map

    mainLayout->addWidget(categoryList);
    mainLayout->addWidget(stackedWidget);

    // OK/Cancel buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);

    // Add layouts to the dialog
    dialogLayout->addLayout(mainLayout);
    dialogLayout->addWidget(buttonBox);

    // Select first item by default
    categoryList->setCurrentRow(0);

    // Connect category selection to page switching
    connect(categoryList, &QListWidget::currentRowChanged, stackedWidget, &QStackedWidget::setCurrentIndex);
}

} // namespace FlySight
