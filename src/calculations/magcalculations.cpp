#include "magcalculations.h"
#include "../sessiondata.h"
#include "../dependencykey.h"
#include <QVector>
#include <cmath>

using namespace FlySight;

void Calculations::registerMagCalculations()
{
    // MAG total magnetic field strength
    SessionData::registerCalculatedMeasurement(
        "MAG", "total",
        {
            DependencyKey::measurement("MAG", "x"),
            DependencyKey::measurement("MAG", "y"),
            DependencyKey::measurement("MAG", "z")
        },
        [](SessionData& session) -> std::optional<QVector<double>> {
        QVector<double> x = session.getMeasurement("MAG", "x");
        QVector<double> y = session.getMeasurement("MAG", "y");
        QVector<double> z = session.getMeasurement("MAG", "z");

        if (x.isEmpty() || y.isEmpty() || z.isEmpty()) {
            qWarning() << "Cannot calculate total due to missing x, y, or z";
            return std::nullopt;
        }

        if ((x.size() != y.size()) || (x.size() != z.size())) {
            qWarning() << "x, y, or z size mismatch in session:" << session.getAttribute("_SESSION_ID");
            return std::nullopt;
        }

        QVector<double> total;
        total.reserve(x.size());
        for(int i = 0; i < x.size(); ++i){
            total.append(std::sqrt(x[i]*x[i] + y[i]*y[i] + z[i]*z[i]));
        }
        return total;
    });
}
