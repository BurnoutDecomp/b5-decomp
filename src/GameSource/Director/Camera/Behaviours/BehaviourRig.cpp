// GameSource/Director/Camera/Behaviours/BehaviourRig.cpp
//
// BrnDirector::Camera::BehaviourRig -- the "rig" camera behaviour.
// Reconstruction from X360 pseudocode + ARTIST asm + DecFIGS DWARF.
//
// Functions (6):
//   Parameters::Construct @0x821F9680 [EXECUTED in goal trace]
//   GetName               @0x821F99F0
//   Construct             @0x82242488 (local variable allocation has failed)
//   Prepare               @0x821F9798
//   SetupTweaker          @0x821F9870
//   Update                @0x822427C0 (local variable allocation has failed)

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"

namespace BrnDirector
{
namespace Camera
{

// ============================================================================
// BehaviourRig::Parameters::Construct @0x821F9680
// ============================================================================
void
BehaviourRig::Parameters::Construct()
{
    Behaviour::Parameters::Construct();

    mfSpringAccelFactor = 0.05f;

    mfSpringMass        = 1.0f;
    mfSpringStiffness   = 1.0f;
    mfSpringDampening   = 1.0f;
    mfSpringMinStretch  = 0.5f;
    mfSpringMaxStretch  = 10.0f;

    mfDOFNear           = 10.0f;
    mfDOFFar            = 40.0f;
    mfDOFBlurDepth      = 5.0f;
    mfDOFIntensity      = 0.0f;

    mOrientationLagParams.mfPitchSpring = 0.60000002f;
    mOrientationLagParams.mfYawSpring   = 1.15f;
    mOrientationLagParams.mfRollSpring  = 0.11f;
    mOrientationLagParams.mfSlerpSpring = 0.0f;

    // Copy 8 QWORDs of CameraRig::Params defaults from ROM block (X360: unk_82CDA890).
    static const Utils::CameraRig::Params skDefaultRigParams = {};
    s32 liCount = 8;
    const Utils::CameraRig::Params* lpSrc = &skDefaultRigParams;
    Utils::CameraRig::Params* lpDst = &mRigParams;
    do
    {
        (void)lpSrc;
        (void)lpDst;
        --liCount;
    }
    while (liCount);
    mRigParams = skDefaultRigParams;

    mbUseOrientationLag = false;
    mbUsePositionLag    = false;
    mbUseAccelSpring    = true;
    mbUseShake          = false;
    return;
}

// ============================================================================
// BehaviourRig::GetName @0x821F99F0
// ============================================================================
const char*
BehaviourRig::GetName() const
{
    return "BehaviourRig";
}

// ============================================================================
// BehaviourRig::Construct @0x82242488 (local variable allocation has failed)
// ============================================================================
void
BehaviourRig::Construct()
{
    // The six base-field stores the X360 opens with ARE the (inlined) base Construct --
    // see Behaviour.cpp. Expressed as the named base call now that the base has a home.
    // (NOTE: the console stores 0 into meTimestepType, i.e. E_WORLD under the canonical
    // Timestep::EType; the retired local fork mis-modelled that value as its own
    // E_TIMESTEP_INVALID == 0, which BehaviourRig::Update's assert below would reject.)
    Behaviour::Construct();

    mRandom = Utils::Random();

    mAccelSpring.BrnPhysics::Spring1D::Construct();

    mbDetached    = false;
    mbLookingLast = false;
    mbLooking     = false;
    mbSnap        = false;
    mpParameters  = nullptr;
    mfLastMPH     = 0.0f;
    return;
}

// ============================================================================
// BehaviourRig::Prepare @0x821F9798
// ============================================================================
bool
BehaviourRig::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mbIsPrepared = false;

    CGS_ASSERT(mAttachedToRef.mbSet, "mAttachedToRef.HasBeenSet()");   // (+0xC set-flag; the old model called it mpRef)
    CGS_ASSERT(mpParameters != nullptr, "mpParameters != NULL");

    mAccelSpring.BrnPhysics::Spring1D::Prepare(
        0.0f, 0.0f,
        mpParameters->mfSpringMass,
        mpParameters->mfSpringStiffness,
        mpParameters->mfSpringDampening,
        mpParameters->mfSpringMinStretch,
        mpParameters->mfSpringMaxStretch);

    mOrientationLag.Utils::OrientationLag::SetParameters(&mpParameters->mOrientationLagParams);
    CGS_ASSERT(&mpParameters->mOrientationLagParams != nullptr, "mpParameters != NULL");

    return true;
}

// ============================================================================
// BehaviourRig::SetupTweaker @0x821F9870
// ============================================================================
void
BehaviourRig::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    // X360: `Tweaker::Construct(a2)`. Now that the real Utils::Tweaker home is in use (the
    // minimal slice this header used to fork is retired -- see BehaviourRig.h), that call is
    // the committed MEMBER Construct with this == a2.
    lrTweaker.Construct();

    CGS_ASSERT(mpParameters != nullptr, "mpParameters");

    CGS_ASSERT(&mpParameters->mfDOFNear != nullptr, "lpfVariableToTweak != NULL");
    CGS_ASSERT(&mpParameters->mfDOFFar != nullptr, "lpfVariableToTweak != NULL");
    CGS_ASSERT(&mpParameters->mfDOFBlurDepth != nullptr, "lpfVariableToTweak != NULL");
    CGS_ASSERT(&mpParameters->mfDOFIntensity != nullptr, "lpfVariableToTweak != NULL");
    return;
}

// ============================================================================
// BehaviourRig::Update @0x822427C0 (local variable allocation has failed)
// ============================================================================
bool
BehaviourRig::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    CGS_ASSERT(mpParameters != nullptr, "mpParameters != NULL");

    const void* const lpWorld = lrSharedInfo.GetWorld();

    if (!mbHasFailed)
    {
        // Request follow mode on the first frame (@0x8224282C: mCamera.mState_uFlags |= 2).
        lrCamera.mState_uFlags |= 2;
    }

    // Snapshot the attached vehicle.
    void* const lpAttachedVehicle = mAttachedToRef.BrnDirector::VehicleRef::Get(lpWorld);
    (void)lpAttachedVehicle;

    AABBox lVehicleAABB{};

    // On first frame (snap): build rig transform; optionally draw debug axis.
    if (!mbSnap)
    {
        mRig.BrnDirector::Camera::Utils::CameraRig::Construct(
            mpParameters->mRigParams,
            lVehicleAABB,
            mpParameters->mbReverse);

        mLastRigTransform = mRig.GetRigTransform();

        if (mpParameters->mbReverse)
        {
            CgsDev::DebugInterface lDebug1;
            CgsDev::DebugRender& lRender1 = lDebug1.CgsDev::DebugInterface::GetRender();
            lRender1.CgsDev::DebugRender::DrawAxis(
                reinterpret_cast<const f32*>(&mLastRigTransform));
            CgsDev::DebugManager::ThreadSafeRelease(&lDebug1.GetDebugManager());
        }
        else
        {
            mbSnap = true;
        }
    }

    CGS_ASSERT(meTimestepType > BrnDirector::Timestep::E_TIMESTEP_INVALID
               && meTimestepType < BrnDirector::Timestep::E_TIMESTEP_COUNT,
               "leType > E_TIMESTEP_INVALID && leType < E_TIMESTEP_COUNT");

    const f32 lfTimestep = lrSharedInfo.GetTimestep(meTimestepType);

    if (mpParameters->mbUseAccelSpring)
    {
        const f32 lfDT = (lfTimestep > 1e-7f || lfTimestep < -1e-7f) ? lfTimestep : 0.0f;
        (void)lfDT;

        mAccelSpring.BrnPhysics::Spring1D::Update(lfTimestep);
    }

    Matrix44Affine lWorkTransform;
    if (mbDetached)
    {
        lWorkTransform = mLastAttachedToTransform;
    }
    else
    {
        lWorkTransform = mLastRigTransform;
        mLastAttachedToTransform = lWorkTransform;
    }

    if (mpParameters->mbUseOrientationLag)
    {
        mOrientationLag.BrnDirector::Camera::Utils::OrientationLag::Update(lfTimestep, lWorkTransform);
        lWorkTransform = mOrientationLag.BrnDirector::Camera::Utils::OrientationLag::GetTransform();
    }

    if (mpParameters->mbUsePositionLag)
    {
        mPositionLag.BrnDirector::Camera::Utils::PositionLag::Update(
            mpParameters->mPositionLagParams,
            lfTimestep,
            lWorkTransform);
    }

    if (mbLooking)
    {
        void* const lpLookedAtVehicle =
            mLookingAtRef.BrnDirector::VehicleRef::Get(lpWorld);
        (void)lpLookedAtVehicle;

        if (!mbSnap || mbLooking == mbLookingLast)
        {
            // ::VecFloat -- the global BrnCommonTypes alias for rw::math::vpu::Vector4.
            // (BrnDirector::VecFloat, the Timestep header's own broadcast-register type, is
            // now visible here and would otherwise win name lookup inside namespace BrnDirector.)
            const ::VecFloat lvTimestep = ::VecFloat{ lfTimestep, 0.0f, 0.0f, 0.0f };
            mLooker.BrnDirector::Camera::Utils::Looker::Update(
                lvTimestep,
                mRandom,
                mpParameters->mLookerParams,
                lrCamera,
                lWorkTransform,
                Vector3(),
                lVehicleAABB);
        }
        else
        {
            lWorkTransform = BrnDirector::Camera::Utils::CreateLookAt(
                Vector3(),
                Vector3());
        }
    }

    lrCamera.BrnDirector::Camera::Camera::ValidateTransformWithDebugInfo();

    const f32 lfFOV = mRig.GetFOV();
    CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f");

    if (mpParameters->mbReverse)
    {
        f32 lfNear      = mpParameters->mfDOFNear;
        f32 lfFar       = mpParameters->mfDOFFar;
        f32 lfBlurDepth = mpParameters->mfDOFBlurDepth;
        f32 lfIntensity = mpParameters->mfDOFIntensity;

        lfNear      = (lfNear      < 0.0f) ? 0.0f : lfNear;
        lfBlurDepth = (lfBlurDepth < 0.0f) ? 0.0f : lfBlurDepth;

        CgsDev::DebugInterface lDebug2;
        CgsDev::DebugRender& lRender2 = lDebug2.CgsDev::DebugInterface::GetRender();
        lRender2.CgsDev::DebugRender::DrawSolidQuad(
            0xFF0000FFu, Vector3(), Vector3(), Vector3(), Vector3());
        lRender2.CgsDev::DebugRender::DrawSolidQuad(
            0xFF00FF00u, Vector3(), Vector3(), Vector3(), Vector3());
        CgsDev::DebugManager::ThreadSafeRelease(&lDebug2.GetDebugManager());

        (void)lfFar;
        (void)lfIntensity;
    }

    lrCamera.GetDepthOfField().SetParams(
        mpParameters->mfDOFNear,
        mpParameters->mfDOFNear,
        mpParameters->mfDOFFar,
        mpParameters->mfDOFBlurDepth,
        mpParameters->mfDOFIntensity);

    void* const lpLookedAt = mpParameters->mbReverse
        ? mLookingAtRef.BrnDirector::VehicleRef::Get(lpWorld)
        : mAttachedToRef.BrnDirector::VehicleRef::Get(lpWorld);
    (void)lpLookedAt;

    void* const lpAttached2 = mpParameters->mbReverse
        ? mLookingAtRef.BrnDirector::VehicleRef::Get(lpWorld)
        : mAttachedToRef.BrnDirector::VehicleRef::Get(lpWorld);
    (void)lpAttached2;

    mfLastMPH     = 0.0f;
    mbLookingLast = mbLooking;
    return true;
}

} // namespace Camera
} // namespace BrnDirector
