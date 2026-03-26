#ifndef STEPCOMMITSPINBOX_H
#define STEPCOMMITSPINBOX_H

#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>

namespace FlySight {

/**
 * SpinBox that commits on arrow-button clicks (not just Enter / focus-out).
 *
 * Also provides:
 *  - Enter commits + clears focus
 *  - Escape reverts to value-on-focus-in + clears focus
 *  - lastStepCount() for callers that need to know the step direction
 */
class StepCommitSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;

    int  lastStepCount() const { return m_lastSteps; }
    void clearStep()           { m_lastSteps = 0; }
    void deselectText()        { lineEdit()->deselect(); }

    void stepBy(int steps) override
    {
        m_lastSteps = steps;
        QDoubleSpinBox::stepBy(steps);
        emit editingFinished();
        clearFocus();
    }

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            interpretText();
            emit editingFinished();
            clearFocus();
            return;
        }
        if (event->key() == Qt::Key_Escape) {
            setValue(m_valueOnFocusIn);
            clearFocus();
            return;
        }
        QDoubleSpinBox::keyPressEvent(event);
    }

    void focusInEvent(QFocusEvent* event) override
    {
        m_valueOnFocusIn = value();
        QDoubleSpinBox::focusInEvent(event);
    }

private:
    int    m_lastSteps = 0;
    double m_valueOnFocusIn = 0.0;
};

} // namespace FlySight

#endif // STEPCOMMITSPINBOX_H
