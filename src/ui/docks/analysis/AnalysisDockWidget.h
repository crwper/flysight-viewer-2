#ifndef ANALYSISDOCKWIDGET_H
#define ANALYSISDOCKWIDGET_H

#include <QWidget>

class QComboBox;
class QLabel;
class QStackedWidget;

namespace FlySight {

/**
 * Inner widget for the Analysis Dock.
 *
 * Contains a method selector combo box and a stacked widget for method pages.
 * Shows an empty-state label when no session is focused.
 */
class AnalysisDockWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AnalysisDockWidget(QWidget* parent = nullptr);

    /// Register a method page. Returns the page index.
    int addMethodPage(const QString& methodName, QWidget* page);

    /// Returns the currently selected method index.
    int currentMethodIndex() const;

    /// Sets the current method by index. Does nothing if index is out of range.
    void setCurrentMethodIndex(int index);

    /// Returns the display text of the method at the given index, or empty string.
    QString methodName(int index) const;

    /// Returns the number of registered method pages.
    int methodCount() const;

    /// Returns the method page widget at the given index, or nullptr.
    QWidget* methodPage(int index) const;

    /// Show/hide the empty state (no focused session).
    void setSessionActive(bool active);

signals:
    /// Emitted when the user changes the method selector.
    void methodChanged(int index);

private:
    QComboBox*      m_methodSelector = nullptr;
    QStackedWidget* m_methodStack    = nullptr;
    QLabel*         m_emptyLabel     = nullptr;
};

} // namespace FlySight

#endif // ANALYSISDOCKWIDGET_H
