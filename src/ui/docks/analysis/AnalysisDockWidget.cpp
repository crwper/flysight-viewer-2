#include "AnalysisDockWidget.h"

#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace FlySight {

AnalysisDockWidget::AnalysisDockWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // Method selector combo box
    m_methodSelector = new QComboBox(this);
    layout->addWidget(m_methodSelector);

    // Stacked widget for method pages
    m_methodStack = new QStackedWidget(this);
    layout->addWidget(m_methodStack, 1);

    // Empty-state label (shown when no session is focused)
    m_emptyLabel = new QLabel(tr("No session selected"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_emptyLabel, 1);

    // Wire combo box to stacked widget and emit methodChanged
    connect(m_methodSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_methodStack, &QStackedWidget::setCurrentIndex);
    connect(m_methodSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisDockWidget::methodChanged);

    // Start in empty state
    setSessionActive(false);
}

int AnalysisDockWidget::addMethodPage(const QString& methodName, QWidget* page)
{
    m_methodSelector->addItem(methodName);
    return m_methodStack->addWidget(page);
}

int AnalysisDockWidget::currentMethodIndex() const
{
    return m_methodSelector->currentIndex();
}

void AnalysisDockWidget::setCurrentMethodIndex(int index)
{
    if (index >= 0 && index < m_methodSelector->count())
        m_methodSelector->setCurrentIndex(index);
}

QString AnalysisDockWidget::methodName(int index) const
{
    if (index >= 0 && index < m_methodSelector->count())
        return m_methodSelector->itemText(index);
    return QString();
}

int AnalysisDockWidget::methodCount() const
{
    return m_methodSelector->count();
}

QWidget* AnalysisDockWidget::methodPage(int index) const
{
    return m_methodStack->widget(index);
}

void AnalysisDockWidget::setSessionActive(bool active)
{
    m_methodSelector->setVisible(active);
    m_methodStack->setVisible(active);
    m_emptyLabel->setVisible(!active);
}

} // namespace FlySight
