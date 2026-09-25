// GameSource/Director/Camera/Behaviours/BehaviourRig.cpp
//
// BrnDirector::Camera::BehaviourRig -- the "rig" camera behaviour: a camera bolted to a car at an authored
// offset (a CameraRig preset), with an optional acceleration spring, orientation / position lag, a looker that can
// turn it toward a second car, and a depth-of-field band. The takedown look-back and the jump cutaway are rigs.
//
// [FX-DIRECTOR2 2026-09-25] RE-DERIVED FROM THE X360 IN FULL. Every body that stood here was a paraphrase:
// Parameters::Construct seeded invented tunings (a 0.05 acceleration factor, a 10..40 m DOF band, the spring on and
// the shake off, the shake's numbers in the orientation lag's fields, a zero rig block), Construct skipped half the
// members, and Update never composed the camera transform at all -- it looked up the cars and dropped the answers.
// The bodies below follow the console instruction for instruction; each banner carries the walk.
//
// Functions:
//   Parameters::Construct @0x821F9680   Construct @0x82242488   Prepare @0x821F9798   Update @0x822427C0
//   SetupTweaker @0x821F9870            GetName @0x821F99F0     GetCollisionPolicy @0x821FB588 (ICF-shared)
//   AttachToRaceCar / SetDetached       (DWARF BehaviourRig.cpp:415 / :429; the X360 inlines both, the PS3 keeps
//                                        them out of line @0x25054 / @0x15B40)
// The vtable (0x8200A5A0) also points slot 3 at 0x82C296C8 (`li r3, 1`, the base PostCollisionUpdate) and slot 4 at
// 0x8284CB38 (the empty base Release), so neither is overridden here.

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"
#include "GameSource/Director/Camera/BrnCameraState.h"                 // CameraState::E_FLAG_VALID
#include "GameSource/Director/Camera/BrnDepthOfField.h"                // DepthOfField::SetParams (the plane form)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"         // Camera::VehicleInfo (the attached car, by value)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"              // Utils::CreateLookAt
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"            // ConsoleVpu:: the SDK inlines, rounded as the console
#include "rw/math/vpu/matrix44affine_operation.h"                      // Mult / InverseOfMatrixWithOrthonormal3x3 / TransformVector
#include "rw/math/vpu/vector3_operation.h"                             // Vector3 + / - / * and Mult
#include "rw/math/fpu/scalar_operation.h"                              // rw::math::fpu::IsZero / Clamp / Max

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // BehaviourRig::Parameters::Construct @0x821F9680 -- the rig block's seed.
    const f32 KF_DEFAULT_SHAKE_XY_MAGNITUDE_DEGS  = 0.6f;    // flt_82004D00 -> +0x50
    const f32 KF_DEFAULT_SHAKE_Z_MAGNITUDE_DEGS   = 0.0f;    // flt_82001CC0 -> +0x54
    const f32 KF_DEFAULT_SHAKE_XY_WOBBLE_DEGS     = 1.15f;   // flt_820047BC -> +0x58
    const f32 KF_DEFAULT_SHAKE_WOBBLE_CENTERING   = 0.11f;   // flt_820047C0 -> +0x5C
    const f32 KF_DEFAULT_SPRING_ACCEL_FACTOR      = 10.0f;   // flt_82004A20 -> +0xF0
    const f32 KF_DEFAULT_SPRING_MASS              = 1.0f;    // flt_82001C98 -> +0xF4
    const f32 KF_DEFAULT_SPRING_STIFFNESS         = 40.0f;   // flt_82004D0C -> +0xF8
    const f32 KF_DEFAULT_SPRING_DAMPENING         = 5.0f;    // flt_8200426C -> +0xFC
    const f32 KF_DEFAULT_SPRING_MIN_STRETCH       = -1.0f;   // flt_820037C8 -> +0x100
    const f32 KF_DEFAULT_SPRING_MAX_STRETCH       = 1.5f;    // flt_82004D04 -> +0x104
    const f32 KF_DEFAULT_DOF_NEAR                 = 0.0f;    // flt_82001CC0 -> +0x108
    const f32 KF_DEFAULT_DOF_FAR                  = 2000.0f; // flt_82004D08 -> +0x10C
    const f32 KF_DEFAULT_DOF_BLUR_DEPTH           = 10.0f;   // flt_82004A20 -> +0x110
    const f32 KF_DEFAULT_DOF_INTENSITY            = 0.0f;    // flt_82001CC0 -> +0x114

    // BehaviourRig::Update @0x822427C0.
    const f32 KF_TWEAK_FOV_MIN                    = 5.0f;    // flt_8200426C (the tweaker's FOV clamp)
    const f32 KF_TWEAK_FOV_MAX                    = 120.0f;  // flt_82004A28
    const f32 KF_TWEAK_DOF_MIN_BAND               = 0.2f;    // flt_82004744 (far >= near + 0.2)
    const f32 KF_TWEAK_DOF_HALF                   = 0.5f;    // flt_82001DA0 (blur depth <= half the band ...)
    const f32 KF_TWEAK_DOF_BLUR_MARGIN            = 0.05f;   // flt_820047C8 (... less 0.05)
    const f32 KF_TWEAK_DOF_INTENSITY_MAX          = 1.0f;    // flt_82001C98
    const f32 KF_TWEAK_DOF_PLANE_HALF_SIZE        = 8.0f;    // flt_82004C88 (the two DOF planes' half size)
    const s32 KI_NOCUTTO_SUBJECT_LEFT_FRAME       = 16;      // `oris r11, r11, 1` on camera +0x138 (0x822436F4)

    // BehaviourRig::SetupTweaker @0x821F9870.
    const f32 KF_TWEAK_DOF_SCALE                  = 0.05f;   // flt_820047C8 (every mapping's scale)
}

// ============================================================================
// BehaviourRig::Parameters::Construct @0x821F9680 (DWARF BehaviourRig.cpp:35)
//
// The console, store by store (a leaf; no call):
//     +0x00 = 2 (eBehaviourRig), +0x04 = 0 (the debug name)           Behaviour::Parameters::Construct + the tag
//     +0xD4 = 0.05, +0xD8 = 1                                         mOrientationLagParams.Construct (inlined)
//     +0xE0/+0xE4/+0xE8 = 1.0, +0xEC = 0.5                            mPositionLagParams.Construct (inlined)
//     +0x10..+0x4F <- 8 doublewords from 0x82CDA890                   mRigParams = ParamsFrontQuarterClose
//     +0x50..+0x5C = 0.6 / 0.0 / 1.15 / 0.11                          the shake block -- FOUR stores, one per field.
//                                                                     BehaviourBystanderCam's Construct shows this
//                                                                     compiler keeps the dead seed store when a
//                                                                     block is Constructed then re-tuned, so the
//                                                                     rig assigns them rather than calling
//                                                                     CameraShake::Parameters::Construct
//     +0xF0..+0x114                                                   the spring and DOF scalars (see the constants)
//     +0x118..+0x11C = 0, 1, 0, 0, 0                                  UseAccelSpring, UseShake, Reverse,
//                                                                     UseOrientationLag, UsePositionLag
// muVersion (+0x08) and mLookerParams (+0x60..+0xC3) are NOT touched. The PS3 build (0x1AB7C) calls the two lag
// Constructs out of line and names the rig preset.
// ============================================================================
void BehaviourRig::Parameters::Construct()
{
    Behaviour::Parameters::Construct();
    mType = eBehaviourRig;

    mOrientationLagParams.Construct();
    mPositionLagParams.Construct();

    mRigParams = Utils::CameraRig::ParamsFrontQuarterClose;

    mShakeParams.mfXYShakeMagnitudeDegs  = KF_DEFAULT_SHAKE_XY_MAGNITUDE_DEGS;
    mShakeParams.mfZShakeMagnitudeDegs   = KF_DEFAULT_SHAKE_Z_MAGNITUDE_DEGS;
    mShakeParams.mfXYWobbleMagnitudeDegs = KF_DEFAULT_SHAKE_XY_WOBBLE_DEGS;
    mShakeParams.mfWobbleCenteringFactor = KF_DEFAULT_SHAKE_WOBBLE_CENTERING;

    mfSpringAccelFactor = KF_DEFAULT_SPRING_ACCEL_FACTOR;
    mfSpringMass        = KF_DEFAULT_SPRING_MASS;
    mfSpringStiffness   = KF_DEFAULT_SPRING_STIFFNESS;
    mfSpringDampening   = KF_DEFAULT_SPRING_DAMPENING;
    mfSpringMinStretch  = KF_DEFAULT_SPRING_MIN_STRETCH;
    mfSpringMaxStretch  = KF_DEFAULT_SPRING_MAX_STRETCH;
    mfDOFNear           = KF_DEFAULT_DOF_NEAR;
    mfDOFFar            = KF_DEFAULT_DOF_FAR;
    mfDOFBlurDepth      = KF_DEFAULT_DOF_BLUR_DEPTH;
    mfDOFIntensity      = KF_DEFAULT_DOF_INTENSITY;

    mbUseAccelSpring    = false;
    mbUseShake          = true;
    mbReverse           = false;
    mbUseOrientationLag = false;
    mbUsePositionLag    = false;
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
// BehaviourRig::Construct @0x82242488 (DWARF BehaviourRig.cpp:77) -- vtable slot 0
//
// One call (Spring1D::Construct) splits the console's stores into the part written before it and the part after
// it; the order of the calls below is the PS3 build's (0x1E7E4), which keeps every one of them out of line:
//   before the call   the base six stores (+0x04..+0x10); mShake +0x2B0..+0x2BC = 0.0; mLooker +0x3E4 = 0.0,
//                     +0x3F4 = 0.2, +0x400/+0x401/+0x402 = 1, +0x403 = 0; mRandom +0x410 -- the default seed
//                     0xC87CD8C91AD0891B, the 1.0f slot and seven more draws (CgsNumeric::Random::Construct)
//   bl Spring1D::Construct(+0x3C0)
//   after the call    mPositionLag +0x330/+0x331 = 1; mOrientationLag +0x300 = 0, +0x304 = 1; the whole inlined
//                     VisibilityCollisionPolicy::Construct over +0x20; `stb 1, 0x1C0` -- the policy's
//                     SetTestLookingAt(true), re-stored after its own Construct (as BehaviourFixedCam does);
//                     mLookingAtRef +0x45C = 0; mAttachedToRef +0x44C = 0, then SetToPlayer (+0x44C = 1, +0x440 = 0,
//                     +0x448 = 0, +0x444 = -1); the four latches = 0; mpParameters = 0.
// mfLastMPH (+0x464) is NOT written: Update's first frame seeds it.
// ============================================================================
void
BehaviourRig::Construct()
{
    Behaviour::Construct();
    mShake.Construct();
    mLooker.Construct();
    mRandom.Construct();
    mAccelSpring.Construct();
    mPositionLag.Construct();
    mOrientationLag.Construct();
    mCollisionPolicy.Construct();
    mCollisionPolicy.SetTestLookingAt(true);
    mLookingAtRef.Construct();
    mAttachedToRef.Construct();
    mAttachedToRef.SetToPlayer();
    mpParameters  = NULL;
    mbLookingLast = false;
    mbLooking     = false;
    mbDetached    = false;
    mbSnap        = false;
}

// ============================================================================
// BehaviourRig::Prepare @0x821F9798 (DWARF BehaviourRig.cpp:110) -- vtable slot 1
//     stb 0, 8(this)                                   mbIsPrepared = false (Update's first frame sets it)
//     assert "mAttachedToRef.HasBeenSet()"             (:115, lbz 0x44C)
//     assert "mpParameters != NULL"                    (:116)
//     Spring1D::Prepare(+0x3C0, 0.0, 0.0, mass, stiffness, dampening, min, max)   (f1..f7)
//     the inlined OrientationLag::SetParameters(&params->mOrientationLagParams): the store (+0x300), then ITS
//     own "mpParameters != NULL" (BrnOrientationLag.cpp:50)
//     return true
// [FX-DIRECTOR2 2026-09-25] the old body asserted the lag's parameters a second time, under this file's name.
// ============================================================================
bool
BehaviourRig::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();

    CGS_ASSERT(mAttachedToRef.IsValid(), "mAttachedToRef.HasBeenSet()");     // :115
    CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");                // :116

    mAccelSpring.Prepare(0.0f, 0.0f,
                         mpParameters->mfSpringMass,
                         mpParameters->mfSpringStiffness,
                         mpParameters->mfSpringDampening,
                         mpParameters->mfSpringMinStretch,
                         mpParameters->mfSpringMaxStretch);

    mOrientationLag.SetParameters(&mpParameters->mOrientationLagParams);

    return true;
}

// ============================================================================
// BehaviourRig::SetupTweaker @0x821F9870 (DWARF BehaviourRig.cpp:366) -- vtable slot 6
//     Tweaker::Construct(lrTweaker) ; assert "mpParameters" (:372)
//     then four inlined AddMapping(name, &param, 0.05 (flt_820047C8), axis, E_MAP_NORMAL) -- each with only its
//     "lpfVariableToTweak != NULL" tripwire left (the rest fold on constants), writing the [0][axis] slot:
//       +0x14 "DOF Blur Near"  -> mfDOFNear       E_AXIS_LEFT_STICK_Y      (slot 1)
//       +0x3C "DOF Blur Far"   -> mfDOFFar        E_AXIS_RIGHT_STICK_Y     (slot 3)
//       +0x78 "DOF Blur Depth" -> mfDOFBlurDepth  E_AXIS_LOWER_TRIGGERS    (slot 6)
//       +0xA0 "DOF Intensity"  -> mfDOFIntensity  E_AXIS_BUTTONS_LEFT_RIGHT (slot 8)
// The tweaker edits the live parameter block, which is why it takes the address of the const block's fields.
// ============================================================================
void
BehaviourRig::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();

    CGS_ASSERT(mpParameters != NULL, "mpParameters");                        // :372

    Parameters& lrParameters = const_cast<Parameters&>(*mpParameters);
    lrTweaker.AddMapping("DOF Blur Near", &lrParameters.mfDOFNear, KF_TWEAK_DOF_SCALE,
                         Utils::Tweaker::E_AXIS_LEFT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("DOF Blur Far", &lrParameters.mfDOFFar, KF_TWEAK_DOF_SCALE,
                         Utils::Tweaker::E_AXIS_RIGHT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("DOF Blur Depth", &lrParameters.mfDOFBlurDepth, KF_TWEAK_DOF_SCALE,
                         Utils::Tweaker::E_AXIS_LOWER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("DOF Intensity", &lrParameters.mfDOFIntensity, KF_TWEAK_DOF_SCALE,
                         Utils::Tweaker::E_AXIS_BUTTONS_LEFT_RIGHT, Utils::Tweaker::E_MAP_NORMAL);
}

// ============================================================================
// BehaviourRig::GetCollisionPolicy @0x821FB588 -- `addi r3, r3, 0x20 ; blr` (ICF-shared with BehaviourFixedCam's).
// ============================================================================
CollisionPolicy*
BehaviourRig::GetCollisionPolicy()
{
    return &mCollisionPolicy;
}

// ============================================================================
// BehaviourRig::AttachToRaceCar (DWARF BehaviourRig.cpp:415; PS3 @0x25054) -- bind the rig to a race car and
// re-attach: the inlined VehicleRef::SetToRaceCar on mAttachedToRef (+0x440) with its index tripwire, then a
// tail call to SetDetached(false).
// ============================================================================
void
BehaviourRig::AttachToRaceCar(EActiveRaceCarIndex leIndex)
{
    mAttachedToRef.SetToRaceCar(leIndex);
    SetDetached(false);
}

// ============================================================================
// BehaviourRig::SetDetached (DWARF BehaviourRig.cpp:429; PS3 @0x15B40 `stb lbDetached, 1128(this)`). A detached rig
// keeps filming from where the car was (Update's mLastAttachedToTransform) instead of following it.
// ============================================================================
void
BehaviourRig::SetDetached(bool lbDetached)
{
    mbDetached = lbDetached;
}

// ============================================================================
// BehaviourRig::Update @0x822427C0 (DWARF BehaviourRig.cpp:134) -- vtable slot 2
//
//   0x822427E8  assert "mpParameters != NULL" (:136)
//   0x82242820  not failed -> the camera's E_FLAG_VALID (camera +0x140 |= 2)
//   0x82242838  lAttachedTo = a COPY of the attached car's VehicleInfo (VehicleRef::Get, then the copy ctor
//               @0x8221CDC8): its transform rows +0x1F0.., its speed +0x3CC and its box +0x4A0
//   0x822428A4  first frame (!mbIsPrepared): mRig.Construct(the rig preset, the car's box, mbReverse); the rig
//               transform into mLastRigTransform, the car's into mLastAttachedToTransform, its speed into mfLastMPH;
//               then EITHER the tweaker's debug axis + FOV clamp (the rig then stays unprepared and is rebuilt every
//               frame, so tweaks show) OR mbIsPrepared = true
//   0x82242AE4  lfTimestep = the behaviour's timestep flavour (Timestep::Get, its :78 tripwire)
//   0x82242B28  mbUseAccelSpring: accel = IsZero(dt) ? 0 : (speed - mfLastMPH) / dt ; the spring's desired length
//               = -(accel factor * accel) ; Spring1D::Update(dt)
//   0x82242B9C  the working frame: detached -> mLastAttachedToTransform; else the car's transform, its position
//               pushed along the car's forward axis by the spring's length when the spring is on, then kept
//   0x82242C50  mbUseOrientationLag -> OrientationLag::Update + GetTransform (its "!mbFirstFrame" tripwire)
//   0x82242CD0  mbUsePositionLag    -> PositionLag::Update(the params' block, dt, working frame)
//   0x82242D14  mbLooking: the looked-at car's transform and velocity brought into the working frame's space; a
//               SNAP on the first looking frame points mLastRigTransform straight at it (CreateLookAt), otherwise
//               the camera is handed mLastRigTransform and the Looker updates it; the rig frame is mLastRigTransform
//               not looking: mLastRigTransform = the rig's own transform, and that is the rig frame
//   0x82242F90  mbLookingLast = mbLooking ; camera = rig frame x working frame ; ValidateTransformWithDebugInfo
//   0x82243020  the camera's FOV = the rig's (Camera::SetFOV's "lfFOV > 0.0f", Camera.h:424)
//   0x82243050  tweaker: the DOF parameters clamped in place, and the two DOF planes drawn
//   0x82243590  DepthOfField::SetParams(near, far, blur depth, intensity) @0x821F1C20
//   0x822435AC  the collision policy targets the looked-at car when there is one, else the attached car
//   0x822436A8  not failed and the policy's visibility interrupted -> "NoCutTo: Subject left frame" (16)
//   0x82243700  mfLastMPH = the attached car's speed ; return true
//
// ROUNDING (the campaign rule, scratch/CRASHPARITY_0922/ROUNDING_RULE.md): the SDK inlines here round as the console
// does -- the spring push (vmaddfp128 at 0x82242C34, rule 3), the inverse / the looked-at car / its velocity
// (0x82242D20..0x82242E60) and the camera composition (0x82242F90..0x8224300C) as the fused cascades of
// Utils/BrnConsoleVpu.h, the tweaker's blur bound one fmsubs (0x822430C4) and the DOF plane corners with their
// fused "+" / unfused "-" split. The vendor SDK forms round every product and partial sum separately.
// ============================================================================
bool
BehaviourRig::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    using namespace rw::math::vpu;

    CGS_ASSERT(mpParameters != NULL, "mpParameters != NULL");                // :136

    if (!HasFailed())
    {
        lrCamera.GetState().SetFlag(CameraState::E_FLAG_VALID, true);
    }

    const VehicleInfo lAttachedTo(mAttachedToRef.GetVehicle(lrInfo));

    if (!IsPrepared())
    {
        mRig.Construct(mpParameters->mRigParams, lAttachedTo.mAABB, mpParameters->mbReverse);
        mLastRigTransform        = mRig.GetRigTransform();
        mLastAttachedToTransform = lAttachedTo.mRaceCarState.mTransform;
        mfLastMPH                = lAttachedTo.mRaceCarState.mfSpeedMPH;

        if (IsTweakerAttached())
        {
            // Where the preset's target offset lands on the car (the box size scales it, no centre term).
            Matrix44Affine lTargetOffset;
            lTargetOffset.SetIdentity();
            lTargetOffset.Pos() = Mult(mpParameters->mRigParams.mOffsetFromTarget,
                                       lAttachedTo.mAABB.mMax - lAttachedTo.mAABB.mMin);

            CgsDev::DebugInterface lDebug;
            lDebug.GetRender().DrawAxis(Utils::ConsoleVpu::Mult(lTargetOffset, lAttachedTo.mRaceCarState.mTransform));

            // The tweaker edits the live block (the console stores through mpParameters).
            Parameters& lrTweaked = const_cast<Parameters&>(*mpParameters);
            lrTweaked.mRigParams.mfFOV = rw::math::fpu::Clamp(lrTweaked.mRigParams.mfFOV, KF_TWEAK_FOV_MIN,
                                                              KF_TWEAK_FOV_MAX);
        }
        else
        {
            SetPrepared();
        }
    }

    const f32 lfTimestep = lrInfo.GetTimestep(meTimestepType);

    if (mpParameters->mbUseAccelSpring)
    {
        const f32 lfAcceleration = rw::math::fpu::IsZero(lfTimestep)
                                       ? 0.0f
                                       : (lAttachedTo.mRaceCarState.mfSpeedMPH - mfLastMPH) / lfTimestep;
        mAccelSpring.SetDesiredLength(-(mpParameters->mfSpringAccelFactor * lfAcceleration));
        mAccelSpring.Update(lfTimestep);
    }

    Matrix44Affine lWork;
    if (mbDetached)
    {
        lWork = mLastAttachedToTransform;
    }
    else
    {
        lWork = lAttachedTo.mRaceCarState.mTransform;
        if (mpParameters->mbUseAccelSpring)
        {
            lWork.Pos() = Utils::ConsoleVpu::MultiplyAdd(lWork.At(), mAccelSpring.GetLength(), lWork.Pos());
        }
        mLastAttachedToTransform = lWork;
    }

    if (mpParameters->mbUseOrientationLag)
    {
        mOrientationLag.Update(lfTimestep, lWork);
        lWork = mOrientationLag.GetTransform();
    }

    if (mpParameters->mbUsePositionLag)
    {
        mPositionLag.Update(mpParameters->mPositionLagParams, lfTimestep, lWork);
    }

    Matrix44Affine lRig;
    if (mbLooking)
    {
        // The looked-at car, in the working frame's space.
        const Matrix44Affine lInverseWork   = Utils::ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lWork);
        const Matrix44Affine lLookedAtLocal = Utils::ConsoleVpu::Mult(mLookingAtRef.GetTransform(lrInfo), lInverseWork);
        const Vector3        lLookedAtVelocityLocal = Utils::ConsoleVpu::TransformVector(
            lInverseWork, mLookingAtRef.GetVehicle(lrInfo).mRaceCarState.mLinearVelocity);

        if (mbSnap && mbLooking != mbLookingLast)
        {
            mLastRigTransform = Utils::CreateLookAt(mLastRigTransform.Pos(), lLookedAtLocal.Pos());
        }
        else
        {
            AABBox lNoBounds;                                                // zeroed (vspltisw 0), by value
            lNoBounds.mMin.SetZero();
            lNoBounds.mMax.SetZero();
            lrCamera.SetTransform(mLastRigTransform);
            mLooker.Update(VecFloat(lfTimestep), mRandom, mpParameters->mLookerParams, lrCamera, lLookedAtLocal,
                           lLookedAtVelocityLocal, lNoBounds);
        }
        lRig = mLastRigTransform;
    }
    else
    {
        mLastRigTransform = mRig.GetRigTransform();
        lRig = mLastRigTransform;
    }

    mbLookingLast = mbLooking;
    const Matrix44Affine lCameraTransform = Utils::ConsoleVpu::Mult(lRig, lWork);   // held in v124..v127 below
    lrCamera.SetTransform(lCameraTransform);
    lrCamera.ValidateTransformWithDebugInfo();

    lrCamera.SetFOV(mRig.GetFOV());

    if (IsTweakerAttached())
    {
        // The DOF band, clamped in place: near >= 0, far >= near + 0.2, 0 <= blur depth <= half the band less
        // 0.05, 0 <= intensity <= 1. Each is the console's fsel pair (rw::math::fpu Max / Clamp).
        Parameters& lrTweaked = const_cast<Parameters&>(*mpParameters);
        lrTweaked.mfDOFNear      = rw::math::fpu::Max(0.0f, lrTweaked.mfDOFNear);
        lrTweaked.mfDOFFar       = rw::math::fpu::Max(lrTweaked.mfDOFFar, lrTweaked.mfDOFNear + KF_TWEAK_DOF_MIN_BAND);
        lrTweaked.mfDOFBlurDepth = rw::math::fpu::Clamp(
            lrTweaked.mfDOFBlurDepth, 0.0f,
            Utils::ConsoleVpu::MultiplyAdd(lrTweaked.mfDOFFar - lrTweaked.mfDOFNear, KF_TWEAK_DOF_HALF,
                                           -KF_TWEAK_DOF_BLUR_MARGIN));   // fmsubs, one rounding (0x822430C4)
        lrTweaked.mfDOFIntensity = rw::math::fpu::Clamp(lrTweaked.mfDOFIntensity, 0.0f, KF_TWEAK_DOF_INTENSITY_MAX);

        // The two DOF planes, square, facing the camera: the far one first, then the near one. They are placed
        // off the composed transform the console still holds in v124..v127 (not re-read after the validation).
        // Their colour is packed from a function-local static the PS3 build names (lu8Alpha, X360 .data byte
        // 0x82CDAD6C == 7): `rotrwi r7, alpha, 8 ; oris 0xFF` for the far plane, `rotrwi ; ori 0xFF` for the near one.
        // Each corner is (centre +- right * 8) +- up * 8 (flt_82004C88), the centre At * depth + Pos: every "+" a
        // vmaddfp onto the running sum (fused), every "-" a vmulfp128 then a vsubfp (0x82243214..0x822432E8 far,
        // 0x82243484..0x82243540 near).
        static u8 lu8Alpha = 7;
        const Matrix44Affine& lrView = lCameraTransform;
        const Vector3 lRight = lrView.Right() * KF_TWEAK_DOF_PLANE_HALF_SIZE;
        const Vector3 lUp    = lrView.Up() * KF_TWEAK_DOF_PLANE_HALF_SIZE;

        CgsDev::DebugInterface lDebug;
        const Vector3 lFar      = Utils::ConsoleVpu::MultiplyAdd(lrView.At(), lrTweaked.mfDOFFar, lrView.Pos());
        const Vector3 lFarLeft  = lFar - lRight;
        const Vector3 lFarRight = Utils::ConsoleVpu::MultiplyAdd(lrView.Right(), KF_TWEAK_DOF_PLANE_HALF_SIZE, lFar);
        lDebug.GetRender().DrawSolidQuad(
            Utils::ConsoleVpu::MultiplyAdd(lrView.Up(), KF_TWEAK_DOF_PLANE_HALF_SIZE, lFarLeft),
            Utils::ConsoleVpu::MultiplyAdd(lrView.Up(), KF_TWEAK_DOF_PLANE_HALF_SIZE, lFarRight),
            lFarRight - lUp, lFarLeft - lUp, (static_cast<CgsDev::RGBA>(lu8Alpha) << 24) | 0x00FF0000u);
        const Vector3 lNear      = Utils::ConsoleVpu::MultiplyAdd(lrView.At(), lrTweaked.mfDOFNear, lrView.Pos());
        const Vector3 lNearLeft  = lNear - lRight;
        const Vector3 lNearRight = Utils::ConsoleVpu::MultiplyAdd(lrView.Right(), KF_TWEAK_DOF_PLANE_HALF_SIZE, lNear);
        lDebug.GetRender().DrawSolidQuad(
            Utils::ConsoleVpu::MultiplyAdd(lrView.Up(), KF_TWEAK_DOF_PLANE_HALF_SIZE, lNearLeft),
            Utils::ConsoleVpu::MultiplyAdd(lrView.Up(), KF_TWEAK_DOF_PLANE_HALF_SIZE, lNearRight),
            lNearRight - lUp, lNearLeft - lUp, (static_cast<CgsDev::RGBA>(lu8Alpha) << 24) | 0x000000FFu);
    }

    lrCamera.GetDepthOfField().SetParams(mpParameters->mfDOFNear, mpParameters->mfDOFFar,
                                         mpParameters->mfDOFBlurDepth, mpParameters->mfDOFIntensity);

    const Behaviour::VehicleRef& lrTarget = mLookingAtRef.IsValid() ? mLookingAtRef : mAttachedToRef;
    mCollisionPolicy.SetTarget(lrTarget.GetTransform(lrInfo), lrTarget.GetVehicle(lrInfo).mAABB,
                               CgsSceneManager::EntityId(lrTarget.GetVehicle(lrInfo).mRaceCarState.mEntityId.muValue));

    if (!HasFailed() && mCollisionPolicy.IsVisibilityInterrupted())
    {
        SetCantSwitchToMeNow(lrCamera, KI_NOCUTTO_SUBJECT_LEFT_FRAME);
    }

    mfLastMPH = lAttachedTo.mRaceCarState.mfSpeedMPH;
    return true;
}

} // namespace Camera
} // namespace BrnDirector
