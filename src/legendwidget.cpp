#include "legendwidget.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QFont>
#include <QBrush>

namespace FlySight {

LegendWidget::LegendWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    m_headerWidget = new QWidget(this);
    auto *headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);

    m_sessionLabel = new QLabel(m_headerWidget);
    m_sessionLabel->setAlignment(Qt::AlignCenter);
    QFont sessionFont = m_sessionLabel->font();
    sessionFont.setItalic(true);
    m_sessionLabel->setFont(sessionFont);

    m_utcLabel = new QLabel(m_headerWidget);
    m_utcLabel->setAlignment(Qt::AlignCenter);

    m_coordsLabel = new QLabel(m_headerWidget);
    m_coordsLabel->setAlignment(Qt::AlignCenter);

    headerLayout->addWidget(m_sessionLabel);
    headerLayout->addWidget(m_utcLabel);
    headerLayout->addWidget(m_coordsLabel);

    layout->addWidget(m_headerWidget);

    m_table = new QTableWidget(this);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setSortingEnabled(false);
    m_table->setWordWrap(false);

    layout->addWidget(m_table, 1);

    configureTableForMode(m_mode);
    clear();
}

void LegendWidget::configureTableForMode(Mode mode)
{
    if (!m_table)
        return;

    if (mode == PointDataMode) {
        m_table->setColumnCount(2);
        m_table->setHorizontalHeaderLabels(QStringList() << "" << tr("Value"));
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    } else {
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels(QStringList() << "" << tr("Min") << tr("Avg") << tr("Max"));
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    }
}

void LegendWidget::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    configureTableForMode(m_mode);

    if (m_table) {
        m_table->setRowCount(0);
        m_table->clearContents();
    }
}

void LegendWidget::setHeaderVisible(bool visible)
{
    if (m_headerWidget)
        m_headerWidget->setVisible(visible);
}

void LegendWidget::setHeader(const QString &sessionDesc,
                             const QString &utcText,
                             const QString &coordsText)
{
    if (!m_sessionLabel || !m_utcLabel || !m_coordsLabel || !m_headerWidget)
        return;

    m_sessionLabel->setText(sessionDesc);
    m_utcLabel->setText(utcText);
    m_coordsLabel->setText(coordsText);

    m_sessionLabel->setVisible(!sessionDesc.isEmpty());
    m_utcLabel->setVisible(!utcText.isEmpty());
    m_coordsLabel->setVisible(!coordsText.isEmpty());

    const bool anyVisible =
        m_sessionLabel->isVisible() || m_utcLabel->isVisible() || m_coordsLabel->isVisible();

    m_headerWidget->setVisible(anyVisible);
}

void LegendWidget::setRows(const QVector<Row> &rows)
{
    if (!m_table)
        return;

    m_table->setRowCount(rows.size());

    const int colCount = m_table->columnCount();

    for (int r = 0; r < rows.size(); ++r) {
        // Column 0: series name (colored)
        {
            auto *item = new QTableWidgetItem(rows[r].name);
            item->setFlags(Qt::ItemIsEnabled);
            item->setForeground(QBrush(rows[r].color));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_table->setItem(r, 0, item);
        }

        if (m_mode == PointDataMode) {
            QString v = rows[r].value.isEmpty() ? QStringLiteral("--") : rows[r].value;
            auto *item = new QTableWidgetItem(v);
            item->setFlags(Qt::ItemIsEnabled);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (colCount > 1)
                m_table->setItem(r, 1, item);
        } else {
            QString minV = rows[r].minValue.isEmpty() ? QStringLiteral("--") : rows[r].minValue;
            QString avgV = rows[r].avgValue.isEmpty() ? QStringLiteral("--") : rows[r].avgValue;
            QString maxV = rows[r].maxValue.isEmpty() ? QStringLiteral("--") : rows[r].maxValue;

            auto *minItem = new QTableWidgetItem(minV);
            minItem->setFlags(Qt::ItemIsEnabled);
            minItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            auto *avgItem = new QTableWidgetItem(avgV);
            avgItem->setFlags(Qt::ItemIsEnabled);
            avgItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            auto *maxItem = new QTableWidgetItem(maxV);
            maxItem->setFlags(Qt::ItemIsEnabled);
            maxItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

            if (colCount > 1) m_table->setItem(r, 1, minItem);
            if (colCount > 2) m_table->setItem(r, 2, avgItem);
            if (colCount > 3) m_table->setItem(r, 3, maxItem);
        }
    }
}

void LegendWidget::clear()
{
    if (m_sessionLabel) m_sessionLabel->clear();
    if (m_utcLabel) m_utcLabel->clear();
    if (m_coordsLabel) m_coordsLabel->clear();

    if (m_headerWidget) m_headerWidget->setVisible(false);

    if (m_table) {
        m_table->setRowCount(0);
        m_table->clearContents();
    }
}

} // namespace FlySight
