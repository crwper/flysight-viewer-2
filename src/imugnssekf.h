#ifndef IMUGNSSEKF_H
#define IMUGNSSEKF_H

#include <QVector>

namespace FlySight {

struct FusionOutput {
    QVector<double> time;
    QVector<double> accN, accE, accD;
    QVector<double> velN, velE, velD;
    QVector<double> posN, posE, posD;
    QVector<double> roll, pitch, yaw;
};

FusionOutput runFusion(
    const QVector<double>& gnssTime,
    const QVector<double>& lat,
    const QVector<double>& lon,
    const QVector<double>& hMSL,
    const QVector<double>& velN,
    const QVector<double>& velE,
    const QVector<double>& velD,
    const QVector<double>& hAcc,
    const QVector<double>& vAcc,
    const QVector<double>& sAcc,

    const QVector<double>& imuTime,
    const QVector<double>& imuAx,
    const QVector<double>& imuAy,
    const QVector<double>& imuAz,
    const QVector<double>& imuWx,
    const QVector<double>& imuWy,
    const QVector<double>& imuWz
    );

} // namespace FlySight

#endif // IMUGNSSEKF_H
