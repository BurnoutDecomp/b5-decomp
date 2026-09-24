// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourAftertouchCrash: all six of its vtable
// overrides (Construct, Prepare, Update, GetCollisionPolicy, SetupTweaker, GetName), the private
// bounce detector CheckForPlayerCarBouncing, and the class-scope tunables. The
// Parameters::Serialise<S> field-walk lives in the sibling *Parameters.cpp, out of the link.
// SetParameters and the small setters are defined inline in the header.
//
// SetParameters is adopted by the crash-mode / takedown arbitrator states and the arbitrator
// testbed when they install an aftertouch-crash parameter block. ArbStateCrashMode::Update copies
// this behaviour's produced camera every frame, so Update below IS the Showtime camera.
//
// ⭐ 2026-09-24 (FX-CAMRIG, crash-parity defect CC-4): Update @0x82228158 and
// CheckForPlayerCarBouncing @0x8220F340 are transcribed. Until then the class kept the base
// Update (`return true`, camera untouched), so crash mode and the takedown camera published a
// camera nothing had written: no follow, no orbit, no bounce shake, no close-up.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"

#include "GameSource/Director/Camera/Camera.h"                  // Camera (Update publishes into it)
#include "GameSource/Director/Camera/BrnCameraState.h"          // CameraState::E_FLAG_VALID
#include "GameSource/Director/Camera/Utils/CameraUtils.h"       // Utils::CreateLookAt / Utils::SineLerp
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"  // Utils::Tweaker::Construct (slot 6)
#include "rw/math/fpu/scalar_operation.h"                       // fpu::Clamp / Min / Abs / IsZero --
                                                                //   the console's fsel / fabs forms
#include "rw/math/vpu/vector2_operation.h"                      // Dot(Vector2, Vector2)
#include "rw/math/vpu/vector3_operation.h"                      // Normalize / Negate / Magnitude(Sq) ...
#include "rw/math/vpu/vector4_operation.h"                      // Splat (the VecFloat broadcast)
#include "rw/math/vpu/matrix44affine_operation.h"               // MakeRotationX/Y/Z, Mult,
                                                                //   TransformVector, SLerp
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint (the
                                                                //   BRN_CAMRIG_DIAG witness only)

#include <cmath>                                                // std::sqrt / std::fabs / std::signbit
#include <cstdlib>                                              // getenv (BRN_CAMRIG_DIAG)

namespace
{
    // The console's vector IsZero, inlined at every Update site: the three lanes' magnitudes
    // (vandc against the sign mask) are compared `> tolerance` by ONE vcmpgtfp. over the lanes
    // (x, y, z, x) -- vrlimi128 copies x into w -- and CR6's "no lane true" bit is read back
    // (`extrwi r, r, 1, 26`). So a NaN lane counts as ZERO here. The vendor vpu::IsZero spells
    // `fabs <= tolerance` (a NaN lane is NOT zero), which is why it is not used. Same form as the
    // AI lane's IsZeroVmx (FX-AINAN2).
    bool IsZeroVmx(const Vector3& lrVector, f32 lfTolerance)
    {
        return !(std::fabs(lrVector.x) > lfTolerance
                 || std::fabs(lrVector.y) > lfTolerance
                 || std::fabs(lrVector.z) > lfTolerance);
    }

    // rw::math::vpu Min / Max / Clamp on a VecFloat, one broadcast lane (rwmath 1.02.00
    // vpu/detail/scalar_operation_inline.h:89-120, Clamp = Min(max, Max(min, value)) :189-192):
    // `vminfp` / `vmaxfp`, which return a NaN when EITHER operand is NaN (vA's when both are) and
    // order the zeros -0 < +0 (AltiVec PEM). NOT the fsel rw::math::fpu forms. The same helpers
    // BrnBehaviourGameplayExternal.cpp carries for its own VecFloat sites.
    f32 VecFloatMax(f32 lfA, f32 lfB)
    {
        if (lfA != lfA) return lfA;
        if (lfB != lfB) return lfB;
        if (lfA == lfB) return std::signbit(lfA) ? lfB : lfA;   // +0 is the larger zero
        return (lfA > lfB) ? lfA : lfB;
    }

    f32 VecFloatMin(f32 lfA, f32 lfB)
    {
        if (lfA != lfA) return lfA;
        if (lfB != lfB) return lfB;
        if (lfA == lfB) return std::signbit(lfA) ? lfA : lfB;   // -0 is the smaller zero
        return (lfA < lfB) ? lfA : lfB;
    }

    f32 VecFloatClamp(f32 lfValue, f32 lfMin, f32 lfMax)
    {
        return VecFloatMin(lfMax, VecFloatMax(lfMin, lfValue));
    }

    // rw::math::fpu::SMALL_FLOAT (flt_82001740 == 0x37800000 == 2^-16), the tolerance of the
    // "has the camera reached its desired direction" test (PS3 names it; X360 0x82228AB4).
    const f32 KF_SMALL_FLOAT = 1.52587890625e-05f;

    // CgsCore::K_V3D_YAXIS @0x83017FA0 -- a .bss constant the CRT thunk @0x82C6CDC0 fills with
    // (0.0f, 1.0f, 0.0f, 0): flt_82001CC0 / flt_82001C98 / flt_82001CC0 / `li r9, 0`. It has no
    // home in this tree; the value is the thunk's.
    const Vector3 K_V3D_YAXIS = { 0.0f, 1.0f, 0.0f, 0.0f };

    // flt_82001744 (0x3C8EFA35) -- degrees to radians, the rig's pitch conversion.
    const f32 KF_DEGS_TO_RADS = 0.017453292f;
}

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// The class-scope tunables (DWARF BrnBehaviourAftertouchCrash.cpp:22..:38). Every value is read
// out of the image, not chosen:
//   * the eleven scalars are a contiguous .data run at 0x82CDA718, in declaration order -- each
//     one's ROLE is independently pinned by its reader in Update / CheckForPlayerCarBouncing
//     (quoted per line);
//   * the five VecFloats are .bss splats written at startup by the CRT thunks
//     0x82C494B8 .. 0x82C49558 (`lfs f0, <rodata>; stfs; lvlx; vspltw 0; stvx128`), which run in
//     declaration order; each role is again pinned by its reader.
// KF_CRASHBREAKER_SHAKE_MAGNITUDE and the two KF_CRASHBREAKER_BLEND_OUT_* have no reader in this
// build's Update (their slots sit between the ones Update reads); they are defined with the
// image's values because the DWARF declares them.
// ----------------------------------------------------------------------------
f32 BehaviourAftertouchCrash::KF_CAMERA_X_ROTATION_SPEED       = -0.05f;  // 0x82CDA718 (orbit yaw per stick x)
f32 BehaviourAftertouchCrash::KF_CAMERA_Y_ROTATION_SPEED       = 0.08f;   // 0x82CDA71C (height per stick y)
f32 BehaviourAftertouchCrash::KF_CAMERA_RESET_SPEED            = 0.99f;   // 0x82CDA720 (height tweak decay)
f32 BehaviourAftertouchCrash::KF_TIME_UNTIL_CAMERA_RESET       = 3.0f;    // 0x82CDA724
f32 BehaviourAftertouchCrash::KF_CAMERA_RESET_MIN_SPEED        = 3.0f;    // 0x82CDA728
f32 BehaviourAftertouchCrash::KF_CRASHBREAKER_SHAKE_MAGNITUDE  = 1.0f;    // 0x82CDA72C
f32 BehaviourAftertouchCrash::KF_BOUNCE_SHAKE_MAGNITUDE        = 0.3f;    // 0x82CDA730
f32 BehaviourAftertouchCrash::KF_MIN_SPEED_FOR_CAMERA_BOUNCE   = 2.3f;    // 0x82CDA734
f32 BehaviourAftertouchCrash::KF_CRASHBREAKER_BLEND_OUT_SIM_SPEED = 0.33f; // 0x82CDA738
f32 BehaviourAftertouchCrash::KF_RECIPROCAL_CRASHBREAKER_BLEND_OUT_SIM_SPEED_BLEND_TIME = 10.0f; // 0x82CDA73C
f32 BehaviourAftertouchCrash::KF_LOOK_DOWN_SCALE               = 0.6f;    // 0x82CDA740

::VecFloat BehaviourAftertouchCrash::KF_MIN_CAMERA_LOOK_TAN_SQ_ANGLE   = { 0.1f,  0.1f,  0.1f,  0.1f  }; // 0x82FAA960 <- flt_82004014 (thunk 0x82C494B8)
::VecFloat BehaviourAftertouchCrash::KF_MAX_CAMERA_LOOK_TAN_SQ_ANGLE   = { 0.6f,  0.6f,  0.6f,  0.6f  }; // 0x82FAA8F0 <- flt_82004D00 (thunk 0x82C494E0)
::VecFloat BehaviourAftertouchCrash::KF_SMOOTHING_FACTOR               = { 0.5f,  0.5f,  0.5f,  0.5f  }; // 0x82FAA7F0 <- flt_82001DA0 (thunk 0x82C49508)
::VecFloat BehaviourAftertouchCrash::KF_SMOOTHING_STOP_DISTANCE_SQ     = { 25.0f, 25.0f, 25.0f, 25.0f }; // 0x82FAA9C0 <- flt_82004FD8 (thunk 0x82C49530)
::VecFloat BehaviourAftertouchCrash::KF_MIN_CAMERA_MANUAL_HEIGHT_TWEAK = { 2.0f,  2.0f,  2.0f,  2.0f  }; // 0x82FAA710 <- flt_82001D9C (thunk 0x82C49558)

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Construct @0x822461F8 -- seed a freshly pooled instance.
//
//   li     r5, 0 / li r4, 0
//   addi   r3,  r6, 0x60      ; &mCollisionPolicy
//   stb    r5,  +0x008(r6)    ; \
//   stb    r5,  +0x009(r6)    ;  |
//   stb    r5,  +0x00A(r6)    ;  |- the base head: these seven stores ARE Behaviour::Construct,
//   stb    r5,  +0x00B(r6)    ;  |  inlined (meTimestepType, the five flag bytes and the debug
//   stb    r5,  +0x00C(r6)    ;  |  parameters name, all zero)
//   stw    r5,  +0x004(r6)    ;  |
//   stw    r5,  +0x010(r6)    ; /
//   bl     CollisionPolicyAttachedToVehicle::Construct   ; with r4 == 0
//   stb    r5,  +0x2A8(r6)    ; policy +0x248 mbAutoElevate         = false
//   stb    r9,  +0x2A9(r6)    ; policy +0x249 mbSmoothRadiusChanges = true   (r9 == 1)
//   stb    r9,  +0x2AC(r6)    ; policy +0x24C mbTestAgainstWorldOnly = true
//   stb    r9,  +0x2AD(r6)    ; policy +0x24D mbUseFrustrumResolver  = true
//   stb    r9,  +0x320(r6) / +0x321(r6)      ; mPositionLag: PositionLag::Construct (both flags)
//   std/stw/stw +0x330..    ; mRandom: Random::Construct -- the default seed 0xC87CD8C91AD0891B
//                             (lis/ori/insrdi @0x82246234..0x8224624C), index 0, slot 0 = 1.0f,
//                             seven AddRandomFloatToBuffer steps, then index = (index + 1) & 7
//   stfs   f0(0.0), +0x360..+0x36C(r6)       ; mShake: CameraShake::Construct
//   stfs   f0(0.0), +0x374..+0x380, +0x370   ; mImpactEffect: CameraImpactEffect::Construct
//   stvx128 v0,  r6, 0x2F0    ; mCameraPositionLastFrame = zero (v0 == vspltisw 0)
//   stb    r5,  +0x3C0(r6) .. stb r5, +0x3C4(r6)   ; the five flag bytes
//   stvx128 v13, r6, 0x3B0    ; mfManualHeightAdjustment = splat(0.0f)
//   stfs   f13, +0x3CC(r6)    ; mfBounceShakeMultiplier = 1.0f
//   stfs   f0,  +0x3D0(r6)    ; mfRollAngleRads     = 0.0f
//   stfs   f0,  +0x3D4(r6)    ; mfCloseupAmount0To1 = 0.0f
//
// The read-only constants were read out of the flat image, not guessed: f0 is 0.0f and f13 is
// 1.0f (big-endian 00000000 / 3f800000 at flt_82001CC0 / flt_82001C98).
//
// NOT written here, faithfully: mfDebugCrashCameraParam0to1 (Prepare seeds it) and mpParameters
// (SetParameters adopts it, and Prepare asserts it was adopted). mPositionLag's two carried
// vectors are not written either -- its first-frame flag makes its own Update seed them.
// ----------------------------------------------------------------------------
void BehaviourAftertouchCrash::Construct()
{
    Behaviour::Construct();

    // The embedded vehicle-attached policy, then the four authored flag overrides the console
    // applies to it immediately after its own Construct returns -- by named setter, not by
    // offset (the same shape the rotate-about-vehicle behaviour uses on the same four bools).
    mCollisionPolicy.Construct(false);                  // bl ..., r4 == 0
    mCollisionPolicy.SetAutoElevate(false);             // policy +0x248
    mCollisionPolicy.SetSmoothRadiusChanges(true);      // policy +0x249
    mCollisionPolicy.SetTestAgainstWorldOnly(true);     // policy +0x24C
    mCollisionPolicy.SetUseFrustrumResolver(true);      // policy +0x24D

    // The four rig sub-objects, each through its own (console-inlined) Construct.
    mPositionLag.Construct();                           // +0x320 / +0x321 = 1
    mRandom.Construct();                                // +0x330 .. +0x35F
    mShake.Construct();                                 // +0x360 .. +0x36C = 0
    mImpactEffect.Construct();                          // +0x370 .. +0x380 = 0

    mCameraPositionLastFrame.SetZero();                 // stvx128 of a zero vector

    mbManualCameraControl               = false;        // stb 0, +0x3C0
    mbWasFallingDownwards               = false;        // stb 0, +0x3C1
    mbDisableCollision                  = false;        // stb 0, +0x3C2
    mbIsTempDebugCrashCamera            = false;        // stb 0, +0x3C3
    mbIsRandomStartTempDebugCrashCamera = false;        // stb 0, +0x3C4

    mfManualHeightAdjustment = rw::math::vpu::Splat(0.0f);  // stvx128 v13 (vspltw of 0.0f), +0x3B0

    mfBounceShakeMultiplier = 1.0f;                     // stfs +0x3CC
    mfRollAngleRads         = 0.0f;                     // stfs +0x3D0
    mfCloseupAmount0To1     = 0.0f;                     // stfs +0x3D4
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Prepare
//   li     r11, 0
//   lwz    r10, +0x3D8(r31)   ; mpParameters
//   stb    r11, +0x008(r31)   ; SetNotPrepared()
//   cmplwi r10, 0
//   bne    ...                ; assert mpParameters != NULL
//   lwz    r11, +0x3D8(r31)
//   li     r3,  1             ; the return value: it cannot fail
//   lfs    f13, +0x48(r11)    ; mpParameters->mfMinimumBlendFactor
//   stfs   f13, +0x38C(r31)   ; mfBlendFactor
//   lfs    f13, +0x38(r11)    ; mpParameters->mfFastHeight
//   stfs   f13, +0x384(r31)   ; mfHeight
//   lfs    f13, +0x34(r11)    ; mpParameters->mfFastDistance
//   lfs    f0,  <rodata>      ; 0.5f
//   stfs   f13, +0x388(r31)   ; mfDistance
//   stfs   f0,  +0x3C8(r31)   ; mfDebugCrashCameraParam0to1
//
// The shared prepare/release info block is not read: this behaviour seeds entirely from its own
// adopted parameter block. Dropping the prepared latch here is the same first-frame idiom the
// road-runner and interpolate behaviours use -- Update re-seeds the rig on the frame after.
// ----------------------------------------------------------------------------
bool BehaviourAftertouchCrash::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();                                       // stb 0, +0x08

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");

    mfBlendFactor = mpParameters->mfMinimumBlendFactor;     // +0x48 -> +0x38C
    mfHeight      = mpParameters->mfFastHeight;             // +0x38 -> +0x384
    mfDistance    = mpParameters->mfFastDistance;           // +0x34 -> +0x388

    // The authored midpoint the debug crash camera restarts from (a 0.5f in read-only data).
    mfDebugCrashCameraParam0to1 = 0.5f;                     // stfs +0x3C8

    return true;                                            // li r3, 1
}

// ============================================================================
// BehaviourAftertouchCrash::Update @0x82228158 (1303 insns; DWARF .cpp:161..:457)
//   PS3 twin @0x5C474 -- its line table names every inlined helper quoted below.
//
// ARGUMENTS: r3 this (r31), r4 lrCamera (spilled to 0x2BC(r1), reloaded as r24), r5
// lrSharedInfo (r26). Returns `li r3, 1` on its only exit (0x82229594).
//
// THE SHARED-INFO READS, pinned by offset (see Behaviour.h's BehaviourSharedInfo):
//   +0x000  mRotationController.mStickVector     (the "is the stick held" test)
//   +0x030  mSphericalRotationController.mStickVector (orbit x / height y / debug adjust y)
//   +0x250  mPlayerInfo.mRaceCarState.mTransform (== 96 + 0x1F0)
//   +0x390  mPlayerInfo.mRaceCarState.mLinearVelocity (== 96 + 816)
//   +0x580 / +0x584 / +0x588  mTimestep.mafTimestep[E_WORLD / E_WORLD_NO_SLOMO / E_GAME]
//   +0x5D4  mpRandom
//
// VMX->portable, the standing convention of this tree's rw::math::vpu home: the console's
// vrsqrtefp + two Newton steps (Normalize / Magnitude) and its three inlined XMVectorSinCos
// minimax polynomials (range register unk_82000C60 == {pi, 2pi, 1/pi, 1/2pi}, coefficient blocks
// unk_82000BD0..0x82000C2F) are the exact std::sqrt / std::sin / std::cos forms, via the vendor
// Normalize / Magnitude / MakeRotation{X,Z} / Mult. Every branch, compare polarity, operand order,
// constant and store is transcribed; fused multiply-adds become separate operations.
// ============================================================================
bool BehaviourAftertouchCrash::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    // DWARF: Update(Camera&, const BehaviourSharedInfo&)::sfDeadzone / ::sfAdjustmentRate --
    // function statics in .data (0x82CDAD3C = 0.2f, 0x82CDAD38 = 1.0f), the debug crash
    // camera's stick dead zone and its param-per-second rate.
    static f32 sfDeadzone       = 0.2f;
    static f32 sfAdjustmentRate = 1.0f;

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");                       // .cpp:163

    const Parameters&                        lrParameters = *mpParameters;
    const BrnPhysics::Vehicle::RaceCarState& lrCarState   = lrSharedInfo.mPlayerInfo.mRaceCarState;
    const f32 lfTimestep = lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_WORLD);   // lfs +0x580

    // .cpp:172 -- the car's speed and direction of travel from ONE rsqrt pipeline
    // (0x822281E8..0x82228270: `vmulfp128 v0, v0, v13` the length, vsel-guarded to 0 for a zero
    // velocity; `vmulfp128 v126, v12, v13` the direction, which is read only when the velocity
    // is not IsZero).
    Vector3 lVelocityDirection;
    const f32 lfSpeed = rw::math::vpu::NormalizeReturnMagnitude(lrCarState.mLinearVelocity,
                                                                lVelocityDirection);

    // .cpp:165..:176 -- lag a copy of the car's frame (the four stvx128 to var_200..var_1D0,
    // then `bl PositionLag::Update` r3 = &mPositionLag, r4 = &mpParameters->mLagParams,
    // f1 = the E_WORLD timestep, r6 = the copy). v123 = its translation row afterwards.
    Matrix44Affine lCarTransform = lrCarState.mTransform;
    mPositionLag.Update(lrParameters.mLagParams, lfTimestep, lCarTransform);
    const Vector3 lCarPosition = lCarTransform.wAxis;

    const bool lbDiagFirstFrame = !IsPrepared();   // [DIAG] BRN_CAMRIG_DIAG only (see the tail)

    // .cpp:179..:207 -- the first frame after Prepare seeds the rig (lbz +0x08; bne 0x822285B0).
    if (!IsPrepared())
    {
        if (mbIsTempDebugCrashCamera)                                            // lbz +0x3C3
        {
            // .cpp:179 -- kick the impact shake once (lfs f1, KF_BOUNCE_SHAKE_MAGNITUDE).
            mImpactEffect.RegisterImpact(KF_BOUNCE_SHAKE_MAGNITUDE);

            if (mbIsRandomStartTempDebugCrashCamera)                             // lbz +0x3C4
            {
                // .cpp:184 -- start from a random side: the "at" row of a Y rotation by one
                // [0, 1) draw. The draw is Random::RandomFloat inlined (0x82228328..0x8222839C:
                // read ring slot, subtract 1.0, refill the slot from the OLD seed, step the LCG
                // by 0x5851F42D4C957F2D, bump the index & 7); the SinCos of the draw lands as
                // (sin, 0, cos) through the perm unk_82CDA350 + vrlimi128 (0x822284DC/E0), i.e.
                // the SDK's MakeRotationY (DecFIGS matrix44affine_operation_platform_inline.h
                // :255) row z.
                mManualCameraDirection =
                    rw::math::vpu::MakeRotationY(lrSharedInfo.mpRandom->RandomFloat()).zAxis;
            }
        }

        // IsZero(velocity) against EPSILON (flt_82001770), 0x822284E8..0x82228520.
        if (IsZeroVmx(lrCarState.mLinearVelocity, rw::math::fpu::KF_IS_ZERO_TOLERANCE))
        {
            // Stationary: sit behind the car's own nose (var_1E0 == the lagged frame's z row).
            mDesiredWorldSpaceNormalizedVectorFromCar = lCarTransform.zAxis;
        }
        else
        {
            // Moving: sit against the direction of travel, never below the car
            // (vxor128 with the sign mask, then vandc on the y lane and vrlimi128 it back).
            mDesiredWorldSpaceNormalizedVectorFromCar = rw::math::vpu::Negate(lVelocityDirection);
            mDesiredWorldSpaceNormalizedVectorFromCar.y =
                std::fabs(mDesiredWorldSpaceNormalizedVectorFromCar.y);
        }
        mWorldSpaceNormalizedVectorFromCar = mDesiredWorldSpaceNormalizedVectorFromCar;   // +0x2C0

        // The aim point starts one metre below the lagged car origin: (0.0, -1.0, 0.0, 0) built
        // from flt_82001CC0 / flt_820037C8 at var_240, added to v123, stored to +0x2E0 / +0x2B0.
        const Vector3 lOneMetreDown = { 0.0f, -1.0f, 0.0f, 0.0f };
        mCrashPoint       = lCarPosition + lOneMetreDown;                         // +0x2E0
        mCurrentTargetPos = lCarPosition + lOneMetreDown;                         // +0x2B0

        SetPrepared();                                                            // stb 1, +0x08
        mfTimeSinceLastDecision = lrParameters.mfTimeBetweenDecisions;           // +0x6C -> +0x390
    }

    // .cpp:212..:244 -- manual (right stick) control. Held means x^2 + y^2 > 0.01
    // (vmulfp128 v0,v0,v0 ; splat y + splat x ; vcmpgtfp. against flt_82002138).
    // Camera2DRotationController::GetRawStickVector is the DecFIGS h:134 inline.
    const Vector2 lHeldStick = lrSharedInfo.mRotationController.GetRawStickVector();
    bool lbManualCameraControl = rw::math::vpu::Dot(lHeldStick, lHeldStick) > 0.01f;

    if (lbManualCameraControl)
    {
        if (!mbManualCameraControl)                                               // lbz +0x3C0
        {
            // Taking control: restart the idle clock and orbit from where the camera is now.
            mfTimeSinceLastManualControl = 0.0f;                                  // stfs f29, +0x394
            mManualCameraDirection = rw::math::vpu::Negate(mWorldSpaceNormalizedVectorFromCar);
        }
    }
    else
    {
        // The idle clock runs on the UN-slowed world step (lfs +0x584).
        mfTimeSinceLastManualControl +=
            lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_WORLD_NO_SLOMO);
    }

    if (mbManualCameraControl)
    {
        // Stay manual until the stick has been idle for KF_TIME_UNTIL_CAMERA_RESET AND the car is
        // moving: `fcmpu time, 3.0 ; blt` then `vcmpgtfp. v0, KF, |v|^2` (vmsum3fp128). The speed
        // test compares the SQUARED speed against KF_CAMERA_RESET_MIN_SPEED -- the console's own
        // mixed units, kept.
        if (mfTimeSinceLastManualControl < KF_TIME_UNTIL_CAMERA_RESET
            || KF_CAMERA_RESET_MIN_SPEED > rw::math::vpu::MagnitudeSquared(lrCarState.mLinearVelocity))
        {
            lbManualCameraControl = true;
        }
    }

    if (mbIsRandomStartTempDebugCrashCamera)                                      // lbz +0x3C4
    {
        lbManualCameraControl = true;
    }
    mbManualCameraControl = lbManualCameraControl;                                // stb r9, +0x3C0

    // .cpp:248..:260 -- the direction the camera wants to look along (v0 at 0x822287F4).
    Vector3 lLookDirection;
    if (lbManualCameraControl)
    {
        const Vector2 lOrbitStick = lrSharedInfo.mSphericalRotationController.GetRawStickVector();

        // .cpp:248..:249 -- orbit: yaw the manual direction by stick x * KF_CAMERA_X_ROTATION_SPEED
        // (`bl XMMatrixRotationY` @0x82228724 -- the xenon SDK's Y-rotation builder -- then the
        // three-row vmulfp128/vmaddfp/vmaddfp @0x8222875C..0x82228764 == TransformVector).
        mManualCameraDirection = rw::math::vpu::TransformVector(
            rw::math::vpu::MakeRotationY(lOrbitStick.x * KF_CAMERA_X_ROTATION_SPEED),
            mManualCameraDirection);

        // .cpp:254 -- raise / lower by stick y * KF_CAMERA_Y_ROTATION_SPEED, clamped to
        // +-KF_MIN_CAMERA_MANUAL_HEIGHT_TWEAK: `vmaddfp v12, v12, v9, v11` (stick*rate + tweak),
        // `vmaxfp v12, -K, v12` then `vminfp v13, K, v12` (0x822287A8..0x822287B8).
        mfManualHeightAdjustment = rw::math::vpu::Splat(VecFloatClamp(
            lOrbitStick.y * KF_CAMERA_Y_ROTATION_SPEED + mfManualHeightAdjustment.x,
            -KF_MIN_CAMERA_MANUAL_HEIGHT_TWEAK.x,
            KF_MIN_CAMERA_MANUAL_HEIGHT_TWEAK.x));

        lLookDirection = mManualCameraDirection;
    }
    else
    {
        // .cpp:260 -- let the height tweak die away (vmulfp128 by splat(KF_CAMERA_RESET_SPEED)),
        // and look along the car's travel (v0 == the velocity, reloaded at 0x822286C4).
        mfManualHeightAdjustment = mfManualHeightAdjustment * rw::math::vpu::Splat(KF_CAMERA_RESET_SPEED);

        lLookDirection = lrCarState.mLinearVelocity;
    }

    // .cpp:266..:270 -- always look DOWN on the car: the look direction's y is replaced by
    //   -KF_LOOK_DOWN_SCALE * sqrt( Clamp(y^2, MIN_TAN_SQ * (x^2 + z^2), MAX_TAN_SQ * (x^2 + z^2)) )
    // (0x822287F8..0x822288B8: x*x + z^2 by vmaddfp, the two VecFloat scales, `vmaxfp v12, lo, y^2`
    // then `vminfp v13, hi, v12`, the vsel-guarded sqrt, the multiply by splat(-KF_LOOK_DOWN_SCALE)
    // (`fneg` @0x82228844) and `vrlimi128 v0, v13, 4, 0` into the y lane).
    const f32 lfHorizontalSq = lLookDirection.x * lLookDirection.x + lLookDirection.z * lLookDirection.z;
    const f32 lfVerticalSq   = lLookDirection.y * lLookDirection.y;
    lLookDirection.y = -KF_LOOK_DOWN_SCALE
                     * std::sqrt(VecFloatClamp(lfVerticalSq,
                                               KF_MIN_CAMERA_LOOK_TAN_SQ_ANGLE.x * lfHorizontalSq,
                                               KF_MAX_CAMERA_LOOK_TAN_SQ_ANGLE.x * lfHorizontalSq));

    // .cpp:273..:276 -- the desired direction FROM the car TO the camera (0x822288C4..0x82228998).
    if (!IsZeroVmx(lLookDirection, rw::math::fpu::KF_IS_ZERO_TOLERANCE))
    {
        mDesiredWorldSpaceNormalizedVectorFromCar =
            rw::math::vpu::Negate(rw::math::vpu::Normalize(lLookDirection));      // stvx128 +0x2D0
        CGS_ASSERT(!IsZeroVmx(mDesiredWorldSpaceNormalizedVectorFromCar,
                              rw::math::fpu::KF_IS_ZERO_TOLERANCE),
                   "!IsZero(mDesiredWorldSpaceNormalizedVectorFromCar)");         // .cpp:276
    }

    // .cpp:287..:322 -- the height and distance the camera holds from the car.
    if (mbIsTempDebugCrashCamera)                                                 // lbz +0x3C3
    {
        // The debug crash camera (the takedown state raises this flag): stick y walks a 0..1
        // parameter between the authored slow and fast framing (0x822289A8..0x82228A50).
        f32 lfAdjust = lrSharedInfo.mSphericalRotationController.GetRawStickVector().y;
        if (rw::math::fpu::Abs(lfAdjust) < sfDeadzone)                            // fcmpu ; bge
        {
            lfAdjust = 0.0f;
        }

        // `fnmsubs f0, (dt_game * adjust), rate, param` then the fsel ladder
        // fsel(-p, 0.0, p) / fsel(1 - p, p, 1.0) == rw::math::fpu::Clamp(p, 0, 1). Both stored.
        mfDebugCrashCameraParam0to1 = mfDebugCrashCameraParam0to1
            - lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_GAME) * lfAdjust * sfAdjustmentRate;
        mfDebugCrashCameraParam0to1 = rw::math::fpu::Clamp(mfDebugCrashCameraParam0to1, 0.0f, 1.0f);

        const f32 lfFraming = mfDebugCrashCameraParam0to1;
        mfHeight += ((lrParameters.mfFastHeight - lrParameters.mfSlowHeight) * lfFraming
                     + lrParameters.mfSlowHeight - mfHeight)
                  * lrParameters.mfHeightDistanceBlendFactor;
        mfDistance += ((lrParameters.mfFastDistance - lrParameters.mfSlowDistance) * lfFraming
                       + lrParameters.mfSlowDistance - mfDistance)
                    * lrParameters.mfHeightDistanceBlendFactor;
    }
    else
    {
        // Frame by speed: fsel(q - 1, 1.0, q) == rw::math::fpu::Min(q, 1) with
        // q = speed / mfHeightDistanceVelocityRange (0x82228A54..0x82228AAC).
        const f32 lfFraming = rw::math::fpu::Min(lfSpeed / lrParameters.mfHeightDistanceVelocityRange, 1.0f);
        mfHeight += ((lrParameters.mfFastHeight - lrParameters.mfSlowHeight) * lfFraming
                     + lrParameters.mfSlowHeight - mfHeight)
                  * lrParameters.mfHeightDistanceBlendFactor;
        mfDistance += ((lrParameters.mfFastDistance - lrParameters.mfSlowDistance) * lfFraming
                       + lrParameters.mfSlowDistance - mfDistance)
                    * lrParameters.mfHeightDistanceBlendFactor;
    }

    // .cpp:327..:369 -- swing the camera's direction toward the desired one, unless it is there
    // already (IsZero(current - desired) against rw::math::fpu::SMALL_FLOAT, 0x82228AE8..0x82228B48).
    // The look length is the vsel-guarded Magnitude computed alongside (var_220).
    const f32 lfLookLength = rw::math::vpu::Magnitude(lLookDirection);
    if (!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar - mDesiredWorldSpaceNormalizedVectorFromCar,
                   KF_SMALL_FLOAT))
    {
        if (!lbManualCameraControl)
        {
            // .cpp:333..:335 -- `fcmpu len, 2.0 (flt_82001D9C) ; bge` -> the maximum (a NaN length
            // takes the maximum too), else the minimum.
            mfBlendFactor = (lfLookLength < 2.0f) ? lrParameters.mfMinimumBlendFactor
                                                  : lrParameters.mfMaximumBlendFactor;
        }
        mfBlendFactor += (lrParameters.mfManualBlendFactor - mfBlendFactor)
                       * lrParameters.mfBlendFactorBlendFactor;                  // fmadds @0x82228BB4

        // .cpp:343..:360 -- blend two look-at frames from the lagged car origin, one along the
        // current direction and one along the desired, by the blend factor (vmaddcfp128
        // dir * distance + origin @0x82228BC4 / 0x82228BFC, then `bl rw::math::vpu::SLerp` with
        // the angle-out pointer at var_220, never read).
        const Matrix44Affine lCurrentLookAt = Utils::CreateLookAt(
            lCarPosition, lCarPosition + mWorldSpaceNormalizedVectorFromCar * mfDistance);
        const Matrix44Affine lDesiredLookAt = Utils::CreateLookAt(
            lCarPosition, lCarPosition + mDesiredWorldSpaceNormalizedVectorFromCar * mfDistance);
        Vector3 lUnusedAngle;
        mWorldSpaceNormalizedVectorFromCar =
            rw::math::vpu::SLerp(lCurrentLookAt, lDesiredLookAt, mfBlendFactor, &lUnusedAngle).zAxis;

        // .cpp:362..:369 -- a collapsed result falls back to +Z (gKVector, unk_82181520), and
        // the direction is re-normalised (rsqrt + two Newton steps, NO zero guard).
        if (IsZeroVmx(mWorldSpaceNormalizedVectorFromCar, rw::math::fpu::KF_IS_ZERO_TOLERANCE))
        {
            mWorldSpaceNormalizedVectorFromCar = rw::math::vpu::GetVector3_ZAxis();
        }
        mWorldSpaceNormalizedVectorFromCar = rw::math::vpu::Normalize(mWorldSpaceNormalizedVectorFromCar);
    }

    // .cpp:372..:375 -- the four tripwires, in the console's order and polarity.
    CGS_ASSERT(rw::math::vpu::IsValid(mWorldSpaceNormalizedVectorFromCar),
               "IsValid(mWorldSpaceNormalizedVectorFromCar)");                    // :372
    CGS_ASSERT(!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar, rw::math::fpu::KF_IS_ZERO_TOLERANCE),
               "!IsZero(mWorldSpaceNormalizedVectorFromCar)");                    // :373
    // `fcmpu d, eps ; bgt` then `fcmpu d, -eps (flt_82002514) ; bge`: the tripwire fires unless
    // one of the two ordered compares holds -- so a NaN distance fires it, as on the console.
    CGS_ASSERT(mfDistance > rw::math::fpu::KF_IS_ZERO_TOLERANCE
               || mfDistance < -rw::math::fpu::KF_IS_ZERO_TOLERANCE,
               "!rw::math::fpu::IsZero(mfDistance)");                             // :374
    CGS_ASSERT(!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar * mfDistance,
                          rw::math::fpu::KF_IS_ZERO_TOLERANCE),
               "!IsZero(mWorldSpaceNormalizedVectorFromCar * mfDistance)");       // :375

    // .cpp:381..:383 -- the camera sits mfDistance out along its direction from the lagged car
    // origin, lifted by the stick's height tweak along CgsCore::K_V3D_YAXIS, and looks at the
    // origin (vmaddfp128 dir * distance + origin @0x82228E98, vmaddfp Y * tweak + that @0x82228E9C,
    // `bl CreateLookAt` v1 = eye, v2 = origin).
    const Vector3 lEyePosition = lCarPosition + mWorldSpaceNormalizedVectorFromCar * mfDistance
                               + rw::math::vpu::Mult(K_V3D_YAXIS, mfManualHeightAdjustment);
    Matrix44Affine lCameraTransform = Utils::CreateLookAt(lEyePosition, lCarPosition);

    // .cpp:389 -- lift the camera by mfHeight (the y lane of the translation only: vaddfp then
    // `vrlimi128 v9, v13, 4, 0` @0x82228F54), then pitch the frame by the authored angle: an
    // X rotation by mfPitch * 0.017453292 (the SDK's MakeRotationX, :239/:240 -- rows
    // (1,0,0) / (0,c,s) / (0,-s,c) / (0,0,0) packed @0x82229050..0x822290BC) pre-multiplied in
    // (Mult(rotation, frame): the rotation's zero translation row leaves the lifted origin).
    lCameraTransform.wAxis.y += mfHeight;
    lCameraTransform = rw::math::vpu::Mult(
        rw::math::vpu::MakeRotationX(lrParameters.mfPitch * KF_DEGS_TO_RADS), lCameraTransform);

    // .cpp:394..:400 -- smooth small camera moves: within KF_SMOOTHING_STOP_DISTANCE_SQ of last
    // frame's position the camera goes KF_SMOOTHING_FACTOR of the way (`vcmpgtfp. v11, KF, |d|^2`
    // @0x82229158; `vmaddfp v12, d, last, factor` == d * factor + last @0x8222917C).
    const Vector3 lCameraMove = lCameraTransform.wAxis - mCameraPositionLastFrame;
    if (KF_SMOOTHING_STOP_DISTANCE_SQ.x > rw::math::vpu::MagnitudeSquared(lCameraMove))
    {
        lCameraTransform.wAxis = mCameraPositionLastFrame + lCameraMove * KF_SMOOTHING_FACTOR;
    }
    mCameraPositionLastFrame = lCameraTransform.wAxis;                            // stvx128 +0x2F0

    // .cpp:404..:406 -- this rig's own shake (r3 = &mShake, r4 = the frame, r5 = &mShakeParams,
    // r6 = &mRandom, f1 = the E_WORLD step, f2 = 1.0), then publish and validate.
    mShake.Update(lCameraTransform, lrParameters.mShakeParams, mRandom, lfTimestep, 1.0f);
    lrCamera.SetTransform(lCameraTransform);                                      // Camera.h:355
    lrCamera.ValidateTransformWithDebugInfo();

    // A bounce this frame kicks the impact shake by the bounce multiplier * KF_BOUNCE_SHAKE_MAGNITUDE
    // (`fmuls f1, f13(+0x3CC), f0(0x82CDA730)` @0x82229214).
    if (CheckForPlayerCarBouncing(lrSharedInfo))
    {
        mImpactEffect.RegisterImpact(mfBounceShakeMultiplier * KF_BOUNCE_SHAKE_MAGNITUDE);
    }

    // .cpp:411..:423 -- the impact shake runs with its own tunings, built on the stack (var_220):
    // shake {0.0, 0.0, 3.0, 1.0} (flt_82001CC0 / flt_82004270 / flt_82001C98), decay 0.06
    // (flt_820047B8), magnitude 60.0 (flt_82004C6C), frequency scale 1.0. The PS3 calls
    // Parameters::Construct first and then overwrites all seven words; the X360 dropped the dead
    // Construct. The effect shakes the CAMERA's transform (r4 = r24), with the shared random.
    Utils::CameraImpactEffect::Parameters lImpactParameters;
    lImpactParameters.mShakeParams.mfXYShakeMagnitudeDegs  = 0.0f;
    lImpactParameters.mShakeParams.mfZShakeMagnitudeDegs   = 0.0f;
    lImpactParameters.mShakeParams.mfXYWobbleMagnitudeDegs = 3.0f;
    lImpactParameters.mShakeParams.mfWobbleCenteringFactor = 1.0f;
    lImpactParameters.mfShakeDecayFactor    = 0.06f;
    lImpactParameters.mfShakeMagnitude      = 60.0f;
    lImpactParameters.mfShakeFrequencyScale = 1.0f;
    mImpactEffect.Update(lrCamera, lImpactParameters, *lrSharedInfo.mpRandom, lfTimestep);

    // .cpp:425 -- the authored FOV (Camera::SetFOV, Camera.h:424 asserts it is positive), then
    // mark the camera valid (`ld/ori 2/std` on the state's current flags @0x822292B8..0x822292C8
    // -- CameraState::SetFlag, BrnCameraState.h:114, inlined).
    lrCamera.SetFOV(lrParameters.mfFOV);
    lrCamera.GetState().SetFlag(CameraState::E_FLAG_VALID, true);

    // Roll the published camera by the crash-mode tilt (the SDK's MakeRotationZ, :269 -- rows
    // (c,s,0) / (-s,c,0) / (0,0,1) / (0,0,0) @0x8222942C..0x82229448 -- pre-multiplied into the
    // camera's four rows @0x8222944C..0x822294BC).
    lrCamera.SetTransform(rw::math::vpu::Mult(rw::math::vpu::MakeRotationZ(mfRollAngleRads),
                                              lrCamera.GetTransform()));

    // .cpp:435 -- the slow-mo close-up frames the car from beside it: aim half a metre below the
    // lagged origin (gJVector * 0.5, v122 == vcsxwfp128 1,1), from four metres out along the
    // camera's direction flattened onto the ground (GetWorldSpaceVectorFromCar, then SetY(0) --
    // `vrlimi128 v0, v127(0), 4, 3` @0x82229500 -- and two SDK Mults by GetVecFloat_Two,
    // `vmulfp128 v1, v127, v0` + `vmaddcfp128 v1, v0, v1, v126` @0x82229538/0x8222953C).
    const Vector3 lCloseupTarget = lCarPosition - rw::math::vpu::GetVector3_YAxis() * 0.5f;
    Vector3 lFlatVectorFromCar = GetWorldSpaceVectorFromCar();                    // .h:104
    lFlatVectorFromCar.y = 0.0f;
    const Vector3 lCloseupEye = lCloseupTarget + (lFlatVectorFromCar * 2.0f) * 2.0f;

    // .cpp:449..:455 -- blend the published camera into the close-up by the eased close-up amount
    // (SineLerp(0.0, 1.0, +0x3D4), `bl rw::math::vpu::SLerp` r4 = the camera, r5 = the look-at),
    // publish and validate.
    Vector3 lUnusedAngle;
    lrCamera.SetTransform(rw::math::vpu::SLerp(lrCamera.GetTransform(),
                                               Utils::CreateLookAt(lCloseupEye, lCloseupTarget),
                                               Utils::SineLerp(0.0f, 1.0f, mfCloseupAmount0To1),
                                               &lUnusedAngle));
    lrCamera.ValidateTransformWithDebugInfo();

    // [DIAG] NOT IN THE X360 BINARY -- BRN_CAMRIG_DIAG, the FX-CAMRIG live witness: proves this
    // body ran and shows what it published against the lagged car origin it follows. The first
    // ten frames after each Prepare, then every 15th -- and at most 240 lines per session (HARD
    // CAP 2026-09-24, crash-parity diag hygiene: the rate limit alone grew with every crash
    // camera a long drive raised). Prints only.
    {
        static const bool sbRigDiag = (getenv("BRN_CAMRIG_DIAG") != 0);
        static s32 siDiagFrame = 0;
        static s32 siDiagLinesLeft = 240;
        if (lbDiagFirstFrame)
        {
            siDiagFrame = 0;
        }
        if (sbRigDiag && siDiagLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0 &&
            (siDiagFrame < 10 || siDiagFrame % 15 == 0))
        {
            --siDiagLinesLeft;
            const Vector3& lrPublished = lrCamera.GetTransform().wAxis;
            *CgsDev::Log::gpDebugPrint
                << "[camrig] Update frame " << siDiagFrame
                << " first " << (lbDiagFirstFrame ? 1 : 0)
                << " tempDebug " << (mbIsTempDebugCrashCamera ? 1 : 0)
                << " manual " << (mbManualCameraControl ? 1 : 0)
                << " car " << lCarPosition.x << " " << lCarPosition.y << " " << lCarPosition.z
                << " cam " << lrPublished.x << " " << lrPublished.y << " " << lrPublished.z
                << " dir " << mWorldSpaceNormalizedVectorFromCar.x << " "
                << mWorldSpaceNormalizedVectorFromCar.y << " " << mWorldSpaceNormalizedVectorFromCar.z
                << " dist " << mfDistance << " height " << mfHeight << " blend " << mfBlendFactor
                << " impact " << mImpactEffect.GetImpactFactor()
                << " roll " << mfRollAngleRads << " closeup " << mfCloseupAmount0To1
                << " fov " << lrCamera.GetFOV() << "\n";
        }
        ++siDiagFrame;
    }

    return true;                                                                  // li r3, 1
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::CheckForPlayerCarBouncing @0x8220F340 (46 insns; DWARF .cpp:465)
//   0x8220F358  lvx128 v0, r4, 0x390 ; vspltw v0, v0, 1   ; splat(mLinearVelocity.y)
//   0x8220F37C  vcmpgefp. v13, v0, splat(0.0)            ; flt_82001CC0 -- a NaN fails it
//   0x8220F38C  beq -> 0x8220F3E8: stb 1, +0x3C1 ; return 0   (moving down: arm)
//   0x8220F390  lbz +0x3C1 ; beq -> return 0                 (not armed: nothing to report)
//   0x8220F3A0  stb 0, +0x3C1                                (disarm)
//   0x8220F3CC  vcmpgtfp. v0, v0, splat(flt_82CDA734)        ; KF_MIN_SPEED_FOR_CAMERA_BOUNCE
//   0x8220F3E0  li r3, 1 only when that holds
// ----------------------------------------------------------------------------
bool BehaviourAftertouchCrash::CheckForPlayerCarBouncing(const BehaviourSharedInfo& lrSharedInfo)
{
    const f32 lfVerticalSpeed = lrSharedInfo.mPlayerInfo.mRaceCarState.mLinearVelocity.y;

    if (lfVerticalSpeed >= 0.0f)
    {
        if (mbWasFallingDownwards)
        {
            mbWasFallingDownwards = false;
            return lfVerticalSpeed > KF_MIN_SPEED_FOR_CAMERA_BOUNCE;
        }
    }
    else
    {
        mbWasFallingDownwards = true;
    }

    return false;
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::GetCollisionPolicy
//   lbz    r11, +0x3C2(r3)    ; mbDisableCollision
//   addi   r3,  r3, 0x60      ; r3 = &this->mCollisionPolicy (the candidate return)
//   cmplwi r11, 0
//   beqlr                     ; flag clear -> return &mCollisionPolicy
//   li     r3, 0              ; flag set   -> return null
//   blr
//
// The `addi r3, r3, 0x60` is the derived-to-base adjustment folded into the return: on the
// console CollisionPolicyAttachedToVehicle's CollisionPolicy sub-object is at its own +0x00, so
// the policy's address and the interface pointer coincide. On the host the compiler emits
// whatever adjustment the real base sub-object needs; parity is by named member.
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourAftertouchCrash::GetCollisionPolicy()
{
    if (mbDisableCollision)                                 // lbz +0x3C2; bne -> null
    {
        return 0;
    }
    return &mCollisionPolicy;                               // this + 0x60
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::SetupTweaker -- vtable slot 6, 0x821FAA70 (ICF-folded with
// BehaviourIceAnim::SetupTweaker): `mr r3, r4 ; b Tweaker::Construct` -- seed the supplied tweaker.
// ----------------------------------------------------------------------------
void BehaviourAftertouchCrash::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::GetName -- returns the class's own literal.
// ----------------------------------------------------------------------------
const char* BehaviourAftertouchCrash::GetName() const
{
    return "BehaviourAftertouchCrash";
}

} // namespace Camera
} // namespace BrnDirector
