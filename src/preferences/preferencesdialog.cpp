#include "preferencesdialog.h"
#include "generalsettingspage.h"
#include "importsettingspage.h"
#include <QDialogButtonBox>
#include <QHBoxLayout>

namespace FlySight {

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Preferences");
    resize(400, 300);

    QHBoxLayout *mainLayout = new QHBoxLayout();
    QVBoxLayout *dialogLayout = new QVBoxLayout(this);

    // Sidebar list
    categoryList = new QListWidget(this);
    categoryList->addItem("General");
    categoryList->addItem("Import");
    categoryList->setFixedWidth(120);

    // Stacked widget for pages
    stackedWidget = new QStackedWidget(this);
    stackedWidget->addWidget(new GeneralSettingsPage(this));
    stackedWidget->addWidget(new ImportSettingsPage(this));

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
