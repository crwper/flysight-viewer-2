#include "attributeregistration.h"
#include "../attributeregistry.h"
#include "../sessiondata.h"
#include "../units/unitdefinitions.h"

using namespace FlySight;

void FlySight::registerBuiltInAttributes() {
    auto& reg = AttributeRegistry::instance();

    reg.registerAttribute({
        QStringLiteral("Session"),
        QStringLiteral("Description"),
        SessionKeys::Description,
        AttributeFormatType::Text,
        true
    });

    reg.registerAttribute({
        QStringLiteral("Session"),
        QStringLiteral("Device Name"),
        SessionKeys::DeviceId,
        AttributeFormatType::Text,
        true
    });

    reg.registerAttribute({
        QStringLiteral("Session"),
        QStringLiteral("Start Time"),
        SessionKeys::StartTime,
        AttributeFormatType::DateTime,
        false
    });

    reg.registerAttribute({
        QStringLiteral("Session"),
        QStringLiteral("Duration"),
        SessionKeys::Duration,
        AttributeFormatType::Duration,
        false
    });

    reg.registerAttribute({
        QStringLiteral("Session"),
        QStringLiteral("Exit Time"),
        SessionKeys::ExitTime,
        AttributeFormatType::DateTime,
        false
    });

    reg.registerAttribute({
        QStringLiteral("Location"),
        QStringLiteral("Ground Elevation"),
        SessionKeys::GroundElev,
        AttributeFormatType::Double,
        true,
        MeasurementTypes::Altitude
    });
}
