#include "imugnssekf.h"
#include "gnssfactor.h"

#include <QVector>
#include <algorithm>
#include <vector>

// GTSAM Headers
#include <gtsam/nonlinear/NonlinearFactor.h>  // ensure we have this
#include <gtsam/geometry/Rot3.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/navigation/ImuFactor.h>
#include <gtsam/navigation/CombinedImuFactor.h> // for CombinedImuFactor
#include <gtsam/navigation/PreintegrationParams.h>
#include <gtsam/slam/PriorFactor.h>
#include <gtsam/slam/BetweenFactor.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/inference/Symbol.h>
#include <gtsam/nonlinear/Marginals.h>

#include <GeographicLib/LocalCartesian.hpp>
#include <boost/make_shared.hpp>

using namespace gtsam;
using gtsam::symbol_shorthand::X; // Pose
using gtsam::symbol_shorthand::V; // Velocity
using gtsam::symbol_shorthand::B; // Bias;

namespace FlySight {

// -----------------------------------------------------------------------------
// Data containers for IMU and GNSS
// -----------------------------------------------------------------------------
struct ImuData {
    double t;    // [s]
    double ax;   // [m/s^2], body frame
    double ay;
    double az;
    double wx;   // [rad/s], body frame
    double wy;
    double wz;
};

struct GnssData {
    double t;    // [s]
    double lat;  // [deg]
    double lon;  // [deg]
    double alt;  // [m]
    double velN; // [m/s]
    double velE;
    double velD;
    double hAcc; // [m]
    double vAcc; // [m]
    double sAcc; // [m/s]
};

// -----------------------------------------------------------------------------
// Main runFusion function
// -----------------------------------------------------------------------------
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
    double Aacc, // IMU accel accuracy (g)
    double wAcc  // IMU gyro accuracy (deg/s)
    ) {
    FusionOutput output;

    // Basic checks
    if( gnssTime.isEmpty() || lat.isEmpty() || lon.isEmpty() || hMSL.isEmpty() ||
        velN.isEmpty()   || velE.isEmpty() || velD.isEmpty() ||
        hAcc.isEmpty()   || vAcc.isEmpty() || sAcc.isEmpty() ||
        imuTime.isEmpty()|| imuAx.isEmpty()|| imuAy.isEmpty()|| imuAz.isEmpty()||
        imuWx.isEmpty()  || imuWy.isEmpty() || imuWz.isEmpty())
    {
        // Missing data => return empty
        return output;
    }

    // 1) Convert IMU
    std::vector<ImuData> imuQueue;
    imuQueue.reserve(imuTime.size());
    for(int i=0; i<imuTime.size(); i++){
        ImuData d;
        d.t  = imuTime[i];
        // g->m/s^2
        d.ax = imuAx[i]*9.81;
        d.ay = imuAy[i]*9.81;
        d.az = imuAz[i]*9.81;
        // deg/s->rad/s
        d.wx = imuWx[i]*M_PI/180.0;
        d.wy = imuWy[i]*M_PI/180.0;
        d.wz = imuWz[i]*M_PI/180.0;
        imuQueue.push_back(d);
    }
    std::sort(imuQueue.begin(), imuQueue.end(), [](auto &a, auto &b){
        return a.t < b.t;
    });

    // 2) Convert GNSS
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

    // 3) Local Cartesian NED
    GnssData firstGnss = gnssQueue.front();
    GeographicLib::LocalCartesian nedConverter(firstGnss.lat, firstGnss.lon, firstGnss.alt);

    auto toNED = [&](double latDeg, double lonDeg, double altM){
        double x,y,z;
        nedConverter.Forward(latDeg, lonDeg, altM, x,y,z);
        return Vector3(x,y,z);
    };

    // 4) Setup Preintegration
    auto params = PreintegrationCombinedParams::MakeSharedU(9.81);
    double accelNoise = Aacc*9.81;
    double gyroNoise  = wAcc*M_PI/180.0;
    params->setAccelerometerCovariance(I_3x3*(accelNoise*accelNoise));
    params->setGyroscopeCovariance(I_3x3*(gyroNoise*gyroNoise));
    params->setIntegrationCovariance(I_3x3*1e-6);

    double accelBiasRW = 1e-4, gyroBiasRW = 1e-5;
    params->setBiasAccCovariance(I_3x3*(accelBiasRW*accelBiasRW));
    params->setBiasOmegaCovariance(I_3x3*(gyroBiasRW*gyroBiasRW));

    // 5) Build factor graph
    NonlinearFactorGraph graph;
    Values initial;

    // a) Prior on first pose/vel/bias
    int index = 0;
    ImuData firstImu = imuQueue.front();
    Vector3 initPos = toNED(firstGnss.lat, firstGnss.lon, firstGnss.alt);
    Rot3 initRot = Rot3::Yaw(0.0);
    Pose3 initPose(initRot, initPos);
    Vector3 initVel(firstGnss.velN, firstGnss.velE, firstGnss.velD);
    imuBias::ConstantBias initBias;

    initial.insert(X(index), initPose);
    initial.insert(V(index), initVel);
    initial.insert(B(index), initBias);

    auto priorNoisePose = noiseModel::Diagonal::Sigmas(
        (Vector(6)<< Vector3::Constant(1.0), Vector3::Constant(0.1)).finished());
    auto priorNoiseVel  = noiseModel::Isotropic::Sigma(3,1.0);
    auto priorNoiseBias = noiseModel::Isotropic::Sigma(6,1e-2);

    graph.add(PriorFactor<Pose3>(X(index), initPose, priorNoisePose));
    graph.add(PriorFactor<Vector3>(V(index), initVel, priorNoiseVel));
    graph.add(PriorFactor<imuBias::ConstantBias>(B(index), initBias, priorNoiseBias));

    // b) Preintegrated measurements
    auto preintegrated = boost::make_shared<PreintegratedCombinedMeasurements>(params, initBias);

    std::vector<double> timeStamps;
    timeStamps.push_back(firstImu.t);

    ImuData prevImu = firstImu;
    imuQueue.erase(imuQueue.begin());
    int nodeIndex = index;

    // 6) Main loop over IMU
    while(!imuQueue.empty()) {
        ImuData currImu = imuQueue.front();
        imuQueue.erase(imuQueue.begin());

        double dt = currImu.t - prevImu.t;
        if(dt <= 0.0) {
            prevImu = currImu;
            continue;
        }
        // integrate
        Vector3 acc(currImu.ax, currImu.ay, currImu.az);
        Vector3 gyr(currImu.wx, currImu.wy, currImu.wz);
        preintegrated->integrateMeasurement(acc, gyr, dt);

        // new node each IMU
        nodeIndex++;
        CombinedImuFactor imuFactor(X(nodeIndex-1), V(nodeIndex-1),
                                    X(nodeIndex),   V(nodeIndex),
                                    B(nodeIndex-1), B(nodeIndex),
                                    *preintegrated);
        graph.add(imuFactor);

        auto biasNoise = noiseModel::Isotropic::Sigma(6,1e-3);
        graph.add( BetweenFactor<imuBias::ConstantBias>(
            B(nodeIndex-1), B(nodeIndex),
            imuBias::ConstantBias(), biasNoise ));

        // predict
        NavState prevState(initRot, initPos, initVel);
        NavState propState = preintegrated->predict(prevState, initBias);
        Pose3 newPose = propState.pose();
        Vector3 newVel= propState.v();

        // Insert guess
        initial.insert(X(nodeIndex), newPose);
        initial.insert(V(nodeIndex), newVel);
        initial.insert(B(nodeIndex), initBias);

        timeStamps.push_back(currImu.t);

        // Insert any GNSS
        while(!gnssQueue.empty() && gnssQueue.front().t <= currImu.t) {
            GnssData gd = gnssQueue.front();
            gnssQueue.erase(gnssQueue.begin());

            Vector3 nedPos = toNED(gd.lat, gd.lon, gd.alt);
            Vector3 nedVel(gd.velN, gd.velE, gd.velD);
            Vector6 meas; meas << nedPos, nedVel;

            Vector6 sigmas;
            sigmas<< gd.hAcc, gd.hAcc, gd.vAcc,
                gd.sAcc, gd.sAcc, gd.sAcc;
            auto gnssModel = noiseModel::Diagonal::Sigmas(sigmas);

            // Add GnssFactor
            graph.add( GnssFactor(
                X(nodeIndex), V(nodeIndex), meas, gnssModel ) );
        }

        // reset integrator
        preintegrated = boost::make_shared<PreintegratedCombinedMeasurements>(params, initBias);

        prevImu = currImu;
        initRot = newPose.rotation();
        initPos = newPose.translation();
        initVel = newVel;
    }

    // 7) Optimize
    LevenbergMarquardtParams lmParams;
    lmParams.setMaxIterations(100);
    lmParams.setVerbosity("ERROR");
    LevenbergMarquardtOptimizer optimizer(graph, initial, lmParams);
    Values result = optimizer.optimize();

    // 8) Output symmetrical acceleration
    output.time.reserve(timeStamps.size());
    output.accN.reserve(timeStamps.size());
    output.accE.reserve(timeStamps.size());
    output.accD.reserve(timeStamps.size());

    for(size_t i=0; i<timeStamps.size(); i++){
        double t = timeStamps[i];
        output.time.push_back(t);

        if(i==0 || i+1==timeStamps.size()){
            output.accN.push_back(0.0);
            output.accE.push_back(0.0);
            output.accD.push_back(0.0);
            continue;
        }
        double dtPlus  = timeStamps[i+1] - timeStamps[i];
        double dtMinus = timeStamps[i]   - timeStamps[i-1];
        Vector3 vMinus = result.at<Vector3>(V(i-1));
        Vector3 vPlus  = result.at<Vector3>(V(i+1));
        Vector3 aNED   = (vPlus - vMinus)/(dtPlus + dtMinus);

        double gInv = 1.0/9.81;
        output.accN.push_back(aNED.x()*gInv);
        output.accE.push_back(aNED.y()*gInv);
        output.accD.push_back(aNED.z()*gInv);
    }

    return output;
}

} // namespace FlySight
