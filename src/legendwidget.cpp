#include "legendwidget.h"
#include "legendtablemodel.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QFont>

namespace FlySight {

static LegendTableModel::Mode toModelMode(LegendWidget::Mode m)
{
    return (m == LegendWidget::PointDataMode)
    ? LegendTableModel::PointDataMode
    : LegendTableModel::RangeStatsMode;
}

LegendWidget::LegendWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);

    // Header area
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

    // Table + model
    m_tableModel = new LegendTableModel(this);

    m_table = new QTableView(this);
    m_table->setModel(m_tableModel);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->setSortingEnabled(false);
    m_table->setWordWrap(false);

    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setStretchLastSection(false);

    layout->addWidget(m_table, 1);

    configureTableForMode(m_mode);
    clear();
}

void LegendWidget::configureTableForMode(Mode mode)
{
    if (!m_table || !m_tableModel)
        return;

    // Keep model and widget mode aligned
    m_tableModel->setMode(toModelMode(mode));

    // Keep the same resize behavior you had before
    auto *hh = m_table->horizontalHeader();
    hh->setSectionResizeMode(0, QHeaderView::Stretch);

    if (mode == PointDataMode) {
        hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    } else {
        hh->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        hh->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    }
}

void LegendWidget::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;
    configureTableForMode(m_mode);
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
    if (!m_tableModel)
        return;

    QVector<LegendTableModel::Row> modelRows;
    modelRows.reserve(rows.size());

    for (const auto &r : rows) {
        LegendTableModel::Row mr;
        mr.name     = r.name;
        mr.color    = r.color;
        mr.value    = r.value;
        mr.minValue = r.minValue;
        mr.avgValue = r.avgValue;
        mr.maxValue = r.maxValue;
        modelRows.push_back(std::move(mr));
    }

    m_tableModel->setRows(modelRows);
}

void LegendWidget::clear()
{
    if (m_sessionLabel) m_sessionLabel->clear();
    if (m_utcLabel)     m_utcLabel->clear();
    if (m_coordsLabel)  m_coordsLabel->clear();

    if (m_headerWidget) m_headerWidget->setVisible(false);

    if (m_tableModel)   m_tableModel->clear();
}

} // namespace FlySight
