/*****************************************************************************
 * File: imugnssekf.cpp
 *
 * Demonstrates an incremental (iSAM2-based) IMU+GNSS fusion in GTSAM:
 *   - Accumulate IMU data between GNSS epochs
 *   - Add 1 new node + CombinedImuFactor + GnssFactor for each GNSS epoch
 *   - Update the iSAM2 solver incrementally
 * This avoids building a single massive factor graph in batch mode,
 * preventing stack overflows.
 *****************************************************************************/

#include "imugnssekf.h"    // header that defines FusionOutput, etc.
#include "gnssfactor.h"

#include <QVector>
#include <algorithm>
#include <vector>
#include <cmath>
#include <iostream>

// GTSAM headers
#include <gtsam/navigation/CombinedImuFactor.h>
#include <gtsam/navigation/PreintegrationParams.h>
#include <gtsam/slam/PriorFactor.h>
#include <gtsam/slam/BetweenFactor.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/ISAM2.h>
#include <gtsam/navigation/ImuFactor.h>

#include <GeographicLib/LocalCartesian.hpp>
#include <boost/make_shared.hpp>

using namespace gtsam;
using gtsam::symbol_shorthand::X; // Pose
using gtsam::symbol_shorthand::V; // Velocity
using gtsam::symbol_shorthand::B; // Bias;

namespace FlySight {

// Simple structs if not declared elsewhere
struct ImuData {
    double t;   // seconds
    double ax;  // m/s^2
    double ay;
    double az;
    double wx;  // rad/s
    double wy;
    double wz;
};

struct GnssData {
    double t;    // seconds
    double lat;  // deg
    double lon;  // deg
    double alt;  // m
    double velN; // m/s
    double velE;
    double velD;
    double hAcc; // m
    double vAcc; // m
    double sAcc; // m/s
};

//------------------------------------------------------------------------------
/** runFusion: iSAM2-based incremental GNSS+IMU integration
 *
 * 1) We accumulate IMU between GNSS times.
 * 2) For each GNSS epoch, we add exactly one new state (Pose/Vel/Bias),
 *    along with one CombinedImuFactor and a GnssFactor.
 * 3) We call iSAM2.update(...) incrementally. This avoids building a
 *    huge batch factor graph all at once (which can cause stack overflow).
 */
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
    const QVector<double>& imuWz,
    double Aacc, // IMU accel accuracy [g]
    double wAcc  // IMU gyro accuracy [deg/s]
    ) {
    FusionOutput output;

    // Basic checks
    if (gnssTime.isEmpty() || lat.isEmpty() || lon.isEmpty() || hMSL.isEmpty() ||
        velN.isEmpty()   || velE.isEmpty() || velD.isEmpty() ||
        hAcc.isEmpty()   || vAcc.isEmpty() || sAcc.isEmpty() ||
        imuTime.isEmpty()|| imuAx.isEmpty()|| imuAy.isEmpty()|| imuAz.isEmpty()||
        imuWx.isEmpty()  || imuWy.isEmpty() || imuWz.isEmpty())
    {
        // Not enough data
        return output;
    }

    // 1) Convert IMU data
    std::vector<ImuData> imuQueue;
    imuQueue.reserve(imuTime.size());
    for(int i=0; i<imuTime.size(); i++){
        ImuData d;
        d.t  = imuTime[i];
        d.ax = imuAx[i] * 9.81;
        d.ay = imuAy[i] * 9.81;
        d.az = imuAz[i] * 9.81;
        d.wx = imuWx[i] * M_PI/180.0;
        d.wy = imuWy[i] * M_PI/180.0;
        d.wz = imuWz[i] * M_PI/180.0;
        imuQueue.push_back(d);
    }
    std::sort(imuQueue.begin(), imuQueue.end(), [](auto&a, auto&b){
        return a.t < b.t;
    });

    // 2) Convert GNSS data
    std::vector<GnssData> gnssQueue;
    gnssQueue.reserve(gnssTime.size());
    for(int i=0; i<gnssTime.size(); i++){
        GnssData g;
        g.t    = gnssTime[i];
        g.lat  = lat[i];
        g.lon  = lon[i];
        g.alt  = hMSL[i];
        g.velN = velN[i];
        g.velE = velE[i];
        g.velD = velD[i];
        g.hAcc = hAcc[i];
        g.vAcc = vAcc[i];
        g.sAcc = sAcc[i];
        gnssQueue.push_back(g);
    }
    std::sort(gnssQueue.begin(), gnssQueue.end(), [](auto &a, auto &b){
        return a.t < b.t;
    });

    if(imuQueue.empty() || gnssQueue.empty()) {
        return output;
    }

    // 3) Local Cartesian system
    GnssData firstGnss = gnssQueue.front();
    GeographicLib::LocalCartesian nedConverter(firstGnss.lat, firstGnss.lon, firstGnss.alt);
    auto toNED = [&](double la, double lo, double alt){
        double x,y,z;
        nedConverter.Forward(la, lo, alt, x,y,z);
        return gtsam::Vector3(x,y,z);
    };

    // 4) Setup iSAM2
    ISAM2Params isamParams;
    // You can experiment with different relinearize thresholds, solver types, etc.
    // For instance:
    // isamParams.optimizationParams = ISAM2GaussNewtonParams(0.001);
    // isamParams.setFactorization("CHOLESKY");
    // isamParams.setRelinearizeSkip(1);
    // ...
    ISAM2 isam(isamParams);

    // 5) Setup Preintegration parameters
    double gravity = 9.81;
    auto p = PreintegrationCombinedParams::MakeSharedU(gravity);

    // Convert user IMU specs to SI
    double accelNoise = Aacc * 9.81;         // from g => m/s^2
    double gyroNoise  = wAcc * M_PI / 180.0; // from deg/s => rad/s
    p->setAccelerometerCovariance(I_3x3 * (accelNoise*accelNoise));
    p->setGyroscopeCovariance   (I_3x3 * (gyroNoise * gyroNoise));
    p->setIntegrationCovariance (I_3x3 * 1e-6);

    // Typical random-walk for biases
    double accelBiasRW = 1e-4, gyroBiasRW = 1e-5;
    p->setBiasAccCovariance  (I_3x3 * (accelBiasRW*accelBiasRW));
    p->setBiasOmegaCovariance(I_3x3 * (gyroBiasRW * gyroBiasRW));

    // 6) Initialize state: from first GNSS
    int stateIndex = 0;
    Pose3 initPose(Rot3::Yaw(0.0), toNED(firstGnss.lat, firstGnss.lon, firstGnss.alt));
    Vector3 initVel(firstGnss.velN, firstGnss.velE, firstGnss.velD);
    imuBias::ConstantBias initBias;  // zero bias guess

    // We'll create an initial factor graph & values just for this first state
    NonlinearFactorGraph newFactors;
    Values newValues;

    // Provide a prior on Pose/Vel/Bias
    auto priorPoseNoise = noiseModel::Diagonal::Sigmas(
        (Vector(6) << Vector3::Constant(1.0), Vector3::Constant(0.1)).finished());
    auto priorVelNoise  = noiseModel::Isotropic::Sigma(3, 1.0);
    auto priorBiasNoise = noiseModel::Isotropic::Sigma(6, 1e-2);

    newFactors.add(PriorFactor<Pose3>(X(stateIndex), initPose, priorPoseNoise));
    newFactors.add(PriorFactor<Vector3>(V(stateIndex), initVel, priorVelNoise));
    newFactors.add(PriorFactor<imuBias::ConstantBias>(B(stateIndex), initBias, priorBiasNoise));

    // Insert initial guesses
    newValues.insert(X(stateIndex), initPose);
    newValues.insert(V(stateIndex), initVel);
    newValues.insert(B(stateIndex), initBias);

    // Update iSAM2 with the initial state
    isam.update(newFactors, newValues);
    isam.update(); // optional extra call to finalize
    Values curEstimate = isam.calculateEstimate();

    // Keep track of state times for the final output
    std::vector<double> stateTimes;
    stateTimes.push_back(gnssQueue.front().t);

    // 7) Now accumulate IMU between GNSS epochs, each time add one new state
    size_t imuIdx = 0;
    // move IMU index up to the first state time
    double prevTime = gnssQueue.front().t;
    while (imuIdx < imuQueue.size() && imuQueue[imuIdx].t < prevTime) {
        imuIdx++;
    }

    // For each subsequent GNSS measurement, create a single CombinedImuFactor
    // from [prevTime -> newGnssTime], and a new GnssFactor
    for (size_t gIdx = 1; gIdx < gnssQueue.size(); gIdx++) {
        GnssData gd = gnssQueue[gIdx];
        double currTime = gd.t;

        NonlinearFactorGraph stepFactors;
        Values stepValues;

        // Preintegrate IMU from prevTime -> currTime
        auto preint = boost::make_shared<PreintegratedCombinedMeasurements>(p, initBias);

        double lastImuT = prevTime;
        while (imuIdx < imuQueue.size() && imuQueue[imuIdx].t <= currTime) {
            const ImuData& imu = imuQueue[imuIdx];
            double dt = imu.t - lastImuT;
            if(dt>0) {
                Vector3 acc(imu.ax, imu.ay, imu.az);
                Vector3 gyr(imu.wx, imu.wy, imu.wz);
                preint->integrateMeasurement(acc, gyr, dt);
            }
            lastImuT = imu.t;
            imuIdx++;
        }

        int oldIdx = stateIndex;
        stateIndex++;
        int newIdx = stateIndex;

        // CombinedImuFactor between old state -> new state
        stepFactors.add( CombinedImuFactor(
            X(oldIdx), V(oldIdx),
            X(newIdx), V(newIdx),
            B(oldIdx), B(newIdx),
            *preint) );

        // Add a bias "between" factor for slight random walk
        auto biasNoise = noiseModel::Isotropic::Sigma(6,1e-3);
        stepFactors.add( BetweenFactor<imuBias::ConstantBias>(
            B(oldIdx), B(newIdx),
            imuBias::ConstantBias(), biasNoise ) );

        // Insert a predicted initial guess for X(newIdx), V(newIdx), B(newIdx)
        {
            Pose3   prevPose = curEstimate.at<Pose3>(X(oldIdx));
            Vector3 prevVel  = curEstimate.at<Vector3>(V(oldIdx));
            imuBias::ConstantBias prevBias = curEstimate.at<imuBias::ConstantBias>(B(oldIdx));

            NavState oldNav(prevPose, prevVel);
            NavState propNav = preint->predict(oldNav, prevBias);

            stepValues.insert(X(newIdx), propNav.pose());
            stepValues.insert(V(newIdx), propNav.v());
            stepValues.insert(B(newIdx), prevBias);
        }

        // Now add GNSS factor
        {
            Vector3 nedPos = toNED(gd.lat, gd.lon, gd.alt);
            Vector3 nedVel(gd.velN, gd.velE, gd.velD);
            Vector6 meas;
            meas << nedPos.x(), nedPos.y(), nedPos.z(),
                nedVel.x(), nedVel.y(), nedVel.z();

            Vector6 sigmas;
            sigmas << gd.hAcc, gd.hAcc, gd.vAcc,
                gd.sAcc, gd.sAcc, gd.sAcc;
            auto gnssModel = noiseModel::Diagonal::Sigmas(sigmas);
            stepFactors.add( GnssFactor(X(newIdx), V(newIdx), meas, gnssModel) );
        }

        // Update iSAM2 with this "mini-graph"
        isam.update(stepFactors, stepValues);
        isam.update();
        curEstimate = isam.calculateEstimate();

        // Next iteration
        prevTime = currTime;
        initBias = curEstimate.at<imuBias::ConstantBias>(B(newIdx));

        // Save the time for output
        stateTimes.push_back(currTime);
    }

    // 8) Final solution is in curEstimate. Let’s fill out FusionOutput

    // Reserve space for all states
    size_t numStates = stateTimes.size();
    output.time.resize(numStates);
    output.posN.resize(numStates, 0.0);
    output.posE.resize(numStates, 0.0);
    output.posD.resize(numStates, 0.0);
    output.velN.resize(numStates, 0.0);
    output.velE.resize(numStates, 0.0);
    output.velD.resize(numStates, 0.0);
    output.accN.resize(numStates, 0.0);
    output.accE.resize(numStates, 0.0);
    output.accD.resize(numStates, 0.0);
    output.roll.resize(numStates, 0.0);
    output.pitch.resize(numStates, 0.0);
    output.yaw.resize(numStates, 0.0);

    // i = index in the final states
    for(size_t i = 0; i < numStates; i++) {
        // Save time
        output.time[i] = stateTimes[i];

        // Pose => position + orientation
        // Make sure X(i) is valid if i <= stateIndex
        // (i.e. stateIndex should be at least numStates-1)
        Pose3 poseI = curEstimate.at<Pose3>( X((int)i) );
        Vector3 trans = poseI.translation();   // (N, E, D)
        Rot3 rot = poseI.rotation();

        // Fill position
        output.posN[i] = trans.x();
        output.posE[i] = trans.y();
        output.posD[i] = trans.z();

        // Extract RPY angles (in radians)
        double r = rot.roll();   // rotation about X
        double p = rot.pitch();  // rotation about Y
        double y = rot.yaw();    // rotation about Z

        // Convert to degrees
        r *= (180.0/M_PI);
        p *= (180.0/M_PI);
        y *= (180.0/M_PI);

        output.roll[i]  = r;
        output.pitch[i] = p;
        output.yaw[i]   = y;

        // Velocity
        Vector3 vel = curEstimate.at<Vector3>( V((int)i) );

        output.velN[i] = vel.x();
        output.velE[i] = vel.y();
        output.velD[i] = vel.z();
    }

    // 9) Compute symmetrical acceleration using velocity at each state
    //    (same logic you had before, but we store in accN/E/D).
    for(size_t i = 1; i + 1 < numStates; i++) {
        double dtPlus  = stateTimes[i+1] - stateTimes[i];
        double dtMinus = stateTimes[i]   - stateTimes[i-1];
        Vector3 vMinus = curEstimate.at<Vector3>(V((int)(i-1)));
        Vector3 vPlus  = curEstimate.at<Vector3>(V((int)(i+1)));
        Vector3 aNED   = (vPlus - vMinus)/(dtPlus + dtMinus);

        // Convert to "g"
        double invG = 1.0 / 9.81;
        output.accN[i] = aNED.x() * invG;
        output.accE[i] = aNED.y() * invG;
        output.accD[i] = aNED.z() * invG;
    }

    return output;
}

} // namespace FlySight
