#include "attributeregistry.h"

using namespace FlySight;

AttributeRegistry& AttributeRegistry::instance() {
    static AttributeRegistry R;
    return R;
}

void AttributeRegistry::registerAttribute(const AttributeDefinition& def) {
    m_attributes.append(def);
}

QVector<AttributeDefinition> AttributeRegistry::allAttributes() const {
    return m_attributes;
}
