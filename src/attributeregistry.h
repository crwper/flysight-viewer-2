#ifndef ATTRIBUTEREGISTRY_H
#define ATTRIBUTEREGISTRY_H

#include <QString>
#include <QVector>

namespace FlySight {

enum class AttributeFormatType {
    Text,       // Raw string display
    DateTime,   // UTC seconds -> formatted date/time
    Duration,   // Seconds -> mm:ss display
    Double      // Numeric double -> string display
};

struct AttributeDefinition {
    QString category;          // UI grouping (e.g., "Session", "Location")
    QString displayName;       // Human-readable name (e.g., "Start Time")
    QString attributeKey;      // SessionKeys constant this reads from
    AttributeFormatType formatType; // How to format and sort the value
    bool editable = false;     // Whether the user can edit this value in the logbook
};

class AttributeRegistry {
public:
    static AttributeRegistry& instance();

    /// Register one attribute definition
    void registerAttribute(const AttributeDefinition& def);

    /// Returns all registered attribute definitions
    QVector<AttributeDefinition> allAttributes() const;

    /// Find an attribute by its key. Returns nullptr if not found.
    const AttributeDefinition* findByKey(const QString &attributeKey) const;

private:
    QVector<AttributeDefinition> m_attributes;
};

} // namespace FlySight

#endif // ATTRIBUTEREGISTRY_H
