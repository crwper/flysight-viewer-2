#ifndef ANALYSISMETHODWIDGET_H
#define ANALYSISMETHODWIDGET_H

#include <QWidget>
#include <QStringList>

namespace FlySight {

class SessionModel;

/**
 * Abstract base class for analysis method page widgets.
 *
 * Each method (e.g. Wingsuit Performance) subclasses this to provide
 * its own UI and parameter handling within the Analysis Dock.
 */
class AnalysisMethodWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AnalysisMethodWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    virtual ~AnalysisMethodWidget() = default;

    /// Called when the focused session changes. Empty string means no session.
    virtual void setFocusedSession(SessionModel* model, const QString& sessionId) = 0;

    /// Return the list of stored parameter attribute keys for this method.
    virtual QStringList parameterKeys() const = 0;
};

} // namespace FlySight

#endif // ANALYSISMETHODWIDGET_H
