#include "imugnssekf.h"

#include <QtGlobal>
#include <GeographicLib/LocalCartesian.hpp>

// --- GTSAM Includes ---
#include <gtsam/inference/Symbol.h>
#include <gtsam/navigation/CombinedImuFactor.h>
#include <gtsam/navigation/GPSFactor.h>
#include <gtsam/navigation/ImuBias.h>
#include <gtsam/navigation/NavState.h>
#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>
#include <gtsam/nonlinear/ISAM2.h>
#include <gtsam/linear/linearExceptions.h>
#include <gtsam/geometry/Rot3.h>
#include <gtsam/geometry/Point3.h>
#include <gtsam/geometry/Pose3.h>
#include <gtsam/linear/NoiseModel.h>
#include <gtsam/base/Matrix.h>
#include <Eigen/Geometry>
#include <Eigen/StdVector>

// --- Standard Includes ---
#include <vector>
#include <cassert>
#include <cmath>
#include <algorithm>

// --- Qt Includes ---
#include <QDebug>
#include <QVector>

namespace FlySight {
using namespace gtsam;
using symbol_shorthand::B;
using symbol_shorthand::V;
using symbol_shorthand::X;

// ---- constants -----------------------------------------------------------
static constexpr double kDeg2Rad = M_PI / 180.0;
static constexpr double kRad2Deg = 1.0 / kDeg2Rad;
static constexpr double kG2ms2   = 9.80665;
static constexpr double kms22G   = 1.0 / kG2ms2;
static constexpr double kMinSigma = 1e-3;   // 1 mm floor to avoid singularities

// ---- helpers -------------------------------------------------------------
struct StartIndices {
    bool success = false;
    int gnssIdx = -1;
    int imuIdx = -1;
};

static StartIndices findStartIndices(
    const QVector<double>& gnssTime,
    const QVector<double>& hAcc,
    const QVector<double>& imuTime,
    double hAccInitThreshold)
{
    StartIndices result;
    const int nGnss = gnssTime.size();
    const int nImu = imuTime.size();

    result.gnssIdx = -1;
    for (int g = 0; g < nGnss; ++g) {
        if (hAcc[g] < hAccInitThreshold) {
            result.gnssIdx = g;
            break;
        }
    }

    if (result.gnssIdx == -1) {
#ifdef QT_DEBUG
        qWarning() << "Could not find any GNSS point with hAcc <" << hAccInitThreshold;
#endif
        result.success = false;
        return result;
    }

    double startTime = gnssTime[result.gnssIdx];
    result.imuIdx = -1;
    for (int i = 0; i < nImu; ++i) {
        if (imuTime[i] >= startTime) {
            result.imuIdx = i;
            break;
        }
    }

    if (result.imuIdx == -1) {
#ifdef QT_DEBUG
        qWarning() << "Could not find any IMU point after the selected GNSS start time:" << startTime;
#endif
        result.success = false;
        return result;
    }

#ifdef QT_DEBUG
    qInfo() << "Found start indices - GNSS:" << result.gnssIdx << "(t=" << gnssTime[result.gnssIdx] << ", hAcc=" << hAcc[result.gnssIdx] << ")"
            << "IMU:" << result.imuIdx << "(t=" << imuTime[result.imuIdx] << ")";
#endif

    result.success = true;
    return result;
}


static gtsam::Rot3 calculateInitialOrientation(
    const QVector<double>& imuAx,
    const QVector<double>& imuAy,
    const QVector<double>& imuAz,
    int nImu,
    int imuStartIndex,
    int numSamplesToAvg = 10)
{
    using namespace gtsam;

    int avgEnd = imuStartIndex;
    int avgStart = std::max(0, avgEnd - numSamplesToAvg);

    if (avgEnd <= avgStart) {
#ifdef QT_DEBUG
        qWarning() << "Not enough preceding IMU samples (" << avgStart << "to" << avgEnd << ") for initial orientation calculation near IMU start index" << imuStartIndex << ". Using identity rotation.";
#endif
        return Rot3();
    }

    Vector3 avgAcc = Vector3::Zero();
    int count = 0;
    for (int i = avgStart; i < avgEnd; ++i) {
        if (i >= 0 && i < nImu) {
            avgAcc += Vector3(imuAx[i], imuAy[i], imuAz[i]);
            count++;
        }
    }

    if (count == 0) {
#ifdef QT_DEBUG
        qWarning() << "No valid IMU samples found in averaging window [" << avgStart << "," << avgEnd << "). Using identity rotation.";
#endif
        return Rot3();
    }

    avgAcc /= count;
    avgAcc *= kG2ms2;

    double avgAccNorm = avgAcc.norm();
    if (avgAccNorm < kG2ms2 / 2.0 || avgAccNorm > kG2ms2 * 2.0 || avgAccNorm < 1e-9 ) {
#ifdef QT_DEBUG
        qWarning() << "Initial acceleration magnitude (" << avgAccNorm << "m/s^2) from samples [" << avgStart << "," << avgEnd << ") too far from g (" << kG2ms2 << ") or near zero. Using identity rotation.";
#endif
        return Rot3();
    }

    Vector3 body_grav_vector = -avgAcc;
    Vector3 nav_grav_vector(0.0, 0.0, kG2ms2);
    Eigen::Quaterniond q = Eigen::Quaterniond::FromTwoVectors(body_grav_vector, nav_grav_vector);
    Rot3 rot0 = Rot3(q);

#ifdef QT_DEBUG
    qDebug() << "Calculated Initial RPY (deg) using IMU samples [" << avgStart << "," << avgEnd << "):"
             << rot0.roll() * kRad2Deg
             << rot0.pitch() * kRad2Deg
             << rot0.yaw() * kRad2Deg;
#endif
    return rot0;
}

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
makeImuParams(double gMag = kG2ms2) {
    // Based on https://www.st.com/resource/en/datasheet/lsm6dso.pdf

    // datasheet‑derived noise densities
    double accNoiseDensity  = 80e-6 * kG2ms2;     // 80 µg/√Hz  → 7.85e‑4 m/s²/√Hz
    double gyroNoiseDensity = 3.8e-3 * kDeg2Rad;  // 3.8 mdps/√Hz → 6.63e‑5 rad/s/√Hz
    Matrix33 Qa = I_3x3 * std::pow(accNoiseDensity , 2);
    Matrix33 Qg = I_3x3 * std::pow(gyroNoiseDensity, 2);

    // Bias testing in Data comp 1 - FS 2 - serie nr 2 - 01465 (test 08)\24-09-07
    //
    // accBiasRW / gyroBiasRW
    //   - 1e-1 / 1e-1 crashed on 10-16-23
    //   - 1e-2 / 1e-2 worked on all but 10-16-23
    //   - 1e-2 / 1e-3 worked on all but 10-16-23
    //   - 1e-3 / 1e-4 crashed on 16-05-34
    //   - 1e-3 / 1e-4 worked on 17-26-24

    // empirical bias random walks
    double accBiasRw  = 1e-2; // m/s²/√s
    double gyroBiasRw = 1e-3; // rad/s/√s
    Matrix33 Qba = I_3x3 * std::pow(accBiasRw , 2);
    Matrix33 Qbg = I_3x3 * std::pow(gyroBiasRw, 2);

    // assemble the params object
    auto p = PreintegratedCombinedMeasurements::Params::MakeSharedD(gMag);
    p->accelerometerCovariance = Qa;
    p->gyroscopeCovariance     = Qg;
    p->integrationCovariance   = I_3x3 * 1e-8;
    p->biasAccCovariance       = Qba;
    p->biasOmegaCovariance     = Qbg;
    p->biasAccOmegaInt         = Z_6x6;
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
                       const QVector<double>& imuWz) {
    const int nGnss = gnssTime.size();
    const int nImu  = imuTime.size();
    Q_ASSERT(nGnss>1 && nImu>1);
    Q_ASSERT(hAcc.size()==nGnss && vAcc.size()==nGnss && sAcc.size()==nGnss);

    // ---- Find valid starting point ----
    const double hAccInitThreshold = 10.0;
    StartIndices startInfo = findStartIndices(gnssTime, hAcc, imuTime, hAccInitThreshold);
    if (!startInfo.success) {
        qWarning() << "Failed to find suitable start point. Returning empty output.";
        return FusionOutput();
    }
    const int startGnssIdx = startInfo.gnssIdx;
    const int startImuIdx  = startInfo.imuIdx;

    // ---- coordinate conversion prep ----
    GeographicLib::LocalCartesian lc(lat[startGnssIdx], lon[startGnssIdx], hMSL[startGnssIdx]);
    std::vector<Vector3, Eigen::aligned_allocator<Vector3>> nedPos(nGnss);
    for (int i=0;i<nGnss;++i)
        nedPos[i] = toNED(lc, lat[i], lon[i], hMSL[i]);

    // ---- iSAM2 Setup ----
    ISAM2Params isamParams;
    isamParams.relinearizeThreshold = 0.01;
    isamParams.relinearizeSkip = 1;
    ISAM2 isam(isamParams);

    NonlinearFactorGraph graphFactors;
    Values               initialEstimate;

    // ---- Initial State Setup ----
    Rot3 rot0 = calculateInitialOrientation(imuAx, imuAy, imuAz, nImu, startImuIdx);
    Pose3   pose0(rot0, nedPos[startGnssIdx]);
    Vector3 vel0(velN[startGnssIdx], velE[startGnssIdx], velD[startGnssIdx]);
    imuBias::ConstantBias bias0;

    // ---- Set up priors ----
    Vector6 priorPoseSig;
    priorPoseSig << 0.5, 0.5, M_PI,
        std::max(hAcc[startGnssIdx], kMinSigma),
        std::max(hAcc[startGnssIdx], kMinSigma),
        std::max(vAcc[startGnssIdx], kMinSigma);
    graphFactors.addPrior<Pose3>(X(0), pose0, noiseModel::Diagonal::Sigmas(priorPoseSig));

    double velSigma = std::max(sAcc[startGnssIdx], kMinSigma);
    graphFactors.addPrior<Vector3>(V(0), vel0, noiseModel::Isotropic::Sigma(3, velSigma));

    graphFactors.addPrior<imuBias::ConstantBias>(B(0), bias0, noiseModel::Isotropic::Sigma(6, 0.1));

    initialEstimate.insert(X(0), pose0);
    initialEstimate.insert(V(0), vel0);
    initialEstimate.insert(B(0), bias0);


    // ---- Initialize iSAM2 ----
    try {
        isam.update(graphFactors, initialEstimate);
        qInfo() << "iSAM2 initialized successfully.";
    } catch (const std::exception& e) {
        qWarning() << "Exception during iSAM2 initialization:" << e.what();
        return FusionOutput();
    } catch (...) {
        qWarning() << "Unknown exception during iSAM2 initialization";
        return FusionOutput();
    }
    Values currentEstimate = isam.calculateEstimate();

    // ---- IMU preintegration ----
    auto pimParams = makeImuParams();
    auto pim = std::make_shared<PreintegratedCombinedMeasurements>(pimParams, currentEstimate.at<imuBias::ConstantBias>(B(0)));

    size_t currentImuIdx = startImuIdx;
    QVector<int> graphGnssIndices;
    graphGnssIndices.append(startGnssIdx);

    // ---- Incremental Update Loop ----
    for (int g = startGnssIdx + 1; g < nGnss; ++g) {
        double currentGnssTime = gnssTime[g];
        int k = g - startGnssIdx;
        int prev_k = k - 1;

        Pose3   prevPose = currentEstimate.at<Pose3>(X(prev_k));
        Vector3 prevVel  = currentEstimate.at<Vector3>(V(prev_k));
        imuBias::ConstantBias prevBias = currentEstimate.at<imuBias::ConstantBias>(B(prev_k));
        NavState prevState(prevPose, prevVel);

        pim->resetIntegrationAndSetBias(prevBias);

        // Integrate IMU
        for (; currentImuIdx < nImu && imuTime[currentImuIdx] <= currentGnssTime; ++currentImuIdx) {
            if (currentImuIdx == 0) continue;
            double dt = imuTime[currentImuIdx] - imuTime[currentImuIdx - 1];
            if (dt <= 0) {
#ifdef QT_DEBUG
                qWarning() << "Skipping IMU integration due to non-positive dt=" << dt << "at imuIdx=" << currentImuIdx;
#endif
                continue;
            }
            Vector3 acc(imuAx[currentImuIdx - 1] * kG2ms2, imuAy[currentImuIdx - 1] * kG2ms2, imuAz[currentImuIdx - 1] * kG2ms2);
            Vector3 gyr(imuWx[currentImuIdx - 1] * kDeg2Rad, imuWy[currentImuIdx - 1] * kDeg2Rad, imuWz[currentImuIdx - 1] * kDeg2Rad);
            pim->integrateMeasurement(acc, gyr, dt);
        }

        // Prepare for iSAM2 update
        NonlinearFactorGraph newFactors;
        Values               newValues;

        // Add IMU factor
        const auto& pimConst = dynamic_cast<const PreintegratedCombinedMeasurements&>(*pim);
        newFactors.emplace_shared<CombinedImuFactor>(X(prev_k), V(prev_k), X(k), V(k), B(prev_k), B(k), pimConst);

        // Add GPS factor
        auto gpsNoise = makePosNoise(hAcc[g], vAcc[g]);
        newFactors.emplace_shared<GPSFactor>(X(k), Point3(nedPos[g]), gpsNoise);

        // Predict initial guess for the new state
        NavState pred = pim->predict(prevState, prevBias);
        newValues.insert(X(k), pred.pose());
        newValues.insert(V(k), pred.v());
        newValues.insert(B(k), prevBias);

        // Update iSAM2
        try {
            isam.update(newFactors, newValues);
            currentEstimate = isam.calculateEstimate();
        } catch (const gtsam::IndeterminantLinearSystemException& e) { // Added gtsam:: namespace
            qWarning() << "IndeterminantLinearSystemException at GNSS index" << g << "(Graph node" << k << "):" << e.what();
            return FusionOutput();
        } catch (const std::exception& e) {
            qWarning() << "Standard exception during iSAM2 update at GNSS index" << g << "(Graph node" << k << "):" << e.what();
            return FusionOutput();
        } catch (...) {
            qWarning() << "Unknown exception during iSAM2 update at GNSS index" << g << "(Graph node" << k << ")";
            return FusionOutput();
        }

        graphGnssIndices.append(g);
    } // End of main loop

    int numGraphNodes = graphGnssIndices.size();
    if (numGraphNodes <= 1) {
        qWarning() << "Graph has too few nodes (" << numGraphNodes << ") after filtering. Cannot generate output.";
        return FusionOutput();
    }


    // ---- build FusionOutput ----
    Values res = currentEstimate;
    FusionOutput out;

    std::vector<Vector3, Eigen::aligned_allocator<Vector3>> velList(numGraphNodes);
    for (int k = 0; k < numGraphNodes; ++k) {
        if (res.exists(V(k))) {
            velList[k] = res.at<Vector3>(V(k));
        } else {
            qWarning() << "Velocity V(" << k << ") not found in final estimate!";
            velList[k] = Vector3::Zero();
        }
    }

    for (int k = 0; k < numGraphNodes; ++k) {
        int originalGnssIdx = graphGnssIndices[k];
        double t = gnssTime[originalGnssIdx];

        if (!res.exists(X(k))) {
            qWarning() << "Pose X(" << k << ") not found in final estimate! Skipping output point.";
            continue;
        }
        Pose3 pose = res.at<Pose3>(X(k));
        Vector3 vel = velList[k];
        Vector3 acc = Vector3::Zero();

        if (k > 0) {
            int prevOriginalGnssIdx = graphGnssIndices[k-1];
            double prev_t = gnssTime[prevOriginalGnssIdx];
            if (t > prev_t && res.exists(V(k-1))) {
                acc = (vel - velList[k-1]) / (t - prev_t);
            }
        }

        out.time.append(t);
        out.posN.append(pose.translation().x());
        out.posE.append(pose.translation().y());
        out.posD.append(pose.translation().z());
        out.velN.append(vel.x());
        out.velE.append(vel.y());
        out.velD.append(vel.z());
        out.accN.append(acc.x()*kms22G);
        out.accE.append(acc.y()*kms22G);
        out.accD.append(acc.z()*kms22G);
        Vector3 rpy = pose.rotation().rpy();
        out.roll .append(rpy.x()*kRad2Deg);
        out.pitch.append(rpy.y()*kRad2Deg);
        out.yaw  .append(rpy.z()*kRad2Deg);
    }
    return out;
}

} // namespace FlySight
