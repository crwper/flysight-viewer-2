#ifndef GNSSFACTOR_H
#define GNSSFACTOR_H

#include <gtsam/nonlinear/NonlinearFactor.h>
#include <gtsam/geometry/Pose3.h>

using namespace gtsam;

namespace FlySight {

// -----------------------------------------------------------------------------
// A custom GNSS factor: Pose3 + Vector3 -> position + velocity in ENU
// IMPORTANT: declare in global (or named) namespace, not anonymous
// -----------------------------------------------------------------------------
class GnssFactor : public NoiseModelFactor2<Pose3, Vector3> {
public:
    // Boilerplate so GTSAM recognizes GnssFactor as a NonlinearFactor
    typedef GnssFactor This;
    typedef boost::shared_ptr<This> shared_ptr;
    typedef NoiseModelFactor2<Pose3, Vector3> Base;
    GTSAM_MAKE_ALIGNED_OPERATOR_NEW

        private:
                  Vector6 measured_; // [pE, pN, pU, vE, vN, vU]

public:
    GnssFactor(Key poseKey, Key velKey,
               const Vector6& measured,
               const SharedNoiseModel& model)
        : Base(model, poseKey, velKey),
        measured_(measured) {}

    // GTSAM 4.x expects raw pointers for optional Jacobians
    virtual Vector evaluateError(
        const Pose3& pose, const Vector3& vel,
        Matrix* H1 = nullptr, Matrix* H2 = nullptr
        ) const override {

        // Position residual in ENU
        Vector3 p = pose.translation();
        Vector3 posError = p - measured_.head<3>();

        // Velocity residual
        Vector3 velError = vel - measured_.tail<3>();

        // Combine
        Vector6 error;
        error << posError, velError;

        // Optional Jacobians
        if(H1) {
            // Pose3 tangent is [rx, ry, rz, tx, ty, tz]
            *H1 = Matrix66::Zero();
            // partial wrt translation => last 3 columns
            H1->block<3,3>(0,3) = I_3x3;
        }
        if(H2) {
            // velocity => bottom 3 rows = Identity
            *H2 = Matrix63::Zero();
            H2->block<3,3>(3,0) = I_3x3;
        }
        return error;
    }
};

} // namespace FlySight

#endif // GNSSFACTOR_H
