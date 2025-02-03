#ifndef GRAPHINFO_H
#define GRAPHINFO_H

#include <QString>
#include <QPen>

/*!
 * \brief A small struct to describe sessionId, sensorId, etc.
 */
namespace FlySight {
struct GraphInfo
{
    QString sessionId;
    QString sensorId;
    QString measurementId;
    QPen    defaultPen;
};
} // namespace FlySight

#endif // GRAPHINFO_H
