// imugnssekf.cpp
// Auto‑generated from CombinedImuFactorsExample.cpp, adapted to FlySight::runFusion()

#include "imugnssekf.h"

// Qt
#include <QtGlobal>

// GeographicLib
#include <GeographicLib/LocalCartesian.hpp>

// GTSAM
#include <gtsam/inference/Symbol.h>
#include <gtsam/navigation/CombinedImuFactor.h>
#include <gtsam/navigation/GPSFactor.h>
#include <gtsam/navigation/ImuBias.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>

#include <cassert>
#include <cmath>

namespace FlySight {
using namespace gtsam;
using symbol_shorthand::B;   // Bias  (ax,ay,az,gx,gy,gz)
using symbol_shorthand::V;   // Velocity (xdot,ydot,zdot)
using symbol_shorthand::X;   // Pose    (x,y,z,R)

//------------------------------------------------------------------------------
// Helper constants / conversions
//------------------------------------------------------------------------------
static constexpr double kDeg2Rad = M_PI / 180.0;
static constexpr double kG2ms2   = 9.80665;          // 1 g in m/s^2
static constexpr double kRad2Deg = 180.0 / M_PI;

//------------------------------------------------------------------------------
// Convenience to convert lat/lon/h to local NED using GeographicLib.
//------------------------------------------------------------------------------
static inline Vector3 toNED(const GeographicLib::LocalCartesian& lc,
                            double latDeg, double lonDeg, double hMsl) {
    double x, y, z;      // ENU from GeographicLib (x East, y North, z Up)
    lc.Forward(latDeg, lonDeg, hMsl, x, y, z);
    return Vector3(y,       // N
                   x,       // E
                   -z);      // D (Down = -Up)
}

//------------------------------------------------------------------------------
// Build IMU pre‑integration parameter block from user‑supplied sensor specs.
//------------------------------------------------------------------------------
static std::shared_ptr<PreintegratedCombinedMeasurements::Params>
makeImuParams(double accelNoise_g, double gyroNoise_deg) {
    const double accelSigma = accelNoise_g * kG2ms2;      // m/s^2 / sqrt(Hz)
    const double gyroSigma  = gyroNoise_deg * kDeg2Rad;    // rad / sqrt(Hz)

    const double accelBiasRW = 0.03;   // crude defaults (m/s^2 /√s)
    const double gyroBiasRW  = 1e-5;   // rad/s /√s

    auto p = PreintegratedCombinedMeasurements::Params::MakeSharedD(0.0);

    p->accelerometerCovariance  = I_3x3 * std::pow(accelSigma, 2);
    p->gyroscopeCovariance      = I_3x3 * std::pow(gyroSigma, 2);
    p->integrationCovariance    = I_3x3 * 1e-8;

    p->biasAccCovariance        = I_3x3 * std::pow(accelBiasRW, 2);
    p->biasOmegaCovariance      = I_3x3 * std::pow(gyroBiasRW,  2);
    p->biasAccOmegaInt          = I_6x6 * 1e-5;

    return p;
}

//------------------------------------------------------------------------------
FusionOutput runFusion(const QVector<double>& gnssTime,
                       const QVector<double>& lat,
                       const QVector<double>& lon,
                       const QVector<double>& hMSL,
                       const QVector<double>& velN,
                       const QVector<double>& velE,
                       const QVector<double>& velD,
                       const QVector<double>& /*hAcc*/,
                       const QVector<double>& /*vAcc*/,
                       const QVector<double>& /*sAcc*/,
                       const QVector<double>& imuTime,
                       const QVector<double>& imuAx,
                       const QVector<double>& imuAy,
                       const QVector<double>& imuAz,
                       const QVector<double>& imuWx,
                       const QVector<double>& imuWy,
                       const QVector<double>& imuWz,
                       double aAcc,
                       double wAcc) {
    //------------------------------------------------------------------
    // Basic sanity checks
    //------------------------------------------------------------------
    const int nGnss = gnssTime.size();
    const int nImu  = imuTime.size();

    Q_ASSERT(nGnss > 1 && nImu > 1);
    Q_ASSERT(lat.size()  == nGnss && lon.size()  == nGnss && hMSL.size() == nGnss);
    Q_ASSERT(velN.size() == nGnss && velE.size() == nGnss && velD.size() == nGnss);
    Q_ASSERT(imuAx.size() == nImu && imuAy.size() == nImu && imuAz.size() == nImu);
    Q_ASSERT(imuWx.size() == nImu && imuWy.size() == nImu && imuWz.size() == nImu);

    //------------------------------------------------------------------
    // Build local‑Cartesian converter centred at the first GNSS fix
    //------------------------------------------------------------------
    GeographicLib::LocalCartesian lc(lat[0], lon[0], hMSL[0]);

    // Convert all GNSS fixes to local NED coordinates in advance.
    std::vector<Vector3> nedPos(nGnss);
    for (int i = 0; i < nGnss; ++i)
        nedPos[i] = toNED(lc, lat[i], lon[i], hMSL[i]);

    //------------------------------------------------------------------
    // Build factor graph and initial values
    //------------------------------------------------------------------
    NonlinearFactorGraph graph;
    Values               values;

    // Initial pose (identity orientation) and velocity from first GNSS sample
    Pose3   initPose(Rot3(), nedPos[0]);
    Vector3 initVel(velN[0], velE[0], velD[0]);
    imuBias::ConstantBias initBias;     // zero

    int index = 0;  // symbol index counter

    // Insert priors
    graph.addPrior<Pose3>              (X(index), initPose,
                          noiseModel::Diagonal::Sigmas((Vector(6) << 0.1, 0.1, 0.1, 1, 1, 1).finished()));
    graph.addPrior<Vector3>            (V(index), initVel,
                            noiseModel::Isotropic::Sigma(3, 0.1));
    graph.addPrior<imuBias::ConstantBias>(B(index), initBias,
                                          noiseModel::Isotropic::Sigma(6, 1e-3));

    values.insert(X(index), initPose);
    values.insert(V(index), initVel);
    values.insert(B(index), initBias);

    //------------------------------------------------------------------
    // Prepare IMU pre‑integrator
    //------------------------------------------------------------------
    auto      imuP   = makeImuParams(aAcc, wAcc);
    auto      pim    = std::make_shared<PreintegratedCombinedMeasurements>(imuP, initBias);

    NavState  prevState(initPose, initVel);
    imuBias::ConstantBias prevBias = initBias;

    //------------------------------------------------------------------
    // Output container
    //------------------------------------------------------------------
    FusionOutput out;
    auto pushResult = [&out](double stamp, const NavState& ns, const Vector3& accEst){
        out.time .append(stamp);
        out.posN .append(ns.position().x());
        out.posE .append(ns.position().y());
        out.posD .append(ns.position().z());
        out.velN .append(ns.v().x());
        out.velE .append(ns.v().y());
        out.velD .append(ns.v().z());
        out.accN .append(accEst.x());
        out.accE .append(accEst.y());
        out.accD .append(accEst.z());

        Vector3 rpy = ns.pose().rotation().rpy();   // radians
        out.roll .append(rpy.x() * kRad2Deg);
        out.pitch.append(rpy.y() * kRad2Deg);
        out.yaw  .append(rpy.z() * kRad2Deg);
    };

    // Store result for t0 (acceleration unknown → 0)
    pushResult(gnssTime[0], prevState, Vector3::Zero());

    //------------------------------------------------------------------
    // Walk through data: integrate IMU between GNSS epochs
    //------------------------------------------------------------------
    size_t imuIdx   = 1;                 // start at second IMU sample
    Vector3 lastVel = initVel;           // for acc estimate

    for (int g = 1; g < nGnss; ++g) {
        const double gnssT = gnssTime[g];

        // 1) Accumulate IMU until we pass this GNSS timestamp
        for (; imuIdx < static_cast<size_t>(nImu) && imuTime[imuIdx] <= gnssT; ++imuIdx) {
            double dt = imuTime[imuIdx] - imuTime[imuIdx - 1];
            Vector3 accel(imuAx[imuIdx - 1] * kG2ms2,
                          imuAy[imuIdx - 1] * kG2ms2,
                          imuAz[imuIdx - 1] * kG2ms2);
            Vector3 gyro (imuWx[imuIdx - 1] * kDeg2Rad,
                         imuWy[imuIdx - 1] * kDeg2Rad,
                         imuWz[imuIdx - 1] * kDeg2Rad);
            pim->integrateMeasurement(accel, gyro, dt);
        }

        // 2) Add IMU + GPS factors
        int nextIdx = ++index;
        const auto& pimConst = dynamic_cast<const PreintegratedCombinedMeasurements&>(*pim);
        graph.emplace_shared<CombinedImuFactor>(X(nextIdx - 1), V(nextIdx - 1),
                                                X(nextIdx),     V(nextIdx),
                                                B(nextIdx - 1), B(nextIdx), pimConst);

        graph.emplace_shared<GPSFactor>(X(nextIdx), Point3(nedPos[g]),
                                        noiseModel::Isotropic::Sigma(3, 1.0));

        // 3) Initial guess for the new state via IMU prediction
        NavState propState = pim->predict(prevState, prevBias);
        values.insert(X(nextIdx), propState.pose());
        values.insert(V(nextIdx), propState.v());
        values.insert(B(nextIdx), prevBias);

        // 4) Optimize
        LevenbergMarquardtParams lmParams;
        LevenbergMarquardtOptimizer opt(graph, values, lmParams);
        Values result = opt.optimize();

        // 5) Update for next cycle
        prevState = NavState(result.at<Pose3>(X(nextIdx)), result.at<Vector3>(V(nextIdx)));
        prevBias  = result.at<imuBias::ConstantBias>(B(nextIdx));

        pim->resetIntegrationAndSetBias(prevBias);

        // 6) Approximate acceleration (simple finite difference on velocity)
        double dtGNSS = gnssT - gnssTime[g - 1];
        Vector3 accEst = (prevState.v() - lastVel) / dtGNSS;
        lastVel = prevState.v();

        // 7) Push to output
        pushResult(gnssT, prevState, accEst);
    }

    return out;
}

} // namespace FlySight
