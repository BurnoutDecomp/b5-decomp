// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGyroCam slices this TU owns:
//   Construct / Prepare / GetCollisionPolicy / GetName  (the declared vtable slots)
//   SetParameters / AttachToRaceCar / SetWorldSpaceNormalizedVectorFromCar
// Every one of them is header-inline (this class is re-based onto Camera::Behaviour and this
// .cpp is not on the build list, so an out-of-line body here would be a vtable slot no link
// could resolve). This .cpp is the translation-unit anchor that pulls the header into the
// compile gate and forces an emission of each. The rig's Update and SetupTweaker slots land
// with the gyro-cam rig TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnDirector
{
namespace Camera
{

// ARTIST 821F9F10 / 8220EE08. The truck carries a fixed heading, a smoothed
// speed and a desired fraction of the subject's velocity along that heading.
void AttachmentTruck::Set(VecFloat lRatio, Vector3 lVelocity, Vector3 lPosition)
{
    using namespace rw::math::vpu;
    mPosition = lPosition;
    mbFirstFrame = false;
    if (IsZero(lVelocity, 1.1920929e-7f))
    {
        mDirection.SetZero();
        mSpeed = VecFloat(0);
        mDesiredSpeedRatio = VecFloat(0);
    }
    else
    {
        mDirection = Normalize(lVelocity);
        const f32 speed = Magnitude(lVelocity);
        mSpeed = VecFloat(speed);
        mDesiredSpeedRatio = lRatio;
    }
}

Vector3 AttachmentTruck::GetVelocity() const
{
    return rw::math::vpu::Mult(mDirection, static_cast<f32>(mSpeed));
}

void AttachmentTruck::Update(Vector3 lPosition, Vector3 lVelocity,
                             VecFloat lTimestep, const Parameters& lrParams)
{
    using namespace rw::math::vpu;
    CGS_ASSERT(std::fabs(lrParams.mfConvergenceTimeSecs) > 1.1920929e-7f,
               "!rw::math::fpu::IsZero(lParams.mfConvergenceTimeSecs)");
    if (mbFirstFrame)
    {
        Set(VecFloat{}, lVelocity, lPosition);
        if (!IsZero(lVelocity, 1.1920929e-7f))
        {
            const f32 speed = static_cast<f32>(mSpeed) - lrParams.mfInitialOffsetDist / lrParams.mfConvergenceTimeSecs;
            const f32 ratio = std::min(1.25f, std::max(0.0f, speed / static_cast<f32>(mSpeed)));
            mSpeed = VecFloat(speed);
            mDesiredSpeedRatio = VecFloat(ratio);
            mPosition = lPosition + mDirection * lrParams.mfInitialOffsetDist;
        }
    }
    const f32 speed = static_cast<f32>(mSpeed) + 0.1f * (Dot(lVelocity, mDirection) * static_cast<f32>(mDesiredSpeedRatio) - static_cast<f32>(mSpeed));
    mSpeed = VecFloat(speed);
    mPosition = mPosition + GetVelocity() * static_cast<f32>(lTimestep);
    mPosition.y = lPosition.y;
}

// ARTIST 82244E40. Camera output is produced on every valid frame, even while
// the arbitrator is blending to this rig or deciding whether it can cut to it.
bool BehaviourGyroCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    using namespace rw::math::vpu;
    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");
    if (!mAttachedTo.IsValid(*lrInfo.GetWorld()))
    {
        Fail(lrCamera, 11);
        return true;
    }
    if (!HasFailed()) lrCamera.mState_uFlags |= 2;
    const VehicleInfo& vehicle = *mAttachedTo.Get(lrInfo.GetWorld());
    Matrix44Affine target = vehicle.mRaceCarState.mTransform;
    if (mbIsPlanted && mbIsFirstFrameOfPlanted)
    {
        mAttachmentTruck.Construct();
        mAttachmentTruck.Set(VecFloat(0.5f), vehicle.mRaceCarState.mLinearVelocity, target.Pos());
        mbIsFirstFrameOfPlanted = false;
    }
    Vector3 velocity = mAttachedTo.meType == BrnDirector::VehicleRef::E_PLAYER_CAR
        ? lrInfo.GetPlayerTracker()->GetImplicitVelocity() : vehicle.mRaceCarState.mLinearVelocity;
    const f32 speed = Magnitude(velocity);
    if (mpParameters->mbUseTruck || mbIsPlanted)
    {
        const f32 dt = lrInfo.GetTimestep(Timestep::E_WORLD_NO_SLOMO);
        mAttachmentTruck.Update(target.Pos(), velocity, VecFloat(dt), mpParameters->mAttachmentTruckParams);
        target.Pos() = mAttachmentTruck.GetPosition();
    }
    const f32 dt = lrInfo.GetTimestep(GetTimestepType());
    mPositionLag.Update(mpParameters->mLagParams, dt, target);
    mVisibilityCollisionPolicy.SetTarget(vehicle.mRaceCarState.mTransform, vehicle.mAABB, CgsSceneManager::EntityId(vehicle.mRaceCarState.mEntityId.muValue));
    mVehicleAttachmentCollisionPolicy.SetVehicleRef(mAttachedTo);
    CGS_ASSERT(std::fabs(mpParameters->mfHeightDistanceVelocityRange) > 1.1920929e-7f,
               "!rw::math::fpu::IsZero(mpParameters->mfHeightDistanceVelocityRange)");
    const f32 ratio = std::min(speed / mpParameters->mfHeightDistanceVelocityRange, 1.0f);
    if (!mbIsWorldSpaceVectorSet)
    {
        Vector3 flat = velocity;
        flat.y = 0;
        mDesiredWorldSpaceNormalizedVectorFromCar = IsZero(flat, 1.1920929e-7f) ? target.zAxis : Normalize(flat);
        if (mpParameters->mbUseSideVector)
            mDesiredWorldSpaceNormalizedVectorFromCar = Normalize(Cross(Vector3{0,1,0,0}, mDesiredWorldSpaceNormalizedVectorFromCar));
        if (mpParameters->mbInvertVector) mDesiredWorldSpaceNormalizedVectorFromCar = -mDesiredWorldSpaceNormalizedVectorFromCar;
        CGS_ASSERT(IsValid(mDesiredWorldSpaceNormalizedVectorFromCar), "IsValid(mDesiredWorldSpaceNormalizedVectorFromCar)");
        mWorldSpaceNormalizedVectorFromCar = mDesiredWorldSpaceNormalizedVectorFromCar;
        mbIsWorldSpaceVectorSet = true;
    }
    const f32 pitch = mpParameters->mfSlowPitch + ratio * (mpParameters->mfFastPitch - mpParameters->mfSlowPitch);
    const f32 height = mpParameters->mfSlowHeight + ratio * (mpParameters->mfFastHeight - mpParameters->mfSlowHeight);
    const f32 distance = mpParameters->mfSlowDistance + ratio * (mpParameters->mfFastDistance - mpParameters->mfSlowDistance);
    if (!IsPrepared())
    {
        mOriginalPoint = mCurrentTargetPos = target.Pos() + Vector3{0,-1,0,0};
        mfPitch = pitch; mfHeight = height; mfDistance = distance;
    }
    const Vector3 previousDesired = mDesiredWorldSpaceNormalizedVectorFromCar;
    if (speed > 1.0f) mDesiredWorldSpaceNormalizedVectorFromCar = Normalize(previousDesired);
    mfPitch += (pitch - mfPitch) * mpParameters->mfHeightDistanceBlendFactor;
    mfHeight += (height - mfHeight) * mpParameters->mfHeightDistanceBlendFactor;
    mfDistance += (distance - mfDistance) * mpParameters->mfHeightDistanceBlendFactor;
    if (mpParameters->mbStickToGround)
    {
        mVisibilityCollisionPolicy.SetDesiredHeight(mfHeight);
        mVehicleAttachmentCollisionPolicy.SetDesiredHeight(mfHeight);
    }
    if (!IsZero(mWorldSpaceNormalizedVectorFromCar - mDesiredWorldSpaceNormalizedVectorFromCar, 1.5258789e-5f))
    {
        const f32 blend = Magnitude(previousDesired) < 5.0f ? mpParameters->mfMinimumBlendFactor : mpParameters->mfMaximumBlendFactor;
        mfBlendFactor += (blend - mfBlendFactor) * mpParameters->mfBlendFactorBlendFactor;
        mWorldSpaceNormalizedVectorFromCar = Normalize(Utils::SafeSLerp(mWorldSpaceNormalizedVectorFromCar,
            mDesiredWorldSpaceNormalizedVectorFromCar, VecFloat(mfBlendFactor)));
    }
    CGS_ASSERT(!IsZero(mWorldSpaceNormalizedVectorFromCar), "!IsZero(mWorldSpaceNormalizedVectorFromCar)");
    CGS_ASSERT(std::fabs(mfDistance) > 1.1920929e-7f, "!rw::math::fpu::IsZero(mfDistance)");
    const Vector3 eye = target.Pos() + mWorldSpaceNormalizedVectorFromCar * mfDistance;
    if (IsPrepared()) mTransform.Pos() = eye;
    else { mTransform = Utils::CreateLookAt(eye, target.Pos()); lrCamera.mTransform = mTransform; }
    if (mpParameters->mbStickToGround) mTransform.Pos().y = lrCamera.mTransform.Pos().y;
    else mTransform.Pos().y += mfHeight;
    lrCamera.mTransform = mTransform;
    mLooker.Update(VecFloat(dt), mRandom, mpParameters->mLookerParams,
                   lrCamera, target, velocity, AABBox{});
    mTransform = lrCamera.mTransform;
    mShake.Update(lrCamera.mTransform, mpParameters->mShakeParams, mRandom, dt, 1.0f);
    lrCamera.SetFOV(mpParameters->mfField_B0);
    lrCamera.mTransform = Mult(MakeRotationX(mfPitch * 0.017453292f), lrCamera.mTransform);
    mVisibilityCollisionPolicy.SetVelocity(mpParameters->mbUseTruck ? mAttachmentTruck.GetVelocity() : velocity);
    if (mbIsPlanted)
    {
        lrCamera.SetRequestedTimeDilation(0.2857142985f);
        if (!HasFailed()) SetCantSwitchFromMeNow(lrCamera, 28);
    }
    if (!HasFailed() && !mbUseVehicleAttachmentCollision)
    {
        if (mVisibilityCollisionPolicy.WillCollideWithGeometry() && mVisibilityCollisionPolicy.TimeUntilCollisionWithGeometry() < 1.0f)
            { lrCamera.mState.mHeadFlags.SetBit(25); mbCanSwitchToMeNow = false; }
        if (mVisibilityCollisionPolicy.WillCollideWithVehicle() && mVisibilityCollisionPolicy.TimeUntilCollisionWithVehicle() < 1.0f)
            { lrCamera.mState.mHeadFlags.SetBit(26); mbCanSwitchToMeNow = false; }
        f32 time = 0;
        if (mpParameters->mbUseTruck && Utils::PointWillLeaveFrustrum(lrCamera.mTransform, vehicle.mRaceCarState.mTransform.Pos(),
                vehicle.mRaceCarState.mLinearVelocity - mAttachmentTruck.GetVelocity(), lrCamera.mfFOV, lrCamera.mfFOV, &time) && time < 1.0f)
            { lrCamera.mState.mHeadFlags.SetBit(15); mbCanSwitchToMeNow = false; }
        if (mVisibilityCollisionPolicy.IsVisibilityInterrupted()) { lrCamera.mState.mHeadFlags.SetBit(16); mbCanSwitchToMeNow = false; }
    }
    SetPrepared();
    // FLAG PC diagnostic: measure produced poses, not just arbitrator state changes.
    static const bool trace = std::getenv("BRN_CAMERA_RIG_DIAG") != 0;
    static u32 lines = 0;
    if (trace && lines++ < 64 && CgsDev::Log::gpDebugPrint)
        *CgsDev::Log::gpDebugPrint << "[camera-rig] gyro car=" << mAttachedTo.miRaceCarIndex
            << " eye=" << lrCamera.mTransform.Pos().x << "," << lrCamera.mTransform.Pos().y << "," << lrCamera.mTransform.Pos().z
            << " target=" << target.Pos().x << "," << target.Pos().y << "," << target.Pos().z
            << " fov=" << lrCamera.mfFOV << " dt=" << dt << "\n";
    return true;
}

// Out-of-line anchors: force the slice functions to be emitted in this TU. The moment/arbitrator
// code adopts a gyro-cam parameter block, attaches the rig to a race car, reads the active
// collision policy, and seeds the world-space from-car vector through these.
void BehaviourGyroCam_SetParametersAnchor(
    BehaviourGyroCam& lrBehaviour,
    const BehaviourGyroCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

void BehaviourGyroCam_AttachToRaceCarAnchor(
    BehaviourGyroCam& lrBehaviour,
    s32 meRaceCarIndex)
{
    lrBehaviour.AttachToRaceCar(meRaceCarIndex);
}

CollisionPolicy* BehaviourGyroCam_GetCollisionPolicyAnchor(
    BehaviourGyroCam& lrBehaviour)
{
    return lrBehaviour.GetCollisionPolicy();
}

void BehaviourGyroCam_SetWorldSpaceNormalizedVectorFromCarAnchor(
    BehaviourGyroCam& lrBehaviour,
    rw::math::vpu::Vector3 lVectorFromCar)
{
    lrBehaviour.SetWorldSpaceNormalizedVectorFromCar(lVectorFromCar);
}

void BehaviourGyroCam_SetUseVehicleAttachmentCollisionAnchor(
    BehaviourGyroCam& lrBehaviour,
    bool lbUseVehicleAttachmentCollision)
{
    lrBehaviour.SetUseVehicleAttachmentCollision(lbUseVehicleAttachmentCollision);
}

} // namespace Camera
} // namespace BrnDirector
