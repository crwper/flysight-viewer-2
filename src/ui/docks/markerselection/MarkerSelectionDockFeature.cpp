#include "MarkerSelectionDockFeature.h"
#include "ui/docks/AppContext.h"
#include "markermodel.h"
#include "plotviewsettingsmodel.h"
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace FlySight {

MarkerSelectionDockFeature::MarkerSelectionDockFeature(const AppContext& ctx, QObject* parent)
    : DockFeature(parent)
    , m_viewSettings(ctx.plotViewSettings)
    , m_markerModel(ctx.markerModel)
{
    // Create dock widget with unique name for layout persistence
    m_dock = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("Marker Selection"));

    // Build container with reference dropdown above the tree view
    auto *container = new QWidget(m_dock);
    auto *mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Reference dropdown row
    auto *refLayout = new QHBoxLayout;
    refLayout->addWidget(new QLabel(QStringLiteral("Reference:"), container));
    m_referenceCombo = new QComboBox(container);
    m_referenceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    refLayout->addWidget(m_referenceCombo);
    mainLayout->addLayout(refLayout);

    // Tree view
    m_treeView = new QTreeView(container);
    m_treeView->setModel(ctx.markerModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_treeView);

    m_dock->setWidget(container);

    // Connect combo box user interaction
    connect(m_referenceCombo, QOverload<int>::of(&QComboBox::activated),
            this, &MarkerSelectionDockFeature::onReferenceComboActivated);

    // Repopulate when enabled markers change
    connect(m_markerModel, &QAbstractItemModel::dataChanged,
            this, &MarkerSelectionDockFeature::populateReferenceCombo);
    connect(m_markerModel, &QAbstractItemModel::modelReset,
            this, &MarkerSelectionDockFeature::populateReferenceCombo);

    // Sync when reference is changed externally (e.g. double-click on bubble)
    connect(m_viewSettings, &PlotViewSettingsModel::referenceMarkerKeyChanged,
            this, &MarkerSelectionDockFeature::onReferenceMarkerKeyChanged);

    // Initial population
    populateReferenceCombo();
}

void MarkerSelectionDockFeature::populateReferenceCombo()
{
    const QSignalBlocker blocker(m_referenceCombo);

    m_referenceCombo->clear();
    m_referenceCombo->addItem(QStringLiteral("None"), QVariant(QString()));

    const QVector<MarkerDefinition> enabled = m_markerModel->enabledMarkers();
    for (const MarkerDefinition &def : enabled) {
        m_referenceCombo->addItem(def.displayName, QVariant(def.attributeKey));
    }

    // Select the current reference key, or fall back to "None"
    const QString currentKey = m_viewSettings->referenceMarkerKey();
    int matchIndex = -1;
    for (int i = 1; i < m_referenceCombo->count(); ++i) {
        if (m_referenceCombo->itemData(i).toString() == currentKey) {
            matchIndex = i;
            break;
        }
    }

    if (matchIndex >= 0) {
        m_referenceCombo->setCurrentIndex(matchIndex);
    } else {
        // Current reference marker is not enabled -- fall back to "None"
        m_referenceCombo->setCurrentIndex(0);
        if (!currentKey.isEmpty()) {
            m_viewSettings->setReferenceMarkerKey(QString());
        }
    }
}

void MarkerSelectionDockFeature::onReferenceComboActivated(int index)
{
    const QString key = m_referenceCombo->itemData(index).toString();
    m_viewSettings->setReferenceMarkerKey(key);
}

void MarkerSelectionDockFeature::onReferenceMarkerKeyChanged(const QString &oldKey, const QString &newKey)
{
    Q_UNUSED(oldKey)

    const QSignalBlocker blocker(m_referenceCombo);

    for (int i = 0; i < m_referenceCombo->count(); ++i) {
        if (m_referenceCombo->itemData(i).toString() == newKey) {
            m_referenceCombo->setCurrentIndex(i);
            return;
        }
    }
    // Key not found in combo -- default to "None"
    m_referenceCombo->setCurrentIndex(0);
}

QString MarkerSelectionDockFeature::id() const
{
    return QStringLiteral("Marker Selection");
}

QString MarkerSelectionDockFeature::title() const
{
    return QStringLiteral("Marker Selection");
}

KDDockWidgets::QtWidgets::DockWidget* MarkerSelectionDockFeature::dock() const
{
    return m_dock;
}

KDDockWidgets::Location MarkerSelectionDockFeature::defaultLocation() const
{
    return KDDockWidgets::Location_OnLeft;
}

} // namespace FlySight
