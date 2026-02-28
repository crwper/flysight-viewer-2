#include "markermodel.h"

#include <QVariant>

#include "sessiondata.h" // for SessionKeys::ExitTime default enabled marker

namespace FlySight {

MarkerModel::MarkerModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

void MarkerModel::setSettings(QSettings *settings)
{
    m_settings = settings;
}

QString MarkerModel::makeMarkerId(const QString& attributeKey)
{
    return attributeKey;
}

QString MarkerModel::makeMarkerId(const MarkerDefinition& def)
{
    return makeMarkerId(def.attributeKey);
}

QString MarkerModel::settingsKey(const QString& attributeKey)
{
    return QStringLiteral("state/markers/") + attributeKey;
}

MarkerModel::Node* MarkerModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<Node*>(index.internalPointer());
}

MarkerModel::CategoryNode* MarkerModel::categoryFromIndex(const QModelIndex& index) const
{
    Node* n = nodeFromIndex(index);
    if (!n || n->type != Node::Type::Category)
        return nullptr;
    return static_cast<CategoryNode*>(n);
}

MarkerModel::MarkerNode* MarkerModel::markerFromIndex(const QModelIndex& index) const
{
    Node* n = nodeFromIndex(index);
    if (!n || n->type != Node::Type::Marker)
        return nullptr;
    return static_cast<MarkerNode*>(n);
}

QModelIndex MarkerModel::indexForMarker(const MarkerNode* node) const
{
    if (!node || node->markerRow < 0 || node->categoryRow < 0)
        return {};
    return createIndex(node->markerRow, 0, const_cast<MarkerNode*>(node));
}

void MarkerModel::setMarkers(const QVector<MarkerDefinition>& defs)
{
    // Preserve enabled state by attributeKey across resets
    QHash<QString, bool> enabledByKey;
    enabledByKey.reserve(m_markersByKey.size());
    for (auto it = m_markersByKey.constBegin(); it != m_markersByKey.constEnd(); ++it) {
        if (it.value()) {
            enabledByKey.insert(it.key(), it.value()->enabled);
        }
    }

    beginResetModel();

    m_categories.clear();
    m_markersByKey.clear();

    QHash<QString, CategoryNode*> categoryByName;
    categoryByName.reserve(16);

    for (const MarkerDefinition& def : defs) {
        CategoryNode* category = nullptr;

        auto catIt = categoryByName.find(def.category);
        if (catIt == categoryByName.end()) {
            auto catNode = std::make_unique<CategoryNode>();
            catNode->name = def.category;
            catNode->row = static_cast<int>(m_categories.size());
            category = catNode.get();

            m_categories.push_back(std::move(catNode));
            categoryByName.insert(def.category, category);
        } else {
            category = catIt.value();
        }

        const QString key = makeMarkerId(def);

        auto markerNode = std::make_unique<MarkerNode>();
        markerNode->def = def;

        // Restore enabled state: in-memory hash (within-session reset),
        // then QSettings (cross-session), then default.
        const bool defaultEnabled = (key == SessionKeys::ExitTime
                                     || key == SessionKeys::AnalysisStartTime
                                     || key == SessionKeys::AnalysisEndTime);
        markerNode->enabled = enabledByKey.contains(key)
            ? enabledByKey.value(key)
            : (m_settings ? m_settings->value(settingsKey(key), defaultEnabled).toBool()
                          : defaultEnabled);

        markerNode->category = category;
        markerNode->categoryRow = category->row;
        markerNode->markerRow = static_cast<int>(category->markers.size());

        MarkerNode* markerPtr = markerNode.get();
        category->markers.push_back(std::move(markerNode));

        m_markersByKey.insert(key, markerPtr);
    }

    endResetModel();
}

QVector<MarkerDefinition> MarkerModel::enabledMarkers() const
{
    QVector<MarkerDefinition> out;
    for (const auto& cat : m_categories) {
        for (const auto& marker : cat->markers) {
            if (marker->enabled) {
                out.push_back(marker->def);
            }
        }
    }
    return out;
}

void MarkerModel::setMarkerEnabled(const QString& attributeKey, bool enabled)
{
    const QString key = makeMarkerId(attributeKey);
    MarkerNode* node = m_markersByKey.value(key, nullptr);
    if (!node) {
        return;
    }

    if (node->enabled == enabled) {
        return;
    }

    node->enabled = enabled;

    if (m_settings)
        m_settings->setValue(settingsKey(key), enabled);

    const QModelIndex idx = indexForMarker(node);
    if (idx.isValid()) {
        emit dataChanged(idx, idx, {Qt::CheckStateRole});
    }
}

bool MarkerModel::isMarkerEnabled(const QString& attributeKey) const
{
    const QString key = makeMarkerId(attributeKey);
    MarkerNode* node = m_markersByKey.value(key, nullptr);
    return node ? node->enabled : false;
}

bool MarkerModel::toggleMarker(const QString& attributeKey)
{
    const QString key = makeMarkerId(attributeKey);
    MarkerNode* node = m_markersByKey.value(key, nullptr);
    if (!node) {
        return false;
    }

    const bool newEnabled = !node->enabled;
    setMarkerEnabled(attributeKey, newEnabled);
    return newEnabled;
}

QModelIndex MarkerModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0) {
        return {};
    }

    if (!parent.isValid()) {
        if (row >= static_cast<int>(m_categories.size())) {
            return {};
        }
        return createIndex(row, column, m_categories[static_cast<std::size_t>(row)].get());
    }

    // Only a two-level hierarchy: parent must be a category
    CategoryNode* category = categoryFromIndex(parent);
    if (!category) {
        return {};
    }

    if (row >= static_cast<int>(category->markers.size())) {
        return {};
    }

    return createIndex(row, column, category->markers[static_cast<std::size_t>(row)].get());
}

QModelIndex MarkerModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) {
        return {};
    }

    Node* n = nodeFromIndex(child);
    if (!n) {
        return {};
    }

    if (n->type == Node::Type::Category) {
        return {};
    }

    MarkerNode* marker = static_cast<MarkerNode*>(n);
    if (!marker->category) {
        return {};
    }

    return createIndex(marker->categoryRow, 0, marker->category);
}

int MarkerModel::rowCount(const QModelIndex& parent) const
{
    if (!parent.isValid()) {
        return static_cast<int>(m_categories.size());
    }

    Node* n = nodeFromIndex(parent);
    if (!n) {
        return 0;
    }

    if (n->type == Node::Type::Category) {
        CategoryNode* category = static_cast<CategoryNode*>(n);
        return static_cast<int>(category->markers.size());
    }

    return 0;
}

int MarkerModel::columnCount(const QModelIndex& /*parent*/) const
{
    return 1;
}

QVariant MarkerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    Node* n = nodeFromIndex(index);
    if (!n) {
        return {};
    }

    if (n->type == Node::Type::Category) {
        CategoryNode* category = static_cast<CategoryNode*>(n);
        if (role == Qt::DisplayRole) {
            return category->name;
        }
        return {};
    }

    MarkerNode* marker = static_cast<MarkerNode*>(n);

    switch (role) {
    case Qt::DisplayRole:
        return marker->def.displayName;
    case Qt::CheckStateRole:
        return marker->enabled ? Qt::Checked : Qt::Unchecked;
    case AttributeKeyRole:
        return marker->def.attributeKey;
    case CategoryRole:
        return marker->def.category;
    case DefaultColorRole:
        return marker->def.color;
    default:
        return {};
    }
}

bool MarkerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid()) {
        return false;
    }

    MarkerNode* marker = markerFromIndex(index);
    if (!marker) {
        return false;
    }

    if (role != Qt::CheckStateRole) {
        return false;
    }

    bool enabled = false;
    if (value.canConvert<int>()) {
        enabled = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked);
    } else if (value.canConvert<bool>()) {
        enabled = value.toBool();
    } else {
        return false;
    }

    if (marker->enabled == enabled) {
        return true;
    }

    marker->enabled = enabled;

    if (m_settings)
        m_settings->setValue(settingsKey(makeMarkerId(marker->def)), enabled);

    emit dataChanged(index, index, {Qt::CheckStateRole});
    return true;
}

Qt::ItemFlags MarkerModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    Node* n = nodeFromIndex(index);
    if (!n) {
        return Qt::NoItemFlags;
    }

    if (n->type == Node::Type::Category) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

QVariant MarkerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && section == 0 && role == Qt::DisplayRole) {
        return tr("Markers");
    }
    return {};
}

QHash<int, QByteArray> MarkerModel::roleNames() const
{
    QHash<int, QByteArray> roles = QAbstractItemModel::roleNames();
    roles[Qt::DisplayRole] = "display";
    roles[Qt::CheckStateRole] = "checkState";
    roles[DefaultColorRole] = "defaultColor";
    roles[AttributeKeyRole] = "attributeKey";
    roles[CategoryRole] = "category";
    return roles;
}

} // namespace FlySight
