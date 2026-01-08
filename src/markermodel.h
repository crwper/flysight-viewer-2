#ifndef MARKERMODEL_H
#define MARKERMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QVector>
#include <memory>
#include <vector>
#include <cstddef>

#include "markerregistry.h"

namespace FlySight {

class MarkerModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum MarkerRoles {
        DefaultColorRole = Qt::UserRole + 1,
        AttributeKeyRole,
        CategoryRole
    };

    explicit MarkerModel(QObject *parent = nullptr);

    // Public API (minimum)
    void setMarkers(const QVector<MarkerDefinition>& defs);
    QVector<MarkerDefinition> enabledMarkers() const;

    void setMarkerEnabled(const QString& attributeKey, bool enabled);
    bool isMarkerEnabled(const QString& attributeKey) const;
    bool toggleMarker(const QString& attributeKey); // convenience

    // QAbstractItemModel
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Node {
        enum class Type { Category, Marker };
        explicit Node(Type t) : type(t) {}
        Type type;
    };

    struct CategoryNode;

    struct MarkerNode : Node {
        MarkerNode() : Node(Type::Marker) {}
        MarkerDefinition def;
        bool enabled = false;

        CategoryNode* category = nullptr;
        int categoryRow = -1;
        int markerRow = -1;
    };

    struct CategoryNode : Node {
        CategoryNode() : Node(Type::Category) {}
        QString name;
        std::vector<std::unique_ptr<MarkerNode>> markers;
        int row = -1;
    };

    static QString makeMarkerId(const QString& attributeKey);
    static QString makeMarkerId(const MarkerDefinition& def);

    Node* nodeFromIndex(const QModelIndex& index) const;
    CategoryNode* categoryFromIndex(const QModelIndex& index) const;
    MarkerNode* markerFromIndex(const QModelIndex& index) const;

    QModelIndex indexForMarker(const MarkerNode* node) const;

    std::vector<std::unique_ptr<CategoryNode>> m_categories;
    QHash<QString, MarkerNode*> m_markersByKey;
};

} // namespace FlySight

#endif // MARKERMODEL_H
