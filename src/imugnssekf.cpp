#include "imugnssekf.h"

#include <iostream>
#include <vector>
#include <deque>
#include <cmath>
#include <Eigen/Dense>
#include <QVector>
#include <GeographicLib/LocalCartesian.hpp>

#define _USE_MATH_DEFINES
#include <math.h>

// -----------------------------------------------------------------------------
// 1) Define a struct for the filter state and covariance
// -----------------------------------------------------------------------------
struct FilterState {
    // Position & velocity in NED (North, East, Down)
    Eigen::Vector3d position;  // [m]
    Eigen::Vector3d velocity;  // [m/s]

    // Orientation in quaternion form (rotation from body to NED)
    // w, x, y, z
    Eigen::Quaterniond orientation;

    // Biases (gyroscope & accelerometer)
    Eigen::Vector3d gyroBias;   // [deg/s]
    Eigen::Vector3d accelBias;  // [g]
};

// We'll keep a 15x15 covariance for [p, v, q, bg, ba] in a simplified approach.
static const int kStateDim = 15;

// Helper: Convert quaternion rates from gyro to update orientation
// (Naive approach: small-angle approximation)
Eigen::Quaterniond integrateGyro(const Eigen::Quaterniond &q,
                                 const Eigen::Vector3d &gyroDegPerSec,
                                 double dt)
{
    // Convert gyro from deg/s to rad/s
    Eigen::Vector3d gyro = gyroDegPerSec * M_PI/180.0;
    // For small dt, rotation quaternion ~ [1, 0.5*gyro*dt]
    double mag = gyro.norm();
    if (mag < 1e-12) {
        // No rotation
        return q;
    }

    // Axis of rotation
    Eigen::Vector3d axis = gyro / mag;
    double theta = mag * dt; // total angle
    double halfTheta = 0.5 * theta;
    double c = std::cos(halfTheta);
    double s = std::sin(halfTheta);

    Eigen::Quaterniond dq;
    dq.w() = c;
    dq.x() = axis.x() * s;
    dq.y() = axis.y() * s;
    dq.z() = axis.z() * s;

    // New orientation = q * dq  (body-to-NED)
    Eigen::Quaterniond newQ = q * dq;
    newQ.normalize();
    return newQ;
}

// Helper: Remove gravity from measured acceleration in NED
Eigen::Vector3d removeGravity(const Eigen::Vector3d& accelNED)
{
    // Standard gravity (approx.)
    // NED: gravity is +9.81 in the "down" direction -> negative in the -Z if Z is down?
    // Actually, in "NED" coordinate, "Down" is positive in the Z axis. So gravity vector is +9.81 in D.
    // So we might do:
    Eigen::Vector3d gNED(0, 0, 9.81);

    // Subtract gravity from the measured acceleration
    // If accelNED includes gravity, then "true linear accel" = accelNED - gNED
    // However, typically an accelerometer measures specific force,
    // which is "accel minus gravity." So check sign carefully.
    // Let's assume we must add or subtract as needed.
    // For clarity, let's suppose the raw IMU reading is "actual acceleration + gravity."
    // Then the net linear acceleration = accelNED - gNED.
    return accelNED - gNED;
}

// -----------------------------------------------------------------------------
// 2) A basic class for sensor fusion (EKF skeleton).
//    In real code, you'd want more robust and tested logic.
// -----------------------------------------------------------------------------
class ImuGnssEkf
{
public:
    ImuGnssEkf() {
        state_.position.setZero();
        state_.velocity.setZero();
        state_.orientation = Eigen::Quaterniond::Identity();
        state_.gyroBias.setZero();
        state_.accelBias.setZero();

        P_.setIdentity();
        P_ *= 1e3; // Large initial covariance for demonstration
    }

    void initialize(const FilterState &initState,
                    const Eigen::Matrix<double, kStateDim, kStateDim> &initCov)
    {
        state_ = initState;
        P_ = initCov;
    }

    // Predict step with IMU data
    void predict(const Eigen::Vector3d &accelG,
                 const Eigen::Vector3d &gyroDegPerSec,
                 double dt)
    {
        // 1) Remove biases
        Eigen::Vector3d accelCorrected = accelG - state_.accelBias;
        Eigen::Vector3d gyroCorrected  = gyroDegPerSec - state_.gyroBias;

        // 2) Transform body-frame acceleration to NED
        // Convert 'g' to m/s^2: 1g ~ 9.81 m/s^2
        Eigen::Vector3d accelMps2 = accelCorrected * 9.81;
        // Rotate by current orientation (body -> NED)
        Eigen::Vector3d accelNED = state_.orientation * accelMps2;

        // 3) Remove gravity if needed
        Eigen::Vector3d linearAccelNED = removeGravity(accelNED);

        // 4) Update velocity, position (Euler integration)
        state_.velocity += linearAccelNED * dt;
        state_.position += state_.velocity * dt;
        // (simple, naive approach)

        // 5) Update orientation from gyro
        state_.orientation = integrateGyro(state_.orientation, gyroCorrected, dt);

        // 6) Covariance propagation (highly simplified!)
        // Typically you'd compute the Jacobian F of your system and do:
        // P_ = F * P_ * F^T + Q;
        // For demonstration, we just expand P_ a bit each step
        double accelNoise = 0.01; // example noise
        double gyroNoise  = 0.01; // example noise
        double biasNoise  = 0.0001;

        // Very rough approximation:
        Eigen::Matrix<double, kStateDim, kStateDim> Q = Eigen::Matrix<double, kStateDim, kStateDim>::Zero();
        // Add some process noise to v, orientation, biases...
        Q(3,3)   = accelNoise;   // vx
        Q(4,4)   = accelNoise;   // vy
        Q(5,5)   = accelNoise;   // vz
        Q(6,6)   = gyroNoise;    // orientation part...
        Q(7,7)   = gyroNoise;
        Q(8,8)   = gyroNoise;
        // bias for gyro
        Q(9,9)   = biasNoise;
        Q(10,10) = biasNoise;
        Q(11,11) = biasNoise;
        // bias for accel
        Q(12,12) = biasNoise;
        Q(13,13) = biasNoise;
        Q(14,14) = biasNoise;

        // Just inflate P_ with Q
        P_ += Q * dt;
    }

    // Update with GNSS position + velocity
    // (We assume we already have them in NED coordinates, plus velocity in NED.)
    void updateGnss(const Eigen::Vector3d &gnssPosNED,
                    const Eigen::Vector3d &gnssVelNED,
                    const Eigen::Matrix<double, 6, 6> &R)
    {
        // Measurement z = [p, v]^T in NED
        Eigen::Matrix<double, 6, 1> z;
        z << gnssPosNED, gnssVelNED;

        // Predicted measurement = [state_.position, state_.velocity]
        Eigen::Matrix<double, 6, 1> h;
        h << state_.position, state_.velocity;

        // Measurement Jacobian H ~ 6x15, partial derivative of [p,v] w.r.t state
        // We'll do a simple block approach:
        Eigen::Matrix<double, 6, 15> H;
        H.setZero();
        // p => identity in top-left
        H.block<3,3>(0,0) = Eigen::Matrix3d::Identity();
        // v => identity in next 3x3
        H.block<3,3>(3,3) = Eigen::Matrix3d::Identity();
        // We skip orientation & biases in measurement model for p,v

        // Residual
        Eigen::Matrix<double, 6, 1> y = z - h;

        // S = H P H^T + R
        Eigen::Matrix<double, 6, 6> S = H * P_ * H.transpose() + R;

        // K = P H^T S^-1
        Eigen::Matrix<double, 15, 6> K = P_ * H.transpose() * S.inverse();

        // Update state
        Eigen::Matrix<double, 15, 1> dx = K * y;

        // Inject corrections into state
        // position
        state_.position += dx.segment<3>(0);
        // velocity
        state_.velocity += dx.segment<3>(3);
        // orientation
        {
            Eigen::Vector3d dTheta = dx.segment<3>(6); // small angle
            double angle = dTheta.norm();
            Eigen::Quaterniond dq;
            if (angle > 1e-12) {
                Eigen::Vector3d axis = dTheta / angle;
                dq = Eigen::AngleAxisd(angle, axis);
            } else {
                dq = Eigen::Quaterniond(1, 0, 0, 0);
            }
            state_.orientation = dq * state_.orientation;
            state_.orientation.normalize();
        }
        // gyro bias
        state_.gyroBias += dx.segment<3>(9);
        // accel bias
        state_.accelBias += dx.segment<3>(12);

        // Update covariance: P = (I - K H) P
        Eigen::Matrix<double, 15, 15> I = Eigen::Matrix<double, 15, 15>::Identity();
        P_ = (I - K * H) * P_;
        // For numerical stability, we might also symmetrize or apply stable update.
    }

    // Get the current state
    FilterState getState() const {
        return state_;
    }

    // Get current covariance
    Eigen::Matrix<double, kStateDim, kStateDim> getCov() const {
        return P_;
    }

private:
    FilterState state_;
    Eigen::Matrix<double, kStateDim, kStateDim> P_; // covariance
};

// -----------------------------------------------------------------------------
// 3) Example usage
// -----------------------------------------------------------------------------

// Helper to convert lat/lon/alt to local NED using GeographicLib
// The first call sets the "origin" so that subsequent calls produce NED
// relative to that origin.
class NEDConverter {
public:
    NEDConverter(double refLat, double refLon, double refAlt)
        : localCart_(refLat, refLon, refAlt, GeographicLib::Geocentric::WGS84())
    {
    }

    // lat, lon in degrees, alt in meters
    // returns (N, E, D) in meters
    Eigen::Vector3d toNED(double latDeg, double lonDeg, double altM) {
        double x, y, z;
        localCart_.Forward(latDeg, lonDeg, altM, x, y, z);
        // By convention, x->north, y->east, z->down
        return Eigen::Vector3d(x, y, z);
    }

private:
    GeographicLib::LocalCartesian localCart_;
};

// The main function that fuses GNSS/IMU and returns time + NED accelerations
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
    double aAcc, // IMU accel accuracy (g)
    double wAcc // IMU gyro accuracy (deg/s)
    )
{
    // 1) Choose a reference lat/lon/alt for local NED
    if (gnssTime.isEmpty()) {
        // No data -> return empty
        return FusionOutput();
    }
    double refLat = lat[0];
    double refLon = lon[0];
    double refAlt = hMSL[0];
    NEDConverter nedConverter(refLat, refLon, refAlt);

    // 2) Convert all GNSS data to NED
    //    We'll store in a simple struct
    struct GnssNED {
        double t;
        Eigen::Vector3d pos;   // [m] (NED)
        Eigen::Vector3d vel;   // [m/s] (NED)
        Eigen::Matrix<double,6,6> R; // measurement covariance
    };
    std::deque<GnssNED> gnssQueue;

    for (int i = 0; i < gnssTime.size(); ++i) {
        GnssNED g;
        g.t = gnssTime[i];
        g.pos = nedConverter.toNED(lat[i], lon[i], hMSL[i]);

        // velocities in NED are given directly: (velN[i], velE[i], velD[i])
        // so store them
        g.vel = Eigen::Vector3d(velN[i], velE[i], velD[i]);

        // Build a 6x6 covariance from hAcc[i], vAcc[i], sAcc[i]
        // For demonstration, assume:
        //   position covariance in horizontal (x,y) = hAcc^2,
        //   in down (z) = vAcc^2,
        //   velocity covariance in x,y,z = sAcc^2
        g.R.setZero();
        double hVar = hAcc[i] * hAcc[i];
        double vVar = vAcc[i] * vAcc[i];
        double sVar = sAcc[i] * sAcc[i];

        // position part
        g.R(0,0) = hVar; // N
        g.R(1,1) = hVar; // E
        g.R(2,2) = vVar; // D
        // velocity part
        g.R(3,3) = sVar;
        g.R(4,4) = sVar;
        g.R(5,5) = sVar;

        gnssQueue.push_back(g);
    }

    // 3) Prepare IMU data in a queue as well
    struct ImuData {
        double t;
        Eigen::Vector3d accelG;        // (g)
        Eigen::Vector3d gyroDegPerSec; // (deg/s)
    };
    std::deque<ImuData> imuQueue;

    for (int i = 0; i < imuTime.size(); ++i) {
        ImuData d;
        d.t = imuTime[i];
        d.accelG = Eigen::Vector3d(imuAx[i], imuAy[i], imuAz[i]);
        d.gyroDegPerSec = Eigen::Vector3d(imuWx[i], imuWy[i], imuWz[i]);
        imuQueue.push_back(d);
    }

    // 4) Initialize the filter
    ImuGnssEkf ekf;

    FilterState initState;
    initState.position = gnssQueue.front().pos; // from first GNSS
    initState.velocity = gnssQueue.front().vel; // from first GNSS
    initState.orientation = Eigen::Quaterniond::Identity(); // unknown heading =>  set to identity
    initState.gyroBias.setZero();
    initState.accelBias.setZero();

    Eigen::Matrix<double,kStateDim,kStateDim> Pinit = Eigen::Matrix<double,kStateDim,kStateDim>::Identity();
    Pinit *= 1e2; // big initial uncertainty
    ekf.initialize(initState, Pinit);

    // 5) Main fusion loop
    // We'll iterate in time from min(gnssTime, imuTime) to max(gnssTime, imuTime).
    // We produce an output record each time we process an IMU sample (for instance).

    double tStart = 0.0;
    if (!gnssQueue.empty()) {
        tStart = gnssQueue.front().t;
    }
    if (!imuQueue.empty()) {
        tStart = std::min(tStart, imuQueue.front().t);
    }
    double tEnd = 0.0;
    if (!gnssQueue.empty()) {
        tEnd = std::max(tEnd, gnssQueue.back().t);
    }
    if (!imuQueue.empty()) {
        tEnd = std::max(tEnd, imuQueue.back().t);
    }

    FusionOutput result;

    // We will sweep through IMU data in ascending time.
    // At each IMU sample, we do predict() from the *previous* time.
    // If a GNSS measurement(s) occurs between last IMU and current IMU, we do an update().

    double prevTime = (imuQueue.empty()) ? tStart : imuQueue.front().t;
    while (!imuQueue.empty()) {
        ImuData imu = imuQueue.front();
        imuQueue.pop_front();

        double currentTime = imu.t;
        double dt = currentTime - prevTime;
        if (dt < 0.0) {
            // Time is not strictly increasing—skip or handle error
            continue;
        }
        prevTime = currentTime;

        // 5a) Predict step
        ekf.predict(imu.accelG, imu.gyroDegPerSec, dt);

        // 5b) If any GNSS measurement times are <= currentTime, update filter
        while (!gnssQueue.empty() && gnssQueue.front().t <= currentTime) {
            GnssNED g = gnssQueue.front();
            gnssQueue.pop_front();
            ekf.updateGnss(g.pos, g.vel, g.R);
        }

        // 5c) Save output
        FilterState st = ekf.getState();
        // The acceleration in NED can be found by re-computing the body-frame acceleration
        // minus bias, then rotate to NED, etc.  But to keep it simple here, we'll just do:
        //   a_NED = orientation * ((acc - bias)*9.81) - gravity
        // This is essentially what we do in `predict()`.
        // So let's replicate it:

        Eigen::Vector3d accelMps2 = (imu.accelG - st.accelBias) * 9.81;
        Eigen::Vector3d accelNED  = st.orientation * accelMps2;
        // remove gravity
        Eigen::Vector3d aLinNED = removeGravity(accelNED);

        // Convert from m/s^2 to "g"
        Eigen::Vector3d aLinNED_g = aLinNED / 9.81;

        result.time.push_back(currentTime);
        result.accN.push_back(aLinNED_g.x());
        result.accE.push_back(aLinNED_g.y());
        result.accD.push_back(aLinNED_g.z());
    }

    // If there are still GNSS measurements left (beyond last IMU time),
    // you might optionally do final updates, but we won't produce new acceleration outputs.

    return result;
}


// -----------------------------------------------------------------------------
// 4) A stub main() that demonstrates how you might call runFusion()
// -----------------------------------------------------------------------------
#ifdef FUSION_EXAMPLE_MAIN
int main()
{
    // Imagine you have read all your data into these QVectors...
    QVector<double> gnssTime, lat, lon, hMSL, velN, velE, velD, hAcc, vAcc, sAcc;
    QVector<double> imuTime, imuAx, imuAy, imuAz, imuWx, imuWy, imuWz;
    double aAcc = 0.02; // e.g. 0.02 g
    double wAcc  = 0.5; // e.g. 0.5 deg/s
    QVector<double> magTime, magX, magY, magZ;
    double mAcc = 0.01; // placeholder

    // ... fill these containers with real data ...

    // Run the fusion
    FusionOutput out = runFusion(
        gnssTime, lat, lon, hMSL, velN, velE, velD, hAcc, vAcc, sAcc,
        imuTime, imuAx, imuAy, imuAz, imuWx, imuWy, imuWz, aAcc, wAcc,
        magTime, magX, magY, magZ, mAcc);

    // Print some results
    for (int i = 0; i < out.time.size(); ++i) {
        std::cout << out.time[i] << " "
                  << out.accN[i] << " "
                  << out.accE[i] << " "
                  << out.accD[i] << std::endl;
    }

    return 0;
}
#endif
