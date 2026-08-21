// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourInterpolate slice this TU owns.
// The trivial accessors/setters (GetCameraAForSetup/GetCameraBForSetup/SetupDuration/Setup/
// HasFinished) are inline in the header; the two non-trivial members -- GetCollisionPolicy
// (returns the forward-declared collision-policy sub-object) and GetParametricTime (the
// fsel-clamped blend parameter) -- are bodied out of line here. The rest of the behaviour
// (Construct/Prepare/Update/Release and the interpolation math) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"
#include "GameSource/Director/Camera/BrnBehaviourManager.h"   // BehaviourManager (the two GetCamera resolves)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log::gpDebugPrint (bring-up measurement)

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The X360 file string every assert in this class's .cpp carries
    // ("..\\..\\..\\GameSource\\Director/Camera/Behaviours/BrnBehaviourInterpolate.cpp").
    // Line numbers are the `li r5, <n>` immediates quoted per assert below.

    // rw::math::fpu::IsZero's epsilon band -- flt_82001770 / flt_82002514, read straight
    // off the .rdata (+/-1.1920928955078125e-07 == FLT_EPSILON).
    const f32 KF_IS_ZERO_EPSILON = 1.1920928955078125e-07f;

    // flt_82001C98 / flt_82001CC0.
    const f32 KF_ONE  = 1.0f;
    const f32 KF_ZERO = 0.0f;

    // rw::math::fpu::IsZero(x) -- the console's `x > EPS ? false : (x >= -EPS)`.
    bool IsZero(f32 lfValue)
    {
        if (lfValue > KF_IS_ZERO_EPSILON)
            return false;
        return lfValue >= -KF_IS_ZERO_EPSILON;
    }
}

// ⛔ RETIRED 2026-08-01: this TU used to carry seven `offsetof(BehaviourInterpolate, ...)`
// static_asserts against X360 displacements (0x2A4 / 0x2B0 / 0x420 / 0x590 / 0x594 / 0x595 /
// 0x596). They only ever passed because the class was modelled out of FABRICATED reserved
// spans padded to hit those displacements on x64. The class now embeds the real
// Camera::Behaviour base, the real VisibilityCollisionPolicy and the real CameraReference, so
// the host layout legitimately differs from the console's -- and asserting a console byte
// offset on an x64 build is exactly the trap the project's x64 rule exists to stop.
// Parity is BY NAMED MEMBER; the offsets survive as provenance comments in the header.

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetCollisionPolicy @0x821F9EE8
//   lbz   r11, 0x594(r3)     ; mbUseCollisionPolicy
//   addi  r3, r3, 0x20       ; r3 = &mCollisionPolicy (computed unconditionally)
//   cmplwi r11, 0
//   bnelr cr6                ; if (mbUseCollisionPolicy) return &mCollisionPolicy
//   li    r3, 0              ; else return NULL
// ----------------------------------------------------------------------------
CollisionPolicy*
BehaviourInterpolate::GetCollisionPolicy()
{
    if (mbUseCollisionPolicy)
    {
        return &mCollisionPolicy;   // this + 0x20 -- by NAME now that the real policy is embedded
    }
    return 0;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetParametricTime @0x822065F8
//   lfs   f0,  0x2A4(r31)    ; mfDuration
//   ... assert mfDuration != 0.0f ...
//   lfs   f13, 0x2A4(r31)    ; mfDuration
//   lfs   f0,  0x590(r31)    ; mfRunningTime
//   fdivs f0, f0, f13        ; ratio = mfRunningTime / mfDuration
//   fneg  f13, f0            ; -ratio
//   fsel  f0, f13, f31, f0   ; f0 = (-ratio >= 0) ? 0.0 : ratio   == max(ratio, 0)
//   fsubs f12, f13(=1.0), f0 ; 1.0 - f0
//   fsel  f1, f12, f0, f13   ; result = (1.0 - f0 >= 0) ? f0 : 1.0 == min(f0, 1.0)
//   -> result = clamp(mfRunningTime / mfDuration, 0.0f, 1.0f)
// (fsel a,b,c selects b when a >= 0, else c. f31 = 0.0f, the value the duration assert
//  compares against; the 1.0f comes from rodata flt_82001C98.)
// ----------------------------------------------------------------------------
f32
BehaviourInterpolate::GetParametricTime() const
{
    CGS_ASSERT(mfDuration != 0.0f, "mfDuration != 0.0f");

    f32 lfRatio = mfRunningTime / mfDuration;

    // fsel f0, -ratio, 0.0, ratio  -> clamp the low end to 0.0
    f32 lfClampedLow = (-lfRatio >= 0.0f) ? 0.0f : lfRatio;

    // fsel f1, 1.0 - f0, f0, 1.0   -> clamp the high end to 1.0
    f32 lfResult = (1.0f - lfClampedLow >= 0.0f) ? lfClampedLow : 1.0f;

    return lfResult;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::Construct @0x82255FC8
//
//   0x82255FF8..0x82256010  the SEVEN base stores -- i.e. Behaviour::Construct() inlined
//                           (mbIsPrepared/mbHasFailed/mbTweakerAttached/mbCanSwitchToMeNow/
//                            mbCanSwitchFromMeNow = 0, meTimestepType = 0 == E_WORLD,
//                            mpcDebugParametersName = 0)
//   0x82256014..0x82256020  this+0x260 and this+0x280: two 16-byte blocks + a trailing byte,
//                           each zeroed -- the two Utils::Interpolater sub-objects
//                           CameraInterpolationController embeds (BrnInterpolater.h)
//   0x82256024..0x82256040  mFromCamera / mToCamera: meType = E_TYPE_INVALID, Camera::Construct
//                           on the embedded camera, mpIceWrapper = 0, mbBehaviourLocked = false
//                           -- i.e. CameraReference::Construct() inlined, twice
//   0x8225604C..0x822560D8  the VisibilityCollisionPolicy::Construct() block on this+0x20 (GATED)
//   0x822560DC..0x822560EC  mfDuration = 0, mfRunningTime = 0, mbHasFinished = false,
//                           mbUseCollisionPolicy = false, mbSetup = false
//
// ⭐ mpParameters (+0x2A0) IS DELIBERATELY NOT WRITTEN -- which is exactly why Update and
// PostCollisionUpdate below both open with an "mpParameters" assert.
// ----------------------------------------------------------------------------
void
BehaviourInterpolate::Construct()
{
    Behaviour::Construct();

    // The console INLINES the two Utils::Interpolater constructions here (this+0x260 and
    // this+0x280, each `stvx 0` + `stb 0` at +0x10). Now that the controller is a real type,
    // this reaches them BY NAME through its own Construct rather than zeroing raw storage.
    mInterpolator.Construct();

    mFromCamera.Construct();
    mToCamera.Construct();

    // ⚠️ GATE: VisibilityCollisionPolicy::Construct(mCollisionPolicy) -- the console's
    //   0x8225604C..0x822560D8 block (~25 stores across the policy's interior, including the
    //   see-through triple at +0x1A0..+0x1A2 and the two 1.5f / 0.5f leaf magnitudes).
    //   VisibilityCollisionPolicy::Construct is declaration-only in this tree and this TU is
    //   not its home, so the policy is left as the pool allocation left it.
    //   CONSEQUENCE: NONE on any live path -- mbUseCollisionPolicy is false below and
    //   GetCollisionPolicy() (the ONLY way anything reaches this sub-object) therefore returns
    //   NULL for the whole life of the behaviour. Nothing in the image sets that flag on an
    //   interpolate behaviour.
    //   DELETE-WHEN: VisibilityCollisionPolicy::Construct is bodied.

    mfDuration           = KF_ZERO;
    mfRunningTime        = KF_ZERO;
    mbHasFinished        = false;
    mbUseCollisionPolicy = false;
    mbSetup              = false;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::Prepare @0x82252A10
//   lbz  r11, 0x595(r31)                     ; assert(mbSetup)                          :78
//   lwz  r11, 0(r29)                         ; assert(lSharedInfo.mpInterpolateLockInterface) :79
//   CameraReference::Prepare(this + 0x2B0, lock)   ; mFromCamera
//   CameraReference::Prepare(this + 0x420, lock)   ; mToCamera
//   stfs 0.0f, 0x590(r31)                    ; mfRunningTime = 0
//   stb  0,    8(r31)                        ; SetNotPrepared()
//   li   r3, 1                               ; return true
// (the two CameraReference::Prepare calls are what LOCK the two source behaviours in the
//  manager pool, so the arbitrator state may Release its own handles while the blend runs.)
// ----------------------------------------------------------------------------
bool
BehaviourInterpolate::Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo)
{
    CGS_ASSERT(mbSetup, "mbSetup");                                                   // :78
    CGS_ASSERT(lrInfo.mpInterpolateLockInterface != 0,
               "lSharedInfo.mpInterpolateLockInterface != NULL");                     // :79

    mFromCamera.Prepare(*lrInfo.mpInterpolateLockInterface);
    mToCamera.Prepare(*lrInfo.mpInterpolateLockInterface);

    mfRunningTime = KF_ZERO;
    SetNotPrepared();

    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::Update @0x82244998
//   assert(mpParameters)                                                             :101
//   assert(mbSetup)                                                                  :102
//   assert(lrSharedInfo.mpBehaviourManager)      ; lwz 0x5C0(sharedInfo)              :103
//   Camera lFrom(mFromCamera.GetCamera(lrSharedInfo.mpBehaviourManager));   ; var_190
//   Camera lTo  (mToCamera.GetCamera  (lrSharedInfo.mpBehaviourManager));   ; var_2F0
//   if (!mbIsPrepared) { lrCamera = lFrom; mbIsPrepared = true; }   ; lbz 8 / operator= / stb 1,8
//   <the collision-policy SetTarget block on this+0x20 -- GATED below>
//   return true                                                    ; li r3, 1
//
// ⭐ THE FIRST-FRAME SEED IS THE POINT OF THIS FUNCTION. The blend itself happens in
// PostCollisionUpdate; Update exists so that the frame the interpolator goes live the helper's
// camera is the SOURCE camera rather than whatever Camera::Construct left, and so the
// collision policy has a target before the collision pass runs.
// ----------------------------------------------------------------------------
bool
BehaviourInterpolate::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    CGS_ASSERT(mpParameters != 0, "mpParameters");                                    // :101
    CGS_ASSERT(mbSetup, "mbSetup");                                                   // :102
    CGS_ASSERT(lrInfo.mpBehaviourManager != 0, "lrSharedInfo.mpBehaviourManager");    // :103

    const Camera lFrom(mFromCamera.GetCamera(lrInfo.mpBehaviourManager));
    const Camera lTo(mToCamera.GetCamera(lrInfo.mpBehaviourManager));
    (void)lTo;

    if (!mbIsPrepared)
    {
        lrCamera     = lFrom;
        mbIsPrepared = true;
    }

    // ⚠️ GATE: the console's tail is an inlined
    //     mCollisionPolicy.SetTarget(lTo.mTransform, AABBox(0,0), *gTargetEntityId)
    //   -- policy +0x0A = true, the four transform rows into policy +0x10..+0x40, a 32-byte
    //   zero AABB into policy +0x50, and *dword_82CDA790 into policy +0x230
    //   (0x82244A80..0x82244B10). VisibilityCollisionPolicy::SetTarget is declaration-only in
    //   this tree and has no console export (it is inlined at every call site), and this TU is
    //   not the policy's home.
    //   CONSEQUENCE: NONE on any live path -- see the Construct GATE: mbUseCollisionPolicy is
    //   false for the whole life of an interpolate behaviour, so GetCollisionPolicy() returns
    //   NULL and no collision pass ever reads the target.
    //   DELETE-WHEN: VisibilityCollisionPolicy::SetTarget is bodied.

    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::PostCollisionUpdate @0x82252AB8
//   ⭐⭐ THIS IS THE BLEND. Everything the interpolate behaviour actually does per frame
//   happens in vtable slot 3, not slot 2.
//
//   assert(mpParameters)                                                             :137
//   assert(mbSetup)                                                                  :138
//   assert(lrSharedInfo.mpBehaviourManager)                                          :139
//   Camera lFrom(mFromCamera.GetCamera(mgr));            ; var_2F0
//   Camera lTo  (mToCamera.GetCamera(mgr));              ; var_190
//   assert(!rw::math::fpu::IsZero(mfDuration))                                       :145
//   f32 lfT = mfRunningTime / mfDuration;
//   if (lfT >= 1.0f) { lfT = 1.0f; mbHasFinished = true; }
//   else if (!mbHasFailed) SetCantSwitchFromMeNow(lrCamera, E_FIRST_NOCUTFROM_FLAG /*27*/);
//   mfRunningTime += lrSharedInfo.GetTimestep(meTimestepType);   ; the inlined Timestep::Get,
//                                                                ; carrying its own :78 assert
//   lrCamera = lFrom;                                            ; Camera::operator=
//   lrCamera.GetEffects().mfGameCameraBlend   = lfT;             ; stfs f31, 0x108(camera)
//   lrCamera.GetEffects().mu8BlendCurve       = mpParameters->meInterpolationMapping; ; +0x11D
//   lrCamera.GetEffects().mu8InterpolateType  = mpParameters->meInterpolationMethod;  ; +0x11E
//   mInterpolator.Update(lrCamera, lTo, lrSharedInfo.GetEyeTarget());   ; +0x250 == the
//                                                                ; player car's world transform
//   return true;
//
// ⚠️ THE THREE CAMERA FIELDS ARE THE SAME THREE THE ICE TAKE WRITES (BrnCameraEffects.h names
//   them from the ICE element side: GAME_CAMERA_BLEND / BLEND_CURVE / INTERPOLATE_TYPE). That
//   is an independent corroboration of both this decode and that carve-out.
// ----------------------------------------------------------------------------
bool
BehaviourInterpolate::PostCollisionUpdate(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    CGS_ASSERT(mpParameters != 0, "mpParameters");                                    // :137
    CGS_ASSERT(mbSetup, "mbSetup");                                                   // :138
    CGS_ASSERT(lrInfo.mpBehaviourManager != 0, "lrSharedInfo.mpBehaviourManager");    // :139

    const Camera lFrom(mFromCamera.GetCamera(lrInfo.mpBehaviourManager));
    const Camera lTo(mToCamera.GetCamera(lrInfo.mpBehaviourManager));

    CGS_ASSERT(!IsZero(mfDuration), "!rw::math::fpu::IsZero(mfDuration)");            // :145

    f32 lfParametricTime = mfRunningTime / mfDuration;
    if (lfParametricTime >= KF_ONE)
    {
        lfParametricTime = KF_ONE;
        mbHasFinished    = true;
    }
    else if (!mbHasFailed)
    {
        SetCantSwitchFromMeNow(lrCamera, ValidityAccount::E_FIRST_NOCUTFROM_FLAG);    // 0x1B
    }

    mfRunningTime += lrInfo.GetTimestep(meTimestepType);

    lrCamera = lFrom;
    lrCamera.GetEffects().mfGameCameraBlend  = lfParametricTime;
    lrCamera.GetEffects().mu8BlendCurve      =
        static_cast<u8>(mpParameters->GetInterpolationMapping());
    lrCamera.GetEffects().mu8InterpolateType =
        static_cast<u8>(mpParameters->GetInterpolationMethod());

    // ⭐⭐ THE ENDPOINT OF THE BLEND -- now the REAL one.
    // The console's last statement is
    //     CameraInterpolationController::Update(&mInterpolator, lrCamera, lTo,
    //                                           lrSharedInfo.GetEyeTarget())   @0x822513D8
    // and that is exactly what runs here. It reads the three fields written just above,
    // maps the parametric time through the selected easing curve, blends the TRANSFORM by
    // the selected method, then lerps CameraState, CameraEffects, the five-float
    // DepthOfField block, the FOV and the near-clip distance toward lTo.
    //
    // ⛔ THE t == 1 CUT THAT USED TO LIVE HERE IS GONE (2026-08-20). It read
    //     if (lfParametricTime >= KF_ONE) { lrCamera = lTo; }
    // with a documented gate saying the in-between needed
    // CameraInterpolationController::Update + RotateAboutPivot +
    // ExtractRotateAboutPivotParams + Matrix44AffineFromRota + CameraState::Interpolate +
    // the direction-preserving slerp. All of those landed, so the ramp is real: this is the
    // fix for camera transitions snapping instead of easing.
    //
    // ⚠️ Update ALSO reaches the t == 1 endpoint by itself -- at a parametric time of 1
    // every one of its blends returns lTo exactly (an endpoint is an endpoint for the
    // rotate-about-pivot round trip and for every one of the four easing mappings). So the
    // removed special case was never doing anything Update does not also do; it was only
    // ever the *rest* of the ramp that was missing.
    mInterpolator.Update(lrCamera, lTo, lrInfo.GetEyeTarget());

    // [BRING-UP MEASUREMENT, camera 1:1 wave] the blend's two endpoints and the parametric
    // time, so a frozen or mis-framed shot can be attributed to the SOURCE camera, the TARGET
    // camera or the blend without another boot. Burst window (first 4, then every 600th
    // ~= 10 s) rather than a modulo sample, so the print cannot alias with a per-frame loop.
    // Remove once the junkyard shot is settled.
    {
        static u32 suBlendTraceCount = 0;
        ++suBlendTraceCount;
        if ((suBlendTraceCount <= 4u || (suBlendTraceCount % 600u) == 0u)
            && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[interp] #" << static_cast<s32>(suBlendTraceCount)
                << " t=" << lfParametricTime
                << " from (" << lFrom.mTransform.wAxis.x << ", " << lFrom.mTransform.wAxis.y
                << ", " << lFrom.mTransform.wAxis.z << ") fov " << lFrom.mfFOV
                << " -> to (" << lTo.mTransform.wAxis.x << ", " << lTo.mTransform.wAxis.y
                << ", " << lTo.mTransform.wAxis.z << ") fov " << lTo.mfFOV << "\n";
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::Release @0x82252CB8
//   lwz  r11, 0(r31)                        ; assert(lSharedInfo.mpInterpolateLockInterface) :207
//   CameraReference::Release(this + 0x2B0, lock)   ; mFromCamera -- unlock + invalidate
//   CameraReference::Release(this + 0x420, lock)   ; mToCamera
// (no return value; the base slot is void.)
// ----------------------------------------------------------------------------
void
BehaviourInterpolate::Release(const BehaviourSharedPrepareReleaseInfo& lrInfo)
{
    CGS_ASSERT(lrInfo.mpInterpolateLockInterface != 0,
               "lSharedInfo.mpInterpolateLockInterface != NULL");                     // :207 (0xCF)

    mFromCamera.Release(*lrInfo.mpInterpolateLockInterface);
    mToCamera.Release(*lrInfo.mpInterpolateLockInterface);
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetName @0x821F9F00
//   lis/addi r3, aBehaviourinter ; "BehaviourInterpolate"
//   blr
// ----------------------------------------------------------------------------
const char*
BehaviourInterpolate::GetName() const
{
    return "BehaviourInterpolate";
}

} // namespace Camera
} // namespace BrnDirector
