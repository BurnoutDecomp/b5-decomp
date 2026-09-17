// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourSpirallingDeathcam.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourSpirallingDeathcam:
//   - Construct             @0x8222C088
//   - Prepare(info)         @0x821FB3F8  (clears the base's prepared byte, returns true)
//   - Update                @0x8224A528  (the orbit, the looker, the shake, the hook)
//   - GetCollisionPolicy    (DWARF cpp:225; ICF-folded on the X360 -- `addi r3, r3, 0x20`)
//   - GetName               @0x821FB600
//   - SetParameters         @0x821F5680
//   - Start                 @0x821F5620  (latches mbStarted)
//   - Parameters::Construct @0x821FB498
//
// Prepare is run when the behaviour is installed (BehaviourHelper::Prepare); Start is run by
// the crashing/testbed arbitrator states (ArbStateCrashing::Update / ArbStateTestbed::Update)
// when the spiralling deathcam first goes active; Update runs every director frame the
// behaviour is installed.
//
// ⭐ REBUILT 2026-09-17 (see the header banner): the class was a hollow shell without its
//   Behaviour base and crashed at the first wreck once the director's action-205 arm made
//   mbRoadRageTotalled reachable.
// ============================================================================
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"

#include "GameSource/Director/Camera/Camera.h"                     // Camera (mTransform / mState_uFlags / effects / DoF)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"     // VehicleInfo (mRaceCarState / mAABB)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"    // EnsureEffectIsPlaying / EffectInterface
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"         // Timestep::Get / GetVecFloat
#include "GameShared/GameClasses/Numeric/CgsRandom.h"              // CgsNumeric::Random (by value into the Looker)

#include <cmath>   // std::sin / std::cos / std::sqrt

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The console's clamp-to-[0,1] idiom: two `fsel`s against 0.0 (f0) and 1.0 (f1), as
    // emitted four times at the tail of Update @0x8224A528.
    inline f32 Clamp01(f32 lfValue)
    {
        if (-lfValue >= 0.0f)          // fsel fX, -v, 0.0, v      -> v <= 0 ? 0 : v
        {
            lfValue = 0.0f;
        }
        if (1.0f - lfValue < 0.0f)     // fsel fX, 1-v, v, 1.0     -> v > 1 ? 1 : v
        {
            lfValue = 1.0f;
        }
        return lfValue;
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Construct @0x8222C088
//
//   stw 0, 4/8..0x10(this)                       ; Behaviour::Construct, inlined
//   bl  CollisionPolicyAttachedToVehicle::Construct(this+0x20, 0)
//   stb 0, +0x268 ; stb 1, +0x269 ; stb 1, +0x26C ; stb 1, +0x26D   ; the four policy switches
//   lvx/vrlimi128/stvx on +0x270 / +0x280       ; lanes X and Z of both orbit vectors <- 0.0
//                                                 (the three zero stack vectors var_40/var_30/
//                                                  var_20, splat of flt_82001CC0)
//   stfs 0.0, +0x2A0 ; stfs 0.2, +0x2B0 ; stb 1,1,1,0 @+0x2BC..+0x2BF   ; Looker::Construct
//   stfs 0.0 x4 @+0x2C0..+0x2CC                  ; CameraShake::Construct
//   stw 0, +0x2D0                                ; mpParameters
//   stw 0, +0x2D4 ; stw -1, +0x2D8 ; stw 0, +0x2DC ; stb 1, +0x2E0   ; VehicleRef = player car
//   stfs 0.0 @+0x2E4 / +0x2E8 / +0x2EC / +0x2F0  ; the four amounts
//   stb 0, +0x2F4                                ; mbStarted
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::Construct()
{
    Behaviour::Construct();

    mCollisionPolicy.Construct(false);                  // bl ..., r4 == 0
    mCollisionPolicy.SetAutoElevate(false);             // policy +0x248
    mCollisionPolicy.SetSmoothRadiusChanges(true);      // policy +0x249
    mCollisionPolicy.SetTestAgainstWorldOnly(true);     // policy +0x24C
    mCollisionPolicy.SetUseFrustrumResolver(true);      // policy +0x24D

    // The console inserts 0.0 into lanes X and Z of both vectors (YRotationDegs / Height,
    // Radius / LookOffsetAmount) and leaves the other lanes as the pool handed them: every one
    // of those is WRITTEN by the first Update (the +8 seed block) before it is read, so a zero
    // seed here is invisible to the console's arithmetic and keeps the object deterministic.
    mVectorData.mVector1.x = 0.0f; mVectorData.mVector1.y = 0.0f;
    mVectorData.mVector1.z = 0.0f; mVectorData.mVector1.w = 0.0f;
    mVectorData.mVector2.x = 0.0f; mVectorData.mVector2.y = 0.0f;
    mVectorData.mVector2.z = 0.0f; mVectorData.mVector2.w = 0.0f;
    // mAttachmentPos (+0x290) is not written by Construct; the first Update seeds it from the car.

    mLooker.Construct();                                // +0x2A0: 0.0 / 0.2 / 1,1,1,0
    mShake.Construct();                                 // +0x2C0..+0x2CC: 0.0 x4

    mpParameters = 0;                                   // stw 0, +0x2D0
    mAttachment.Set(BrnDirector::VehicleRef::E_PLAYER_CAR,
                    E_ACTIVE_RACE_CAR_INDEX_INVALID, 0u);   // {E_PLAYER_CAR, -1, 0, set}

    mfBlurAmount       = 0.0f;                          // stfs +0x2E4
    mfShakeAmount      = 0.0f;                          // stfs +0x2E8
    mfLookOffsetFactor = 0.0f;                          // stfs +0x2EC
    mfRunningTime      = 0.0f;                          // stfs +0x2F0
    mbStarted          = false;                         // stb 0, +0x2F4
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Prepare @0x821FB3F8
//   stb 0, 8(this)      ; the base's mbIsPrepared -- Update's first-frame seed reads it
//   li  r3, 1
// (The prepare/release info is not touched.)
// ----------------------------------------------------------------------------
bool BehaviourSpirallingDeathcam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();            // stb 0, 8(this)
    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Update @0x8224A528 (DWARF cpp:127)
//
// Read off the pseudocode + the VMX ladder, in the console's order:
//   1. assert mpParameters; copy mAttachment (+0x2D4) into the collision policy's vehicle ref
//      (+0x240 == policy +0x220) -- the inlined SetVehicleRef;
//   2. `lbz 9(this)` (mbHasFailed) -> `ori 2` into the camera-state word at camera +0x140,
//      the same bit BehaviourRotateAboutVehicle::Update sets;
//   3. first frame (`lbz 8(this)` == 0): seed the orbit from the parameter block and the car
//      (HeightIncreaseSpeed <- +0x80, YRotationSpeedDegsPS <- +0x7C, RadiusIncreaseSpeed <-
//      +0x84, Height <- +0x88 x halfExtent.y, Radius <- +0x8C x |halfExtent| (vmsum3fp +
//      vrsqrtefp with two Newton steps, zero-guarded by vcmpeqfp/vsel), mAttachmentPos <- the
//      car's position), then `stb 1, 8(this)`;
//   4. the attach amount: max(mfMinAttachAmount, (1 - mfBlurAmount)^5) (`fsel f31, f12, f13,
//      f0` on f12 = min - decay), and mAttachmentPos += attach x (carPos - mAttachmentPos)
//      (vmaddcfp128);
//   5. the orbit position: angle = YRotationDegs x 0.017453292 (deg->rad); sin/cos through
//      the VMX polynomial (range reduction by round-to-nearest(angle / 2pi) x 2pi, then the
//      odd series over unk_82000BD0..BF0 = sin and the even series over unk_82000C00..C20 =
//      cos -- 1/9!, 1/17!, 1/8!, 1/16! are among the coefficients); the row (sin, 0, cos) x
//      Radius plus (0, Height, 0) plus mAttachmentPos is stored at camera +0x30 (the
//      transform's translation row);
//   6. Looker::Update(timestep vec, *mpRandom (by value), params +0x08, camera, car transform
//      (+0x1F0), car linear velocity (+0x330), car AABB (VehicleInfo +0x4A0));
//   7. CameraShake::Update(this+0x2C0, camera transform, params +0x6C, *mpRandom, timestep
//      scalar, mfShakeAmount);
//   8. camera +0x134 (DepthOfField::mfBlurriness) <- mfBlurAmount;
//   9. if mbStarted: the spiral advance (Height while mfRunningTime < mfHeightIncreaseDuration,
//      Radius, YRotationDegs, LookOffsetAmount, the rotation speed decay floored at
//      mfMinRotationSpeed, then the three amounts by dt / their times);
//  10. clamp the three amounts to [0,1]; camera +0xAC/+0xB0/+0xB4/+0xB5 <- clamp01(1 - attach)
//      / 1.0 / 1 / 1;
//  11. assert mpEffectInterface; EnsureEffectIsPlaying(camera, *it, "Player_Shutdown", 1.0);
//  12. mfRunningTime += timestep scalar; return true.
// ----------------------------------------------------------------------------
bool BehaviourSpirallingDeathcam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");                              // cpp:129
    const Parameters& lrParams = *mpParameters;

    // 1. the collision policy hangs off the same car (16-byte copy +0x2D4 -> +0x240).
    mCollisionPolicy.SetVehicleRef(mAttachment);

    // 2. the camera-state bit (`lbz 9(this)` then `ld/ori 2/std` at camera +0x140).
    if (!mbHasFailed)
    {
        lrCamera.mState_uFlags |= 2;
    }

    // 3. the first-frame seed (`lbz 8(this)` == 0).
    const VehicleInfo* lpVehicle = mAttachment.Get(lrInfo.mpAllVehicleData);
    if (!mbIsPrepared)
    {
        mVectorData.HeightIncreaseSpeed()  = lrParams.mfHeightIncreaseSpeed;         // +0x80 -> lane W
        mVectorData.YRotationSpeedDegsPS() = lrParams.mfRotationSpeedDegs;           // +0x7C -> lane Y
        mVectorData.RadiusIncreaseSpeed()  = lrParams.mfRadiusIncreaseSpeed;         // +0x84 -> lane Y

        const Vector3& lrHalfExtent = lpVehicle->mRaceCarState.mHalfExtent;          // car +0x350
        mVectorData.Height() = lrHalfExtent.y * lrParams.mfInitialHeight;            // +0x88 x extent.y

        // vmsum3fp128 + vrsqrtefp + two Newton steps, `vcmpeqfp v12, 0, |v|^2` / `vsel` -> a
        // zero-length extent yields 0 rather than 0 x inf.
        const f32 lfLengthSq = lrHalfExtent.x * lrHalfExtent.x
                             + lrHalfExtent.y * lrHalfExtent.y
                             + lrHalfExtent.z * lrHalfExtent.z;
        const f32 lfLength   = (lfLengthSq == 0.0f) ? 0.0f : std::sqrt(lfLengthSq);
        mVectorData.Radius() = lfLength * lrParams.mfInitialRadius;                  // +0x8C x |extent|

        mAttachmentPos = lpVehicle->mRaceCarState.mTransform.wAxis;                  // car +0x220
        SetPrepared();                                                               // stb 1, 8(this)
    }

    // 4. the attach amount and the lagged attachment point.
    const f32 lfOneMinusBlur = 1.0f - mfBlurAmount;
    const f32 lfDecay        = (((lfOneMinusBlur * lfOneMinusBlur) * lfOneMinusBlur)
                                * lfOneMinusBlur) * lfOneMinusBlur;
    const f32 lfAttach       = (lrParams.mfMinAttachAmount - lfDecay >= 0.0f)
                             ? lrParams.mfMinAttachAmount : lfDecay;                 // fsel f31
    {
        const Vector3& lrCarPos = lpVehicle->mRaceCarState.mTransform.wAxis;
        mAttachmentPos.x += lfAttach * (lrCarPos.x - mAttachmentPos.x);              // vmaddcfp128
        mAttachmentPos.y += lfAttach * (lrCarPos.y - mAttachmentPos.y);
        mAttachmentPos.z += lfAttach * (lrCarPos.z - mAttachmentPos.z);
        mAttachmentPos.w += lfAttach * (lrCarPos.w - mAttachmentPos.w);
    }

    // 5. the orbit position -> the camera's translation row.
    {
        const f32 lfAngleRads = mVectorData.YRotationDegs() * 0.017453292f;          // deg -> rad
        const f32 lfSin       = std::sin(lfAngleRads);                               // the VMX odd series
        const f32 lfCos       = std::cos(lfAngleRads);                               // the VMX even series
        Vector3 lPosition;
        lPosition.x = mVectorData.Radius() * lfSin + mAttachmentPos.x;               // row (sin, 0, cos) x r
        lPosition.y = mVectorData.Height()         + mAttachmentPos.y;               // + (0, h, 0)
        lPosition.z = mVectorData.Radius() * lfCos + mAttachmentPos.z;
        lPosition.w = mAttachmentPos.w;
        lrCamera.mTransform.wAxis = lPosition;                                       // stvx128 camera+0x30
    }

    // 6. look at the car.
    mLooker.Update(lrInfo.GetTimestep().GetVecFloat(GetTimestepType()),
                   *lrInfo.GetRandom(),
                   lrParams.mLookerParams,
                   lrCamera,
                   lpVehicle->mRaceCarState.mTransform,
                   lpVehicle->mRaceCarState.mLinearVelocity,
                   lpVehicle->mAABB);

    // 7. the shake, on this behaviour's timestep flavour, scaled by the shake amount.
    mShake.Update(lrCamera.mTransform,
                  lrParams.mShakeParams,
                  *lrInfo.GetRandom(),
                  lrInfo.GetTimestep(GetTimestepType()),
                  mfShakeAmount);

    // 8. the depth-of-field blur (camera +0x134).
    lrCamera.GetDepthOfField().SetBlurriness(mfBlurAmount);

    // 9. the spiral advance, once started.
    if (mbStarted)
    {
        const f32 lfDt = lrInfo.GetTimestep(GetTimestepType());
        if (mfRunningTime < lrParams.mfHeightIncreaseDuration)
        {
            mVectorData.Height() += mVectorData.HeightIncreaseSpeed() * lfDt;
        }
        mVectorData.Radius()        += mVectorData.RadiusIncreaseSpeed() * lfDt;
        mVectorData.YRotationDegs() += mVectorData.YRotationSpeedDegsPS() * lfDt;
        mVectorData.LookOffsetAmount() += (lfDt * lrParams.mfLookOffsetSpeed) * mfLookOffsetFactor;
        mVectorData.YRotationSpeedDegsPS() -= lfDt * lrParams.mfRotationSpeedDecreaseRate;
        if (mVectorData.YRotationSpeedDegsPS() < lrParams.mfMinRotationSpeed)          // vmaxfp
        {
            mVectorData.YRotationSpeedDegsPS() = lrParams.mfMinRotationSpeed;
        }
        mfLookOffsetFactor += lfDt / lrParams.mfTimeBeforeFullLookOffsetSpeed;
        mfBlurAmount       += lfDt / lrParams.mfBlurTime;
        mfShakeAmount      += lfDt / lrParams.mfShakeTime;
    }

    // 10. the clamps and the impact shake request.
    mfBlurAmount       = Clamp01(mfBlurAmount);
    mfShakeAmount      = Clamp01(mfShakeAmount);
    mfLookOffsetFactor = Clamp01(mfLookOffsetFactor);
    lrCamera.SetImpactShake(Clamp01(1.0f - lfAttach), 1.0f, 1);   // +0xAC / +0xB0 / +0xB4
    lrCamera.GetEffects().mu8BlendCurve = 1;                      // stb 1, camera +0xB5

    // 11. the screen hook.
    CGS_ASSERT(lrInfo.mpEffectInterface != 0, "lrSharedInfo.mpEffectInterface");    // cpp:210
    EnsureEffectIsPlaying(lrCamera, *lrInfo.mpEffectInterface, "Player_Shutdown", 1.0f);

    // 12. the clock.
    mfRunningTime += lrInfo.GetTimestep(GetTimestepType());
    return true;
}

// ----------------------------------------------------------------------------
// GetCollisionPolicy (DWARF cpp:225). No X360 export of its own: the body is the same
// `addi r3, r3, 0x20 ; blr` every sibling with its policy at +0x20 folds into.
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourSpirallingDeathcam::GetCollisionPolicy()
{
    return &mCollisionPolicy;
}

// @0x821FB600
const char* BehaviourSpirallingDeathcam::GetName() const
{
    return "BehaviourSpirallingDeathcam";
}

// SetupTweaker (DWARF cpp:266): [FLAG] no X360 export carries this override (folded or
// stripped), so the base's empty default stands; nothing is invented for it.

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Start @0x821F5620
//   lbz  r11, 0x2F4(this)      ; mbStarted
//   cmplwi r11, 0              ; assert it is clear (!mbStarted)
//   ... on failure: Begin/Fire/End assert ...
//   li   r11, 1
//   stb  r11, 0x2F4(this)      ; mbStarted = 1
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::Start()
{
    CGS_ASSERT(!mbStarted, "!mbStarted");   // lbz 0x2F4; assert clear
    mbStarted = true;                       // stb 1, 0x2F4(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::SetParameters @0x821F5680
//
// ArbStateCrashing::Prepare @0x822655E8 allocates the deathcam and immediately hands it the
// NamedParameters bank's deathcam block.
//   lwz    r11, 0(params)      ; lpParameters->GetType()
//   cmplwi r11, 0x13           ; == eBehaviourSpirallingDeathcam
//   ... assert "lpParameters->GetType() == eBehaviourSpirallingDeathcam" (h:193) ...
//   stw    r31, 0x2D0(this)    ; mpParameters = lpParameters
// The assert is a non-gating tripwire: the console stores the block either way.
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourSpirallingDeathcam,
               "lpParameters->GetType() == eBehaviourSpirallingDeathcam");   // h:193
    mpParameters = lpParameters;   // stw 0x2D0(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Parameters::Construct @0x821FB498
//
// Seed the authored block to its defaults. Store order in the asm is scheduler-shuffled; the
// displacements are what pin each value, and they are written here in ascending offset.
// The Looker sub-block is seeded by its own Construct (asm: the one `bl` in this function,
// on this+0x08); the shake quad is the project-wide 0.06 / 0.0 / 1.15 / 0.11 seed.
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::Parameters::Construct()
{
    meType        = eBehaviourSpirallingDeathcam;   // stw 0x13, 0(this)
    miBaseField04 = 0;                              // stw 0,    4(this)

    mLookerParams.Construct();                      // bl Looker::Parameters::Construct(this+8)
    mShakeParams.Construct();                       // stfs 0.06 / 0.0 / 1.15 / 0.11 @+0x6C..+0x78

    mfRotationSpeedDegs             = 60.0f;   // stfs flt_82004C6C, 0x7C(this)
    mfHeightIncreaseSpeed           =  0.5f;   // stfs flt_82001DA0, 0x80(this)
    mfRadiusIncreaseSpeed           =  0.0f;   // stfs flt_82001CC0, 0x84(this)
    mfInitialHeight                 =  0.5f;   // stfs flt_82001DA0, 0x88(this)
    mfInitialRadius                 =  1.5f;   // stfs flt_82004D04, 0x8C(this)
    mfTimeBeforeFullLookOffsetSpeed =  4.0f;   // stfs flt_82004EF4, 0x90(this)
    mfLookOffsetSpeed               =  1.0f;   // stfs flt_82001C98, 0x94(this)
    mfHeightIncreaseDuration        = 15.0f;   // stfs flt_820047C4, 0x98(this)
    mfRotationSpeedDecreaseRate     =  5.0f;   // stfs flt_8200426C, 0x9C(this)
    mfMinRotationSpeed              = 20.0f;   // stfs flt_820054CC, 0xA0(this)
    mfMinAttachAmount               =  0.8f;   // stfs flt_820054C8, 0xA4(this)
    mfBlurTime                      =  7.0f;   // stfs flt_820054D0, 0xA8(this)
    mfShakeTime                     =  7.0f;   // stfs flt_820054D0, 0xAC(this)
}

} // namespace Camera
} // namespace BrnDirector
