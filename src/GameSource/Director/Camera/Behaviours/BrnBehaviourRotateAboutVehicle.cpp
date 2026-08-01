// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.cpp
//
// Compilation home for the whole BrnDirector::Camera::BehaviourRotateAboutVehicle slice:
//   * GetCollisionPolicy  @0x821FB410  (inline in the header; anchored out-of-line below)
//   * GetName             @0x821FB488  (inline in the header)
//   * SetParameters       @0x821F55B8  (inline in the header -- its assert cites the .h)
//   * Parameters::Construct @0x821FB300 (inline in the header)
//   * Construct           @0x8222BEC0  (BODIED HERE, completed 2026-08-01)
//   * Update              @0x822493C0  (BODIED HERE, 2026-08-01 -- 995 asm lines)
//   * BecomeSimilarTo     @0x8224A350  (BODIED HERE, 2026-08-01)
// Three DWARF virtuals (Prepare / GetParameters / SetupTweaker) are absent from the X360
// export and are therefore NOT declared -- see the ⛔⛔ block in the header.
// ============================================================================

#include <cmath>                                 // sqrtf / sinf / cosf / fabsf

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"
#include "GameSource/Director/Camera/Camera.h"   // Camera::mTransform / mfFOV / RequestMotionBlur / SetFOV
#include "GameSource/Director/Camera/Utils/CameraUtils.h"  // CreateLookAt / CreateAdjustedLookAt /
                                                           //   GetSizeOnScreen / GetFOVDegsToFitObjectToScreenSize
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"  // Timestep::E_GAME
#include "rw/math/vpu/matrix44affine_operation.h"           // TransformVector / TransformPoint / Mult / SLerp
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::gpDebugPrint (bring-up measurement)

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// The six file-scope constants the DWARF names for this .cpp
// (BrnBehaviourRotateAboutVehicle.cpp:21..:32). Their VALUES are read out of the X360 .data
// block at 0x82CDA768..0x82CDA780, whose ADDRESS ORDER matches the DWARF's SOURCE-LINE ORDER
// one for one -- seven consecutive words, seven consecutive declarations. Every one is
// loaded by address somewhere in this TU's asm except KF_BLUR_SCALE (see its note).
// ----------------------------------------------------------------------------

// 0x82CDA768. The "close enough" band for the frame-to-frame hold at the end of Update: when
// both the transform and the FOV move by less than this, last frame's values are re-published
// instead. KVF_ (the DWARF's companion at cpp:22) is its VecFloat splat, lazily built into
// .bss @0x82FAA6E0 -- the same value, used for the per-lane vector compare.
const f32 KF_GENEROUS_EPSILON      = 0.0015f;

// 0x82CDA76C / 0x82CDA770. The pitch band handed to the free-look controller. NOTE THE SIGN:
// the minimum is -1 deg, i.e. the orbit is allowed barely below the car's horizontal.
const f32 KF_MIN_PITCH             = -1.0f;
const f32 KF_MAX_PITCH             = 40.0f;

// 0x82CDA774 / 0x82CDA778. The FOV eases toward the fitted value by these fractions -- once
// before the screen-offset adjust and once after. Identical values, two named constants.
const f32 KF_PRE_ADJUST_BLEND_RATE  = 0.1f;
const f32 KF_POST_ADJUST_BLEND_RATE = 0.1f;

// 0x82CDA77C. ⚠️ THE ONE CONSTANT WHOSE USE IS INFERRED, NOT READ. It has ZERO addressed
// readers anywhere in the X360 image; the two motion-blur amounts Update requests are loaded
// from the generic 1.0f literal pool entry (flt_82001C98) instead, because the compiler
// folded this file-scope const to its value. The attribution rests on three things: the
// DWARF places it in THIS file between KF_POST_ADJUST_BLEND_RATE and KF_STEP_BACK_DISTANCE
// (so it is at 0x82CDA77C), its value is exactly the amount requested, and a "blur scale" has
// no other candidate use in this behaviour. Using the name here is value-identical either way.
const f32 KF_BLUR_SCALE            = 1.0f;

// 0x82CDA780. How far past the car's bounding ellipsoid the eye is pushed -- as a MULTIPLE of
// the current car-to-eye offset, not a distance in metres (see Update's banner). With the eye
// seated on the ellipsoid, this is what turns a ~1-2.5 m half-extent into a sane orbit radius.
const f32 KF_STEP_BACK_DISTANCE    = 4.0f;

// Out-of-line anchor: forces the +0x50 accessor to be emitted in this TU.
// ⭐ RENAMED 2026-08-01: the accessor is GetCollisionPolicy(), and the "unrecoverable opaque
// sub-object" it returned is the embedded CollisionPolicyAttachedToVehicle -- pinned by
// BehaviourRotateAboutVehicle::Construct @0x8222BEDC, which calls that policy's Construct on
// this+0x50. See the header.
CollisionPolicyAttachedToVehicle*
BehaviourRotateAboutVehicle_GetCollisionPolicyAnchor(BehaviourRotateAboutVehicle& lrBehaviour)
{
    return lrBehaviour.GetCollisionPolicy();   // addi r3, r3, 0x50 ; blr
}

namespace
{
    // ------------------------------------------------------------------------
    // The two lane-wise primitives BecomeSimilarTo's VMX block reduces to.
    // rw::math::vpu::Vector3 carries NO operators (project rule), so the multiply-accumulate
    // that the console spells `vmaddfp` over a splatted scalar is written out by lane.
    // ------------------------------------------------------------------------

    // `vmulfp128 vD, splat(lfScale), lrColumn` -- the first term of an accumulation chain.
    rw::math::vpu::Vector3 ScaleVector(f32 lfScale, const rw::math::vpu::Vector3& lrColumn)
    {
        rw::math::vpu::Vector3 lResult;
        lResult.x = lfScale * lrColumn.x;
        lResult.y = lfScale * lrColumn.y;
        lResult.z = lfScale * lrColumn.z;
        lResult.w = lfScale * lrColumn.w;
        return lResult;
    }

    // `vmaddfp vD, splat(lfScale), vD, lrColumn` -- IDA prints vmaddfp in RAW FIELD ORDER
    // (VD, VA, VB, VC) and the semantic is VD = VA * VC + VB, so e.g.
    // `vmaddfp v0, v4, v0, v9` is v0 = v4 * v9 + v0: scale-by-splat then accumulate.
    void ScaleAndAdd(rw::math::vpu::Vector3& lrAccumulator, f32 lfScale,
                     const rw::math::vpu::Vector3& lrColumn)
    {
        lrAccumulator.x += lfScale * lrColumn.x;
        lrAccumulator.y += lfScale * lrColumn.y;
        lrAccumulator.z += lfScale * lrColumn.z;
        lrAccumulator.w += lfScale * lrColumn.w;
    }

    // `vcmpeqfp. v0, v0, v0` + the CR6 "all four lanes compared equal" bit: a float compares
    // unequal to ITSELF only when it is a NaN, so the self-compare is the console's NaN test.
    // (The three `stw r10, 0x70+var_20(r1)` in among the tests are dead spills of the bool
    // accumulator to its stack home, not logic.)
    bool IsNotNaN(f32 lfValue)
    {
        return lfValue == lfValue;
    }

    // NOTE -- `IsValid(...)`, the console's debug validity predicate that Update's five
    // asserts expand inline (a three-lane x==x self-compare per Vector3, ANDed over the four
    // rows for a matrix), is NOT redeclared here: rw::math::vpu already owns both overloads
    // (vector3_operation.h:265 / matrix44affine_operation.h:159) with exactly that body, and
    // ADL finds them. A local copy is an ambiguity, not a convenience.

    // ------------------------------------------------------------------------
    // Update's per-row anti-jitter comparison. The console does, for one row:
    //     diff = |lastRow - row|                      (vsubfp then vandc against 0x80000000)
    //     v9   = {diff.x, diff.y, diff.z, diff.x}     (vrlimi128 mask 1 shift 1 -- lane 3 is
    //                                                  overwritten with lane 0 so the w lane
    //                                                  cannot fail the 4-lane compare)
    //     take CR6[2] of `vcmpgtfp. v9, KVF_GENEROUS_EPSILON` == "NO lane was greater"
    // and ANDs the four rows' bits together (spelled nand/orc/andc over the four mfocrf
    // results, which reduces to r7 & r10 & r9 & r11).
    // ⚠️ NOTE IT IS `>` THAT IS TESTED, so the "within" answer is `!(diff > eps)`, i.e. a
    //   diff exactly EQUAL to the epsilon counts as within. Written as `<=` to match.
    // ------------------------------------------------------------------------
    bool IsWithinEpsilon(const rw::math::vpu::Vector3& lrLast,
                         const rw::math::vpu::Vector3& lrCurrent,
                         f32 lfEpsilon)
    {
        return fabsf(lrLast.x - lrCurrent.x) <= lfEpsilon
            && fabsf(lrLast.y - lrCurrent.y) <= lfEpsilon
            && fabsf(lrLast.z - lrCurrent.z) <= lfEpsilon;
    }

    // ------------------------------------------------------------------------
    // The two Xbox-360 xnamath rotation builders Construct and Update call by name
    // (`bl XMMatrixRotationX` / `bl XMMatrixRotationY`). They have no reconstructed home in
    // this tree, and they are library functions with fixed published semantics rather than
    // game code, so they are de-inlined here rather than invented: a right-handed rotation of
    // `lfAngleRads` about the axis, row-major, with an identity translation row -- which is
    // exactly the convention the consumers rely on (Construct takes RotationY's zAxis row and
    // gets (sin, 0, cos), matching the -22.5 deg seed the rodata pair holds).
    // DELETE-WHEN: an rw/math home for the rotation builders lands; these become calls to it.
    // ------------------------------------------------------------------------
    rw::math::vpu::Matrix44Affine XMMatrixRotationX(f32 lfAngleRads)
    {
        const f32 lfSin = sinf(lfAngleRads);
        const f32 lfCos = cosf(lfAngleRads);

        rw::math::vpu::Matrix44Affine lResult;
        lResult.xAxis.x = 1.0f;   lResult.xAxis.y = 0.0f;    lResult.xAxis.z = 0.0f;   lResult.xAxis.w = 0.0f;
        lResult.yAxis.x = 0.0f;   lResult.yAxis.y = lfCos;   lResult.yAxis.z = lfSin;  lResult.yAxis.w = 0.0f;
        lResult.zAxis.x = 0.0f;   lResult.zAxis.y = -lfSin;  lResult.zAxis.z = lfCos;  lResult.zAxis.w = 0.0f;
        lResult.wAxis.x = 0.0f;   lResult.wAxis.y = 0.0f;    lResult.wAxis.z = 0.0f;   lResult.wAxis.w = 0.0f;
        return lResult;
    }

    rw::math::vpu::Matrix44Affine XMMatrixRotationY(f32 lfAngleRads)
    {
        const f32 lfSin = sinf(lfAngleRads);
        const f32 lfCos = cosf(lfAngleRads);

        rw::math::vpu::Matrix44Affine lResult;
        lResult.xAxis.x = lfCos;  lResult.xAxis.y = 0.0f;    lResult.xAxis.z = -lfSin; lResult.xAxis.w = 0.0f;
        lResult.yAxis.x = 0.0f;   lResult.yAxis.y = 1.0f;    lResult.yAxis.z = 0.0f;   lResult.yAxis.w = 0.0f;
        lResult.zAxis.x = lfSin;  lResult.zAxis.y = 0.0f;    lResult.zAxis.z = lfCos;  lResult.zAxis.w = 0.0f;
        lResult.wAxis.x = 0.0f;   lResult.wAxis.y = 0.0f;    lResult.wAxis.z = 0.0f;   lResult.wAxis.w = 0.0f;
        return lResult;
    }

    // Degrees -> radians. The console spells it as a `fmuls` against flt_82001744.
    const f32 KF_DEGS_TO_RADS = 0.017453292f;

    // The yaw the free-look controller is overridden to while LOOKBACK is held
    // (`lfs f12, flt_820025FC` behind `lbz +0x20`). A half turn: the orbit swings to the
    // car's tail.
    const f32 KF_LOOKBACK_YAW_DEGS = 180.0f;

    // `Utils::ClampToSensibleFOVDegs` (declared CameraUtils.h:60) as Update inlines it:
    // `fsel`-max against 10 then `fsel`-min against 85, in that order.
    // ⚠️ ONE WITNESS for the band -- Update @0x82249E04..0x82249E24 is the only site in the
    //   camera domain that loads BOTH literals (flt_82004A20 = 10.0 and flt_8200A038 = 85.0;
    //   the other two readers of the 85.0 entry are BrnGui). So it is kept LOCAL here rather
    //   than used to body the shared CameraUtils symbol from a single site.
    //   Note the band is NOT the Looker parameters' mfMinFOV/mfMaxFOV (10.0/80.0 by default) --
    //   these are rodata literals, not parameter reads.
    f32 ClampToSensibleFOVDegs(f32 lfFOVDegs)
    {
        const f32 KF_MIN_SENSIBLE_FOV_DEGS = 10.0f;
        const f32 KF_MAX_SENSIBLE_FOV_DEGS = 85.0f;

        f32 lfClamped = (KF_MIN_SENSIBLE_FOV_DEGS >= lfFOVDegs) ? KF_MIN_SENSIBLE_FOV_DEGS : lfFOVDegs;
        lfClamped     = (KF_MAX_SENSIBLE_FOV_DEGS >= lfClamped) ? lfClamped : KF_MAX_SENSIBLE_FOV_DEGS;
        return lfClamped;
    }
}

// ============================================================================
// BehaviourRotateAboutVehicle::Construct @0x8222BEC0   (113 asm lines -- vtable slot 0)
//
// Seed the whole rig. BehaviourManager::BehaviourHelper::Prepare dispatches this on every
// freshly pooled behaviour, so it runs exactly once per allocation, on memory the pool's
// `new (slot) T()` has already value-initialised to zero.
//
// ⭐ COMPLETED 2026-08-01 (orbit-camera wave). The previous version stopped after the
// collision policy, the rotation controller, the vehicle ref and the orbit direction, behind
// a FLAG that said the rest of the block's field NAMES were unrecovered. They are all
// recovered now (see the header): the four post-Construct policy flag re-tunes, the Looker
// seed and the identity shake transform are the rest of this function.
//
// ---- asm walk (r31 = this, r30 = 0, f31 = 0.0f) ----------------------------
//   0x8222BEE0..0x8222BEF8  the six Behaviour base stores (== Behaviour::Construct, inlined)
//   0x8222BEDC/BEFC         addi r3, r31, 0x50 ; li r4, 0 ; bl CollisionPolicyAttachedToVehicle
//                             ::Construct                       -- mbDoVehicleCollision = false
//   0x8222BF14..0x8222BF54  stb 0,+0x298 / stb 1,+0x299 / stb 1,+0x29C / stb 1,+0x29D
//                             -- policy +0x248/+0x249/+0x24C/+0x24D, i.e. the four flag setters
//   0x8222BF5C..0x8222BF64  the +0x2A0 VehicleRef seeded to THE PLAYER CAR (0 / -1 / 0 / 1)
//   0x8222BF68..0x8222BF90  the ten mRotationController stores (== that class's own Construct)
//   0x8222BF98..0x8222BFAC  the six mLooker stores (== Utils::Looker::Construct, inlined)
//   0x8222BFCC/0x8222C04C   XMMatrixRotationY(-pi/2 * 0.5 * 0.5) -> its zAxis row into +0x360
//   0x8222BFD4..0x8222C070  an identity Matrix44Affine built on the stack and stored to +0x2E0
//   0x8222C074              stw 0, 0x374(r31)                    -- mpParameters = NULL
//
// ⚠️ THE ANGLE IS BUILT AT RUNTIME, NOT FOLDED: `vcfsx 0.5` twice against flt_82005560
//   (-1.5707964 == -pi/2), i.e. -pi/2 * 0.5 * 0.5 == -pi/8 == -22.5 degs. The old version
//   short-circuited this to the two literals its zAxis row produces (-0.38268343, 0,
//   0.92387953); the multiply chain is written out here because that is what the console does
//   and because the seed then reads better as what it is -- a 22.5-degree offset from the
//   car's own forward.
// ⚠️ mShake (+0x2D0) and mLastTransform (+0x320) are NOT written by Construct. That is
//   deliberate, not a gap: no store in the 113 lines lands in either range. The pool's
//   placement-new has already zeroed both, which for mShake is exactly what
//   CameraShake::Construct writes, and mLastTransform is latched by Update's own first frame.
// ============================================================================
void BehaviourRotateAboutVehicle::Construct()
{
    // The six base stores the console emits inline at the head.
    Behaviour::Construct();

    // `addi r3, r31, 0x50 ; li r4, 0` -- the embedded policy, with vehicle collision OFF.
    mCollisionPolicy.Construct(false);

    // The four flags this camera re-tunes once the policy's own Construct has returned. The
    // policy seeds mbAutoElevate = 1 and mbSmoothRadiusChanges = 0; this camera wants the
    // opposite of both (it orbits a parked car, so there is nothing to elevate over, and the
    // radius must ease rather than snap while the player spins the stick).
    mCollisionPolicy.SetAutoElevate(false);           // stb 0, 0x298(r31)  -- policy +0x248
    mCollisionPolicy.SetSmoothRadiusChanges(true);    // stb 1, 0x299(r31)  -- policy +0x249
    mCollisionPolicy.SetTestAgainstWorldOnly(true);   // stb 1, 0x29C(r31)  -- policy +0x24C
    mCollisionPolicy.SetUseFrustrumResolver(true);    // stb 1, 0x29D(r31)  -- policy +0x24D

    // stw 0,+0x2A0 / stw -1,+0x2A4 / stw 0,+0x2A8 / stb 1,+0x2AC. The E_PLAYER_CAR arm of
    // VehicleRef::Set forces miRaceCarIndex to -1 regardless of the argument, which is what
    // the console's `li r8, -1` store is; the argument is passed as INVALID to say so.
    mAttachedTo.Set(BrnDirector::VehicleRef::E_PLAYER_CAR,
                    E_ACTIVE_RACE_CAR_INDEX_INVALID, 0u);

    mRotationController.Construct();
    mLooker.Construct();

    // The un-rotated orbit seed: 22.5 degrees off the car's forward, taken as the "at" row of
    // a Y rotation. (Only the DIRECTION matters -- every reader normalises again.)
    mNormalisedOffsetDir = XMMatrixRotationY(-1.5707964f * 0.5f * 0.5f).zAxis;

    mLastShakeTransform.xAxis.x = 1.0f; mLastShakeTransform.xAxis.y = 0.0f;
    mLastShakeTransform.xAxis.z = 0.0f; mLastShakeTransform.xAxis.w = 0.0f;
    mLastShakeTransform.yAxis.x = 0.0f; mLastShakeTransform.yAxis.y = 1.0f;
    mLastShakeTransform.yAxis.z = 0.0f; mLastShakeTransform.yAxis.w = 0.0f;
    mLastShakeTransform.zAxis.x = 0.0f; mLastShakeTransform.zAxis.y = 0.0f;
    mLastShakeTransform.zAxis.z = 1.0f; mLastShakeTransform.zAxis.w = 0.0f;
    mLastShakeTransform.wAxis.x = 0.0f; mLastShakeTransform.wAxis.y = 0.0f;
    mLastShakeTransform.wAxis.z = 0.0f; mLastShakeTransform.wAxis.w = 0.0f;

    mpParameters = 0;
}

// ============================================================================
// BehaviourRotateAboutVehicle::Update @0x822493C0   (995 asm lines -- vtable slot 2)
//
// ⭐⭐ THE ORBIT-ABOUT-CAR CAMERA'S ENTIRE PER-FRAME JOB. Until 2026-08-01 this override was
// NOT DECLARED AT ALL, so `Behaviour::Update`'s `return true;` -- which never touches the
// camera -- was this behaviour's real Update, and the car-select camera published the
// identity-at-the-origin camera `Camera::Construct` left behind, every frame, since the
// behaviour first existed. Nothing about that is visible to the compiler, the linker or a
// boot test. See the class banner in the header.
//
// ---- the shot, in order ----------------------------------------------------
//  1. raise the camera's FOLLOW bit unless the behaviour has failed
//  2. advance the free-look controller from the shared info's stick, pitch-clamped to
//     [KF_MIN_PITCH, KF_MAX_PITCH]. ⚠️ IT IS DRIVEN PAUSED (`li r6, 1`) AND NOT-LOOKBACK
//     (`li r5, 0`) UNCONDITIONALLY -- see the note at the call.
//  3. take the un-rotated orbit seed, rotate it by pitch (X) then yaw (Y), then re-express it
//     in the frame CreateLookAt(mNormalisedOffsetDir, origin) -- i.e. about whatever direction
//     BecomeSimilarTo last re-seated the orbit to
//  4. ⭐ normalise that direction IN THE CAR'S HALF-EXTENT SPACE and scale back by the
//     half-extent, so the eye lands on the car's bounding ELLIPSOID rather than on a sphere;
//     transform it into the car's frame; then push out by KF_STEP_BACK_DISTANCE times the
//     current car-to-eye offset
//  5. look at the car; fit the FOV to the authored on-screen subject size; ease toward it by
//     KF_PRE_ADJUST_BLEND_RATE
//  6. re-aim through CreateAdjustedLookAt (the authored screen offset), re-fit the FOV, ease
//     by KF_POST_ADJUST_BLEND_RATE, clamp to the sensible band
//  7. request motion blur, advance the shake and SLerp it against last frame's by
//     mfShakeBlending0to1, then post-multiply it onto the camera
//  8. ⭐ THE ANTI-JITTER HOLD: if this frame's transform and FOV are both within
//     KF_GENEROUS_EPSILON of last frame's, republish LAST FRAME'S instead. Then latch both.
//
// ⚠️ mLooker IS NEVER USED. Update does not call Looker::Update -- it inlines its own
//   two-pass zoom instead (GetSizeOnScreen + GetFOVDegsToFitObjectToScreenSize, twice). The
//   member is constructed and then read by nothing. That is the console's own shape.
// ⚠️ NINE ASSERTS, all non-gating. Five are the IsValid family (cpp:136/171/183/201/234), two
//   are Camera::SetFOV's `lfFOV > 0.0f` (Camera.h:424) and one is Timestep::Get's bounds
//   check -- reproduced through the real callees rather than re-spelled here.
// ⛔ mpParameters IS DEREFERENCED FOUR TIMES AFTER A NON-GATING ASSERT. A null block is a
//   crash, not a diagnostic. MainDirector::BuildArbStateSharedInfo now publishes real
//   NamedParameters storage seeded by Parameters::Construct, so the block is non-null AND
//   carries the authored defaults -- both matter, because a ZEROED block would put
//   mfTargetSubjectXSize/YSize at 0 and hand a degenerate target size to the FOV fit.
// ============================================================================
bool BehaviourRotateAboutVehicle::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");                          // .cpp:136

    // ---- 1. the camera-state FOLLOW bit --------------------------------------
    // `lbz r11,9(this)` then `ld/ori 2/std` at camera +0x140. The same bit
    // ArbStateCarSelect clears when it parks the shot (KI_CAMERA_STATE_FOLLOW).
    if (!mbHasFailed)
    {
        lrCamera.mState_uFlags |= 2;
    }

    // ---- 2. the free-look controller ----------------------------------------
    // ⚠️ THE TWO BOOLEANS ARE CONSTANTS ON THE CONSOLE, NOT STATE: `li r5, 0` (lookback off)
    //   and `li r6, 1` (paused on), unconditionally. "Paused" selects the controller's
    //   integrate-the-velocity arm rather than its spring-back arm -- which is what a
    //   car-select orbit wants, because the player's stick input must PERSIST instead of
    //   easing back to the car's forward the moment they let go. It is not a bug and it is
    //   not a placeholder.
    // The stick comes from the SHARED INFO's own spherical controller (info +0x30), not from
    // this behaviour's; the timestep is the GAME flavour (info +0x588 == mTimestep +0x38).
    mRotationController.Update(lrInfo.GetTimestep(BrnDirector::Timestep::E_GAME),
                               lrInfo.mSphericalRotationController.GetRawStickVector(),
                               false,
                               true,
                               KF_MIN_PITCH,
                               KF_MAX_PITCH);

    // ---- 3. the orbit direction ---------------------------------------------
    // The un-rotated seed, built once into a function-local static (@0x82FAAD60, behind the
    // usual `dword_82FAAD70 & 1` init guard): the unit Z axis scaled by -sqrt(3).
    // ⚠️ THE MAGNITUDE IS PROVABLY DISCARDED -- step 4 normalises this vector before anything
    //   reads its length -- so only the SIGN and AXIS matter here. The expression that
    //   produced sqrt(3) is not recovered (the console materialises 1.0f and 2.0f with
    //   `vcfsx`, adds them and runs a vrsqrtefp+2x-Newton reciprocal-square-root, i.e.
    //   `Sqrt(1.0f + 2.0f)`); it is transcribed as that, literally, rather than guessed at.
    static const f32 KF_ORBIT_SEED_MAGNITUDE = sqrtf(1.0f + 2.0f);
    const Vector3 lSeedDirection = { 0.0f, 0.0f, -KF_ORBIT_SEED_MAGNITUDE, 0.0f };

    const bool lbLookback   = mRotationController.IsLookback();
    const f32  lfPitchDegs  = lbLookback ? 0.0f : mRotationController.GetPitchRotationAngleDegs();
    const f32  lfYawDegs    = lbLookback ? KF_LOOKBACK_YAW_DEGS
                                         : mRotationController.GetYawRotationAngleDegs();

    Vector3 lOffsetDirection =
        rw::math::vpu::TransformVector(XMMatrixRotationX(lfPitchDegs * KF_DEGS_TO_RADS),
                                       lSeedDirection);
    lOffsetDirection =
        rw::math::vpu::TransformVector(XMMatrixRotationY(lfYawDegs * KF_DEGS_TO_RADS),
                                       lOffsetDirection);

    // The frame the orbit is expressed in: a look-at built from the re-seated offset direction
    // against the origin, so only its ROTATION is meaningful (CreateLookAt normalises the eye
    // internally, which is why BecomeSimilarTo's un-normalised store is harmless).
    const Vector3 lOrigin = { 0.0f, 0.0f, 0.0f, 0.0f };
    lOffsetDirection =
        rw::math::vpu::TransformVector(Utils::CreateLookAt(mNormalisedOffsetDir, lOrigin),
                                       lOffsetDirection);

    // ---- 4. seat the eye on the car's bounding ellipsoid, then step back -----
    const VehicleInfo&    lrVehicle          = *mAttachedTo.Get(lrInfo.GetWorld());
    const Matrix44Affine& lrVehicleTransform = lrVehicle.mRaceCarState.mTransform;   // +0x1F0
    const Vector3&        lrHalfExtent       = lrVehicle.mRaceCarState.mHalfExtent;  // +0x350

    // [BRING-UP MEASUREMENT, physics wave 1] the READ end of the half-extent transfer. The
    // orbit seats the eye on the ellipsoid of THIS vector and then steps back four more radii,
    // so a wrong half-extent is a silently valid, silently wrong shot -- nothing asserts. The
    // WRITE end prints the same three lanes in BrnGameModule::BridgeWorldToDirector. Burst
    // window (first 4 reads, then every 3000th) rather than a modulo sample, so the print
    // cannot alias with a per-frame loop. Remove once the extent is settled.
    {
        static u32 suExtentReadCount = 0;
        ++suExtentReadCount;
        if (suExtentReadCount <= 4u || (suExtentReadCount % 3000u) == 0u)
        {
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[orbit-extent] read #" << static_cast<s32>(suExtentReadCount)
                    << " halfExtent (" << lrHalfExtent.x << ", " << lrHalfExtent.y
                    << ", " << lrHalfExtent.z << ") carPos ("
                    << lrVehicleTransform.wAxis.x << ", " << lrVehicleTransform.wAxis.y
                    << ", " << lrVehicleTransform.wAxis.z << ")\n";
            }
        }
    }

    // `vrefp128` + three Newton-Raphson refinements == the reciprocal of the half-extent,
    // folded here to the divide it converges to (the tree's rw vpu reconstruction precedent).
    const Vector3 lInEllipsoidSpace = { lOffsetDirection.x / lrHalfExtent.x,
                                        lOffsetDirection.y / lrHalfExtent.y,
                                        lOffsetDirection.z / lrHalfExtent.z,
                                        0.0f };

    // vmsum3fp128 + vrsqrtefp128 + two Newton-Raphson steps.
    const f32 lfLengthSquared    = lInEllipsoidSpace.x * lInEllipsoidSpace.x
                                 + lInEllipsoidSpace.y * lInEllipsoidSpace.y
                                 + lInEllipsoidSpace.z * lInEllipsoidSpace.z;
    const f32 lfReciprocalLength = 1.0f / sqrtf(lfLengthSquared);

    // ...normalised there, then scaled BACK by the same half-extent: the eye lands exactly on
    // the car's bounding ellipsoid in whichever direction the orbit currently points.
    const Vector3 lEllipsoidOffset = { lInEllipsoidSpace.x * lfReciprocalLength * lrHalfExtent.x,
                                       lInEllipsoidSpace.y * lfReciprocalLength * lrHalfExtent.y,
                                       lInEllipsoidSpace.z * lfReciprocalLength * lrHalfExtent.z,
                                       0.0f };

    const Vector3 lEyePosition = rw::math::vpu::TransformPoint(lrVehicleTransform,
                                                               lEllipsoidOffset);

    // `lMovedBackPos = eye - (carPos - eye) * KF_STEP_BACK_DISTANCE`. The bracket is the
    // eye-to-car vector, so this pushes the eye AWAY from the car by four times the ellipsoid
    // radius it is currently sitting at -- a five-radius orbit in total.
    const Vector3& lrVehiclePosition = lrVehicleTransform.wAxis;
    const Vector3  lMovedBackPos = {
        lEyePosition.x - (lrVehiclePosition.x - lEyePosition.x) * KF_STEP_BACK_DISTANCE,
        lEyePosition.y - (lrVehiclePosition.y - lEyePosition.y) * KF_STEP_BACK_DISTANCE,
        lEyePosition.z - (lrVehiclePosition.z - lEyePosition.z) * KF_STEP_BACK_DISTANCE,
        0.0f };

    CGS_ASSERT(IsValid(lMovedBackPos), "IsValid(lMovedBackPos)");                   // .cpp:171
    lrCamera.mTransform.wAxis = lMovedBackPos;   // stvx128 v127, r0, r19 (camera +0x30)

    // ---- 5. aim at the car and fit the FOV -----------------------------------
    const Matrix44Affine lLookAtCarTransform = Utils::CreateLookAt(lMovedBackPos,
                                                                  lrVehiclePosition);
    CGS_ASSERT(IsValid(lLookAtCarTransform), "IsValid(lLookAtCarTransform)");       // .cpp:183

    lrCamera.mTransform = lLookAtCarTransform;
    lrCamera.ValidateTransformWithDebugInfo();

    // The AABB is the SUBJECT car's, fetched through a second VehicleRef::Get -- the console
    // resolves the reference twice here rather than reusing lrVehicle, and both resolutions
    // are reproduced through the same named accessor.
    const Vector2 lSizeOnScreen =
        Utils::GetSizeOnScreen(lrCamera.mTransform,
                               lrCamera.mfFOV,
                               lrCamera.mfAspectRatio,
                               mAttachedTo.Get(lrInfo.GetWorld())->mRaceCarState.mTransform,
                               mAttachedTo.Get(lrInfo.GetWorld())->mAABB);

    const Vector2 lTargetScreenSize = { mpParameters->mLookerParams.mfTargetSubjectXSize,
                                        mpParameters->mLookerParams.mfTargetSubjectYSize,
                                        0.0f, 0.0f };

    const f32 lfFittedFOVDegs = Utils::GetFOVDegsToFitObjectToScreenSize(lrCamera.mfFOV,
                                                                        lSizeOnScreen,
                                                                        lTargetScreenSize);

    // `fsubs` then `fmadds` -- ease toward the fitted FOV, do not snap to it.
    lrCamera.SetFOV((lfFittedFOVDegs - lrCamera.mfFOV) * KF_PRE_ADJUST_BLEND_RATE
                    + lrCamera.mfFOV);

    // ---- 6. apply the authored screen offset and re-fit -----------------------
    const Vector2 lTargetScreenOffset = {
        mpParameters->mLookerParams.mfTargetSubjectXScreenOffset,
        mpParameters->mLookerParams.mfTargetSubjectYScreenOffset,
        0.0f, 0.0f };

    const Matrix44Affine lAdjustedLookAt = Utils::CreateAdjustedLookAt(lrCamera.mTransform,
                                                                      lrCamera.mfFOV,
                                                                      lrCamera.mfAspectRatio,
                                                                      lTargetScreenOffset);
    CGS_ASSERT(IsValid(lAdjustedLookAt), "IsValid(lAdjustedLookAt)");               // .cpp:201

    lrCamera.mTransform = lAdjustedLookAt;
    lrCamera.ValidateTransformWithDebugInfo();

    const Vector2 lAdjustedSizeOnScreen =
        Utils::GetSizeOnScreen(lrCamera.mTransform,
                               lrCamera.mfFOV,
                               lrCamera.mfAspectRatio,
                               mAttachedTo.Get(lrInfo.GetWorld())->mRaceCarState.mTransform,
                               mAttachedTo.Get(lrInfo.GetWorld())->mAABB);

    const f32 lfRefittedFOVDegs =
        Utils::GetFOVDegsToFitObjectToScreenSize(lrCamera.mfFOV,
                                                 lAdjustedSizeOnScreen,
                                                 lTargetScreenSize);

    const f32 lfBlendedFOVDegs = (lfRefittedFOVDegs - lrCamera.mfFOV) * KF_POST_ADJUST_BLEND_RATE
                               + lrCamera.mfFOV;
    lrCamera.SetFOV(lfBlendedFOVDegs);
    lrCamera.SetFOV(ClampToSensibleFOVDegs(lfBlendedFOVDegs));

    // ---- 7. motion blur, then the shake ---------------------------------------
    lrCamera.RequestMotionBlur(KF_BLUR_SCALE, KF_BLUR_SCALE);

    // The shake is computed into a fresh IDENTITY affine, not onto the camera: the console
    // builds one on its stack from gIVector / (0,1,0,0) / (0,0,1,0) / zero and hands its
    // address to CameraShake::Update.
    Matrix44Affine lShakeTransform;
    lShakeTransform.xAxis.x = 1.0f; lShakeTransform.xAxis.y = 0.0f;
    lShakeTransform.xAxis.z = 0.0f; lShakeTransform.xAxis.w = 0.0f;
    lShakeTransform.yAxis.x = 0.0f; lShakeTransform.yAxis.y = 1.0f;
    lShakeTransform.yAxis.z = 0.0f; lShakeTransform.yAxis.w = 0.0f;
    lShakeTransform.zAxis.x = 0.0f; lShakeTransform.zAxis.y = 0.0f;
    lShakeTransform.zAxis.z = 1.0f; lShakeTransform.zAxis.w = 0.0f;
    lShakeTransform.wAxis.x = 0.0f; lShakeTransform.wAxis.y = 0.0f;
    lShakeTransform.wAxis.z = 0.0f; lShakeTransform.wAxis.w = 0.0f;

    // ⚠️ THE SHAKE RUNS ON *THIS BEHAVIOUR'S DECLARED* TIMESTEP FLAVOUR (`lwz r29, 4(this)`
    //   then `lfsx f1, (r29 + 0x160)*4, info`), not on the E_GAME one step 2 used. That is
    //   Timestep::Get(meTimestepType), and it is where the console's ninth assert comes from.
    mShake.Update(lShakeTransform,
                  mpParameters->mShakeParams,
                  *lrInfo.GetRandom(),
                  lrInfo.GetTimestep(GetTimestepType()),
                  1.0f);

    // Ease this frame's shake against last frame's, then fold the result onto the camera.
    // A blend of 0 (which is what Parameters::Construct's default gives) holds the identity,
    // i.e. NO SHAKE -- the authored bank is what turns it on.
    Vector3 lUnusedAngle = { 0.0f, 0.0f, 0.0f, 0.0f };
    mLastShakeTransform = rw::math::vpu::SLerp(mLastShakeTransform,
                                               lShakeTransform,
                                               mpParameters->mfShakeBlending0to1,
                                               &lUnusedAngle);

    const Matrix44Affine lShakenCamera = rw::math::vpu::Mult(mLastShakeTransform,
                                                             lrCamera.mTransform);
    CGS_ASSERT(IsValid(lShakenCamera), "IsValid(lShakenCamera)");                   // .cpp:234

    lrCamera.mTransform = lShakenCamera;
    lrCamera.ValidateTransformWithDebugInfo();

    // ---- 8. the anti-jitter hold ---------------------------------------------
    // First frame: adopt what we just produced as "last frame" so the comparison below has
    // something real to compare against. `lbz r11,8(this)` -- the base's mbIsPrepared is used
    // here as this behaviour's own first-frame latch (Behaviour::Construct zeroes it and
    // nothing else on this path raises it).
    if (!mbIsPrepared)
    {
        mLastTransform = lrCamera.mTransform;
        mbIsPrepared   = true;
        mfLastFOV      = lrCamera.mfFOV;
    }

    // Hold the FOV when it barely moved. NOTE THE DIRECTION OF THE TEST: SetFOV is called
    // when the delta is BELOW the epsilon (`blt` skips the "false" store), i.e. the small
    // change is the one that gets thrown away.
    if (fabsf(mfLastFOV - lrCamera.mfFOV) < KF_GENEROUS_EPSILON)
    {
        lrCamera.SetFOV(mfLastFOV);
    }

    // ...and the same for the transform, per row, per lane. The console runs four
    // `vcmpgtfp.` over |lastRow - row| against the KVF_GENEROUS_EPSILON splat and combines
    // the four "no lane exceeded" bits with nand/orc/andc -- which is a plain four-way AND.
    if (IsWithinEpsilon(mLastTransform.xAxis, lrCamera.mTransform.xAxis, KF_GENEROUS_EPSILON)
     && IsWithinEpsilon(mLastTransform.yAxis, lrCamera.mTransform.yAxis, KF_GENEROUS_EPSILON)
     && IsWithinEpsilon(mLastTransform.zAxis, lrCamera.mTransform.zAxis, KF_GENEROUS_EPSILON)
     && IsWithinEpsilon(mLastTransform.wAxis, lrCamera.mTransform.wAxis, KF_GENEROUS_EPSILON))
    {
        lrCamera.mTransform = mLastTransform;
        lrCamera.ValidateTransformWithDebugInfo();
    }

    mfLastFOV      = lrCamera.mfFOV;
    mLastTransform = lrCamera.mTransform;

    return true;   // `li r3, 1`
}

// ============================================================================
// BehaviourRotateAboutVehicle::BecomeSimilarTo @0x8224A350   (116 asm lines)
//
// Re-seat the orbit-about-car camera so that it picks up where ANOTHER behaviour's camera
// currently is: take that camera's world position, express it in the anchored car's local
// frame, flatten it onto the car's horizontal plane, and adopt that as the orbit direction.
// Then wipe the free-look rotation state so the re-seated orbit starts un-rotated.
// This is the call the junkyard / car-select states make every frame while an ICE movie is
// driving the shot, so that the later interpolation ONTO the car has no discontinuity
// (BrnArbStateCarSelect::Update @0x8226F5D0, four PC call sites).
//
// ---- asm walk (r31 = this, r30 = the source camera) ------------------------
//   0x8224A370  addi r3, r31, 0x2A0 ; bl VehicleRef::Get      -> r3 = the VehicleInfo
//   0x8224A37C  addi r11, r3, 0x1F0                            -> &mRaceCarState.mTransform
//   0x8224A38C  lvx128 v0, r30, 0x30    the SOURCE CAMERA's mTransform.wAxis (its position)
//   0x8224A39C  lvx128 v11, r11, 0      \
//   0x8224A3A8  lvx128 v10, r11, 0x10    | the vehicle's world basis + position
//   0x8224A3B0  lvx128 v12, r11, 0x20    |
//   0x8224A390  lvx128  v9, r11, 0x30   /
//   0x8224A3A4  vsubfp v0, v13, v9                             -> -vehiclePos  (v13 == 0)
//   0x8224A3B4..0x8224A3D8   SIX vmrghw/vmrglw == the 4x4 (really 3x3) TRANSPOSE:
//                            v12 = {x.x, y.x, z.x, 0}   (column 0)
//                            v9  = {x.y, y.y, z.y, 0}   (column 1)
//                            v10 = {x.z, y.z, z.z, 0}   (column 2)
//   0x8224A3E8..0x8224A3F0   v0 = col2*(-pos).z + col1*(-pos).y + col0*(-pos).x
//   0x8224A3F8..0x8224A404   v0 += col0*cam.x + col1*cam.y + col2*cam.z
//                            => v0 = {dot(cam-pos, xAxis), dot(.., yAxis), dot(.., zAxis)}
//   0x8224A410..0x8224A41C   vperm(mask @0x82CDA350) + vrlimi128 lane 2 == the standard rw
//                            `Vector3(x, y, z)` construction codegen: Vector3(local.x, 0, local.z)
//   0x8224A420..0x8224A450   vmsum3fp128 dot3 + vrsqrtefp + TWO Newton-Raphson refinements,
//                            then v11 = flat * rsqrt(|flat|^2)   -- the NORMALISE
//   0x8224A454..0x8224A4A4   three `vcmpeqfp. vN,vN,vN` self-compares on lanes 0/1/2 of the
//                            NORMALISED vector, short-circuiting to false on the first NaN
//   0x8224A4B8  stvx128 v12, r31, 0x360     <-- stores v12, the UN-normalised flat vector
//   0x8224A4C4  else: mNormalisedOffsetDir = *(Vector3*)0x82181520 == (0, 0, 1, 0)
//   0x8224A4D8..0x8224A508  the ten-store reset of mRotationController (see the header)
//
// ⚠️⚠️ SHIPPED-CONSOLE QUIRK, REPRODUCED VERBATIM -- READ BEFORE "FIXING" THIS.
//   The console computes the normalised direction into v11, validates THAT, and then stores
//   v12 -- the UN-normalised flattened offset, whose length is the current orbit RADIUS (tens
//   of metres). The value it falls back to when the test fails is the exact unit vector
//   (0, 0, 1), and Construct @0x8222C04C seeds this same member from XMMatrixRotationY's
//   at-row, which is also unit. So the two "authored" seeds are unit and the re-seat path's is
//   not: the normalise result is computed, tested, and then discarded. This is a real defect in
//   the shipped X360 code, not a transcription gap -- v12 is provably untouched between the
//   `vmulfp128 v11, v12, v0` that produces the normalised copy and the `stvx128 v12` that
//   stores the raw one.
//   IT IS BENIGN TODAY, which is why it shipped: the ONLY consumer is Update @0x822495E0,
//   which passes mNormalisedOffsetDir to Utils::CreateLookAt as the eye against a zero target and
//   then reads back only rows 0..2 of the result (the ROTATION). CreateLookAt normalises
//   internally, so the magnitude is discarded before anything uses it.
//   DO NOT "correct" this to store the normalised vector: it would change nothing visible and
//   would break parity. If a future reader of mNormalisedOffsetDir needs a unit vector, normalise
//   AT THAT READER and note it there.
//
// ⚠️ mRotationController.Construct() IS AN EMPTY STUB TODAY (DirectorLinkStubs.cpp:522), and
//   this call site is the first one on a LIVE path. See the note at the call below.
// ============================================================================
void BehaviourRotateAboutVehicle::BecomeSimilarTo(const Camera& lrSourceCamera,
                                                  const AllVehicleData& lrAllVehicleData)
{
    // ---- the anchored car's world space -------------------------------------
    const VehicleInfo&    lrVehicle      = *mAttachedTo.Get(&lrAllVehicleData);
    const Matrix44Affine& lrVehicleSpace = lrVehicle.mRaceCarState.mTransform;   // vehicle +0x1F0

    // ---- the 3x3 transpose (== the inverse rotation; the basis is orthonormal) ----
    // Columns of the vehicle basis, i.e. the rows of its transpose. Lane 3 is zero in all
    // three (the six vmrghw/vmrglw merge the basis rows against a zeroed register), which is
    // what keeps the w lane out of the accumulation below.
    const Vector3 lColumn0 = { lrVehicleSpace.xAxis.x, lrVehicleSpace.yAxis.x, lrVehicleSpace.zAxis.x, 0.0f };
    const Vector3 lColumn1 = { lrVehicleSpace.xAxis.y, lrVehicleSpace.yAxis.y, lrVehicleSpace.zAxis.y, 0.0f };
    const Vector3 lColumn2 = { lrVehicleSpace.xAxis.z, lrVehicleSpace.yAxis.z, lrVehicleSpace.zAxis.z, 0.0f };

    const Vector3& lrVehiclePosition = lrVehicleSpace.wAxis;
    const Vector3& lrCameraPosition  = lrSourceCamera.mTransform.wAxis;   // camera +0x30

    // ---- the source camera's world position, expressed in the car's local frame ----
    // Kept as the console's TWO accumulation groups (inverse translation first, then the
    // camera term) rather than folded to `transpose * (camera - vehicle)`, so the order of
    // operations matches the asm.
    Vector3 lLocalOffset = ScaleVector(-lrVehiclePosition.z, lColumn2);   // vmulfp128 v0, v8, v10
    ScaleAndAdd(lLocalOffset, -lrVehiclePosition.y, lColumn1);            // vmaddfp   v0, v4, v0, v9
    ScaleAndAdd(lLocalOffset, -lrVehiclePosition.x, lColumn0);            // vmaddfp   v0, v11, v0, v12
    ScaleAndAdd(lLocalOffset,  lrCameraPosition.x,  lColumn0);            // vmaddfp   v0, v12, v0, v7
    ScaleAndAdd(lLocalOffset,  lrCameraPosition.y,  lColumn1);            // vmaddfp   v0, v9, v0, v6
    ScaleAndAdd(lLocalOffset,  lrCameraPosition.z,  lColumn2);            // vmaddfp   v0, v10, v0, v5

    // ---- flatten onto the car's horizontal plane ----------------------------
    // Lane 1 of the local offset is the component along the car's UP axis; dropping it keeps
    // the orbit level with the car. The console leaves lLocalOffset.x in the w lane (the
    // vperm mask @0x82CDA350 broadcasts lane 0 into lanes 0/2/3 before the vrlimi128 overwrites
    // lane 2) -- that is Vector3's undefined 4th lane, written 0.0f here to match the tree's
    // established Vector3(x,y,z) reconstruction (Camera::CreateHeadingSpaceLookAt uses the
    // identical codegen with the identical mask).
    const Vector3 lFlatOffset = { lLocalOffset.x, 0.0f, lLocalOffset.z, 0.0f };

    // ---- normalise, purely to detect the degenerate case --------------------
    // The console runs vrsqrtefp + two Newton-Raphson steps, which converges to the true
    // reciprocal square root; folded to the scalar divide per the rw vpu reconstruction
    // precedent. The DEGENERATE behaviour is preserved: a zero-length offset (the camera
    // exactly above/below the car) yields an infinite scale, and the y lane's 0 * inf is a NaN
    // on both targets, so the test below fails identically.
    const f32 lfLengthSquared = lFlatOffset.x * lFlatOffset.x
                              + lFlatOffset.y * lFlatOffset.y
                              + lFlatOffset.z * lFlatOffset.z;         // vmsum3fp128 v0, v12, v12
    const f32 lfReciprocalLength = 1.0f / sqrtf(lfLengthSquared);

    const Vector3 lNormalised = { lFlatOffset.x * lfReciprocalLength,   // vmulfp128 v11, v12, v0
                                  lFlatOffset.y * lfReciprocalLength,
                                  lFlatOffset.z * lfReciprocalLength,
                                  lFlatOffset.w * lfReciprocalLength };

    if (IsNotNaN(lNormalised.x) && IsNotNaN(lNormalised.y) && IsNotNaN(lNormalised.z))
    {
        // ⚠️ the RAW flattened offset, NOT lNormalised -- see the SHIPPED-CONSOLE QUIRK banner.
        mNormalisedOffsetDir = lFlatOffset;                                 // stvx128 v12, r31, 0x360
    }
    else
    {
        mNormalisedOffsetDir = { 0.0f, 0.0f, 1.0f, 0.0f };                  // .rdata @0x82181520
    }

    // ---- wipe the free-look rotation state ----------------------------------
    // ⚠️ SILENT-DROP STUB ON A LIVE PATH: CameraSphericalRotationController::Construct is an
    // EMPTY body in GameSource/Director/DirectorLinkStubs.cpp:522. That was argued safe because
    // its only other caller (BehaviourGameplayExternal::Construct) runs on a freshly
    // placement-new'd, zero-initialised object -- but THIS caller does not. BecomeSimilarTo runs
    // on a behaviour that has been live for many frames, and the whole point of these ten stores
    // is to throw away the accumulated stick yaw / pitch / lookback state so the re-seated orbit
    // starts neutral. With the empty stub the stale rotation survives the re-seat.
    // The real body IS fully attested (three witnesses, spelled out in the header's layout
    // note) -- it just cannot be written from this file, because DirectorLinkStubs.cpp owns the
    // symbol and an inline in the canonical header would collide with it.
    mRotationController.Construct();   // stvx128 0,+0x20 / +0x30..+0x42 / +0x48 / +0x4C
}

} // namespace Camera
} // namespace BrnDirector
