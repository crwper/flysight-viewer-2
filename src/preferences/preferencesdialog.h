#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>

namespace FlySight {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

private:
    QListWidget *categoryList;
    QStackedWidget *stackedWidget;
};

} // namespace FlySight

#endif // PREFERENCESDIALOG_H
