#include "imugnssekf.h"

#include <QtGlobal>
#include <GeographicLib/LocalCartesian.hpp>

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
using symbol_shorthand::B;
using symbol_shorthand::V;
using symbol_shorthand::X;

// ---- constants -----------------------------------------------------------
static constexpr double kDeg2Rad = M_PI / 180.0;
static constexpr double kG2ms2   = 9.80665;
static constexpr double kRad2Deg = 180.0 / M_PI;
static constexpr double kMinSigma = 1e-3;   // 1 mm floor to avoid singularities

// ---- helpers -------------------------------------------------------------
static inline Vector3 toNED(const GeographicLib::LocalCartesian& lc,
                            double latDeg, double lonDeg, double hMsl) {
    double x, y, z; // ENU
    lc.Forward(latDeg, lonDeg, hMsl, x, y, z);
    return Vector3(y, x, -z); // N,E,D
}

static noiseModel::Diagonal::shared_ptr makePosNoise(double hSigma, double vSigma) {
    double sigN = std::max(hSigma, kMinSigma);
    double sigE = sigN;
    double sigD = std::max(vSigma, kMinSigma);
    return noiseModel::Diagonal::Sigmas((Vector3() << sigN, sigE, sigD).finished());
}

static std::shared_ptr<PreintegratedCombinedMeasurements::Params>
makeImuParams(double accelNoise_g, double gyroNoise_deg) {
    double accelSigma = accelNoise_g * kG2ms2;
    double gyroSigma  = gyroNoise_deg * kDeg2Rad;
    auto p = PreintegratedCombinedMeasurements::Params::MakeSharedD(0.0);
    p->accelerometerCovariance = I_3x3 * std::pow(accelSigma,2);
    p->gyroscopeCovariance     = I_3x3 * std::pow(gyroSigma,2);
    p->integrationCovariance   = I_3x3 * 1e-8;
    p->biasAccCovariance   = I_3x3 * std::pow(0.03,2);
    p->biasOmegaCovariance = I_3x3 * std::pow(1e-5,2);
    p->biasAccOmegaInt     = I_6x6 * 1e-5;
    return p;
}

// -------------------------------------------------------------------------
FusionOutput runFusion(const QVector<double>& gnssTime,
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
                       const QVector<double>& imuWz,
                       double aAcc,
                       double wAcc) {
    const int nGnss = gnssTime.size();
    const int nImu  = imuTime.size();
    Q_ASSERT(nGnss>1 && nImu>1);
    Q_ASSERT(hAcc.size()==nGnss && vAcc.size()==nGnss && sAcc.size()==nGnss);

    // ---- coordinate conversion prep ------------------------------------
    GeographicLib::LocalCartesian lc(lat[0], lon[0], hMSL[0]);
    std::vector<Vector3> nedPos(nGnss);
    for (int i=0;i<nGnss;++i) nedPos[i] = toNED(lc, lat[i], lon[i], hMSL[i]);

    // ---- graph / initial values ---------------------------------------
    NonlinearFactorGraph graph;
    Values               values;

    Pose3   pose0(Rot3(), nedPos[0]);
    Vector3 vel0(velN[0], velE[0], velD[0]);
    imuBias::ConstantBias bias0;

    // orientation sigmas (rad) remain heuristic
    Vector6 priorSig;
    priorSig << M_PI, M_PI, M_PI,     // roll,pitch,yaw
        std::max(hAcc[0], kMinSigma), // N
        std::max(hAcc[0], kMinSigma), // E
        std::max(vAcc[0], kMinSigma); // D

    graph.addPrior<Pose3>(X(0), pose0, noiseModel::Diagonal::Sigmas(priorSig));

    double velSigma = std::max(sAcc[0], kMinSigma);
    graph.addPrior<Vector3>(V(0), vel0, noiseModel::Isotropic::Sigma(3, velSigma));

    graph.addPrior<imuBias::ConstantBias>(B(0), bias0, noiseModel::Isotropic::Sigma(6,1e-3));

    values.insert(X(0), pose0);
    values.insert(V(0), vel0);
    values.insert(B(0), bias0);

    // ---- IMU preintegration -----------------------------------------
    auto pimParams = makeImuParams(aAcc, wAcc);
    auto pim       = std::make_shared<PreintegratedCombinedMeasurements>(pimParams, bias0);

    NavState prevState(pose0, vel0);
    imuBias::ConstantBias prevBias = bias0;

    size_t imuIdx = 1; // second IMU sample
    int    idx    = 0; // graph index

    // ---- graph building loop ------------------------------------------
    for (int g=1; g<nGnss; ++g) {
        double gnssT = gnssTime[g];
        // integrate IMU until GNSS time
        for (; imuIdx<nImu && imuTime[imuIdx]<=gnssT; ++imuIdx) {
            double dt = imuTime[imuIdx] - imuTime[imuIdx-1];
            Vector3 acc(imuAx[imuIdx-1]*kG2ms2,
                        imuAy[imuIdx-1]*kG2ms2,
                        imuAz[imuIdx-1]*kG2ms2);
            Vector3 gyr(imuWx[imuIdx-1]*kDeg2Rad,
                        imuWy[imuIdx-1]*kDeg2Rad,
                        imuWz[imuIdx-1]*kDeg2Rad);
            pim->integrateMeasurement(acc, gyr, dt);
        }

        int next = ++idx;
        const auto& pimConst = dynamic_cast<const PreintegratedCombinedMeasurements&>(*pim);
        graph.emplace_shared<CombinedImuFactor>(X(next-1),V(next-1),X(next),V(next),B(next-1),B(next),pimConst);

        auto gpsNoise = makePosNoise(hAcc[g], vAcc[g]);
        graph.emplace_shared<GPSFactor>(X(next), Point3(nedPos[g]), gpsNoise);

        NavState pred = pim->predict(prevState, prevBias);
        values.insert(X(next), pred.pose());
        values.insert(V(next), pred.v());
        values.insert(B(next), prevBias);

        // reset preint buffer for next segment
        prevState = pred; // only as starting guess, bias unchanged
        pim->resetIntegrationAndSetBias(prevBias);
    }

    // ---- single optimisation pass -------------------------------------
    LevenbergMarquardtParams lm; lm.maxIterations = 100;
    LevenbergMarquardtOptimizer opt(graph, values, lm);
    Values res = opt.optimize();

    // ---- build FusionOutput from *final* trajectory -------------------
    FusionOutput out;

    // pre‑compute velocity list for acceleration finite diff
    std::vector<Vector3> velList(idx+1);
    for (int k=0;k<=idx;++k) velList[k] = res.at<Vector3>(V(k));

    for (int k=0;k<=idx;++k) {
        double t = gnssTime[k];
        Pose3 pose = res.at<Pose3>(X(k));
        Vector3 vel = velList[k];
        Vector3 acc;
        if (k == 0) {
            acc.setZero();
        } else {
            acc = (vel - velList[k-1]) / (t - gnssTime[k-1]);
        }

        out.time.append(t);
        out.posN.append(pose.translation().x());
        out.posE.append(pose.translation().y());
        out.posD.append(pose.translation().z());
        out.velN.append(vel.x());
        out.velE.append(vel.y());
        out.velD.append(vel.z());
        out.accN.append(acc.x());
        out.accE.append(acc.y());
        out.accD.append(acc.z());
        Vector3 rpy = pose.rotation().rpy();
        out.roll .append(rpy.x()*kRad2Deg);
        out.pitch.append(rpy.y()*kRad2Deg);
        out.yaw  .append(rpy.z()*kRad2Deg);
    }
    return out;
}

} // namespace FlySight
