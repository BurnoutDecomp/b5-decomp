// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.cpp
//
// BrnDirector::Camera::BehaviourHeliCam -- the helicopter camera's Behaviour overrides.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT
#include "GameSource/Director/Camera/Camera.h"                        // Camera::SetFOV / ValidateTransformWithDebugInfo
#include "GameSource/Director/Camera/BrnCameraState.h"                // CameraState::E_FLAG_VALID
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"        // VehicleInfo (the player car)
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"        // Utils::Tweaker::Construct
#include "rw/math/vpu/vector3_operation.h"                            // Normalize / the Vector3 operators

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The authored height and start distances are in kilometres.
    const f32 KF_METRES_PER_KILOMETRE = 1000.0f;

    // The field of view the shot opens on.
    const f32 KF_START_FOV_DEGS = 50.0f;

    // The scale on the shake angle.
    const f32 KF_SHAKE_SCALE = 1.0f;
}

void BehaviourHeliCam::Construct()
{
    Behaviour::Construct();
    mCameraShake.Construct();
    mLooker.Construct();
    mRandom.Construct();
}

bool BehaviourHeliCam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mTransform.SetIdentity();
    mbCalculatePosition = true;
    SetPrepared();
    return true;
}

// ----------------------------------------------------------------------------
// Update. On the first frame after Prepare the camera is placed above and behind-left of the
// player car (the authored height and the two start distances along the car's right and forward
// axes), aimed to fly level along the car's right+forward diagonal at the authored speed, and the
// FOV is opened. Every frame the position advances along the velocity, the looker turns the rig
// onto the car, and the shake is applied to the produced camera only (the rig keeps the
// unshaken look).
// ----------------------------------------------------------------------------
bool BehaviourHeliCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    CGS_ASSERT(mpParameters, "mpParameters");

    lrCamera.mState_uFlags |= (1 << CameraState::E_FLAG_VALID);

    const VehicleInfo&    lrPlayer          = lrInfo.mPlayerInfo;
    const Matrix44Affine& lrPlayerTransform = lrPlayer.mRaceCarState.mTransform;
    const Vector3         lPlayerPosition   = lrPlayerTransform.wAxis;

    if (mbCalculatePosition)
    {
        const f32 lfDistanceX = mpParameters->mfInitialDistanceX * KF_METRES_PER_KILOMETRE;
        const f32 lfHeight    = mpParameters->mfHeight * KF_METRES_PER_KILOMETRE;
        const f32 lfDistanceZ = mpParameters->mfInitialDistanceZ * KF_METRES_PER_KILOMETRE;

        const Vector3 lHeightOffset = { 0.0f, lfHeight, 0.0f, 0.0f };
        mPosition = ((lPlayerPosition + lHeightOffset) - lrPlayerTransform.xAxis * lfDistanceX)
                  - lrPlayerTransform.zAxis * lfDistanceZ;

        mVelocity   = lrPlayerTransform.xAxis + lrPlayerTransform.zAxis;
        mVelocity.y = 0.0f;
        mTarget     = lPlayerPosition;
        mVelocity   = rw::math::vpu::Normalize(mVelocity) * mpParameters->mfVelocityMPS;

        lrCamera.SetFOV(KF_START_FOV_DEGS);
        mbCalculatePosition = false;
    }

    const f32 lfTimestep = lrInfo.GetTimestep(GetTimestepType());

    mPosition         = mVelocity * lrInfo.GetTimestep(GetTimestepType()) + mPosition;
    mTransform.wAxis  = mPosition;
    lrCamera.mTransform = mTransform;
    lrCamera.ValidateTransformWithDebugInfo();

    const AABBox lNoBounds = {};
    mLooker.Update(VecFloat(lfTimestep), mRandom, mpParameters->mLookerParams, lrCamera,
                   lrPlayerTransform, lrPlayer.mRaceCarState.mLinearVelocity, lNoBounds);
    mTransform = lrCamera.mTransform;

    Matrix44Affine lShakenTransform = mTransform;
    mCameraShake.Update(lShakenTransform, mpParameters->mShakeParams, mRandom, lfTimestep,
                        KF_SHAKE_SCALE);
    lrCamera.mTransform = lShakenTransform;
    lrCamera.ValidateTransformWithDebugInfo();

    return true;
}

void BehaviourHeliCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

const char* BehaviourHeliCam::GetName() const
{
    return "BehaviourHeliCam";
}

} // namespace Camera
} // namespace BrnDirector
