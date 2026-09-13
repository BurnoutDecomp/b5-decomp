#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert + the rig asserts)
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Vector3 (SetWorldSpaceNormalizedVectorFromCar)
#include "rw/math/vpu/vector3_operation.h"            // MagnitudeSquared (the IsSimilar magnitude assert)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"   // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"     // VisibilityCollisionPolicy /
                                                               //   CollisionPolicyAttachedToVehicle (the two
                                                               //   sub-objects GetCollisionPolicy hands back)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h
//
// BrnDirector::Camera::BehaviourGyroCam -- the "gyro" follow-camera behaviour: it holds the
// camera a fixed height/distance/pitch off a tracked car along a world-space vector from the
// car, smoothing the rig with a PositionLag and a CameraShake and optionally an attachment
// "truck" that eases the offset distance in.
//
// ----------------------------------------------------------------------------
// RE-BASED (2026-09-13). This class used to model its base head as an opaque
// `u8 maHead000[0x9]` slice with the base's flag bytes (mbHasFailed +0x009,
// mbCanSwitchToMeNow +0x00B, mbCanSwitchFromMeNow +0x00C) hand-placed after it, and it had NO
// vtable of its own. It now derives the canonical BrnDirector::Camera::Behaviour and carries
// the declaration reference member list by name.
//
// WHY IT HAD TO BE RE-BASED: BehaviourManager::AllocateBehaviour<BehaviourGyroCam> hands a raw
// pool slot to AbstractPool::AllocateVoid<T>, which placement-news the behaviour into it;
// BehaviourHelper::Prepare then dispatches vtable slot 0 (Construct). For a NON-POLYMORPHIC
// class placement-new installs no vtable, so that dispatch read a null vptr -- the access
// violation that killed the game on the FIRST TAKEDOWN (ArbStateTakedown::Prepare ->
// NewBehaviour<> -> BehaviourHelper::Prepare, reading address 0).
//
// WHAT THE RE-BASE DELETED (the base now provides each by name):
//   maHead000 / mHead00A (the raw head bytes)   -> Behaviour's vptr + meTimestepType + mbIsPrepared
//                                                  + mbTweakerAttached
//   mbHasFailed / HasFailed()                   -> Behaviour::mbHasFailed / Behaviour::HasFailed()
//   mbCanSwitchToMeNow / CanSwitchToMeNow()     -> Behaviour::CanSwitchToMeNow()
//   mbCanSwitchFromMeNow / CanSwitchFromMeNow() -> Behaviour::CanSwitchFromMeNow()
// MomentHitTraffic::Update (HasFailed) and MomentTumbling::Update (the two switch gates) call
// the SAME names with the same signatures, so no consumer changes meaning.
//
// VTABLE ORDER -- proven from this class's vtable in the image (EIGHT words, then string data):
//   0 Construct                            4 Release              (the shared base default)
//   1 Prepare                              5 GetCollisionPolicy
//   2 Update                               6 SetupTweaker
//   3 PostCollisionUpdate  (base default)  7 GetName
// i.e. exactly the base's eight-slot order, with slots 3 and 4 left at the base defaults -- the
// declaration reference's own method list for this class agrees (it names Construct, Prepare,
// Update, GetCollisionPolicy, SetupTweaker, GetName and nothing else). The class introduces NO
// extra virtuals, so its table is eight words, not ten.
//
// FLAG (not transcribed -- a DOCUMENTED GAP, not a fabrication): slot 2 (Update, the ~500-line
//   rig that drives the camera off the tracked car) and slot 6 (SetupTweaker) are NOT declared
//   here, so both keep the base's defaults (Update returns true and leaves the camera
//   untouched; SetupTweaker does nothing). Slot 0 (Construct) IS declared, but only PARTIALLY
//   transcribed -- see the gate on its body below.
//   DELETE-WHEN: the gyro rig TU lands.
//
// LAYOUT AUTHORITY: the declaration reference member list for this class -- mpParameters,
// mVisibilityCollisionPolicy, mVehicleAttachmentCollisionPolicy, mTransform, mLooker,
// mAttachedTo, mCurrentTargetPos, mWorldSpaceNormalizedVectorFromCar,
// mDesiredWorldSpaceNormalizedVectorFromCar, mOriginalPoint, mPositionLag, mRandom, mShake,
// mAttachmentTruck, mfHeight/mfDistance/mfPitch/mfBlendFactor, then the four bools
// mbIsPlanted / mbIsFirstFrameOfPlanted / mbIsWorldSpaceVectorSet /
// mbUseVehicleAttachmentCollision -- with EVERY offset below independently re-derived from the
// console asm of Construct, Prepare, SetParameters, AttachToRaceCar, GetCollisionPolicy and
// SetWorldSpaceNormalizedVectorFromCar.
//
// x64: the console offsets in the comments are PROVENANCE ONLY -- parity here is BY NAMED
// MEMBER (the project's x64 rule). The reserved spans are the rig members that have no home
// type yet; they hold the members after them in the console's ORDER, and nothing casts them.
//
// FLAG (pool routing): the console's sizeof(BehaviourGyroCam) is 1600, which exactly fills the
//   behaviour manager's SMALL pool bucket. On the host the Behaviour base is 32 bytes (vs the
//   console's 16) and the parameter pointer widens, so the class measures 1632 here and no
//   longer fits that bucket: BehaviourManager::AllocateBehaviour<> routes it to the 8-slot
//   LARGE pool (bucket 4000) instead. That is a host-width consequence, not a reconstruction
//   choice -- trimming a reserved span to buy the 32 bytes back would be a layout
//   accommodation. The large pool has room: the shared gameplay cameras, the failsafe and the
//   ICE-anim behaviour are its only other tenants, and at most two gyro cams are live at once.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   gyro-cam one. The console value for eBehaviourGyroCam is 9 (the asm compares the block's
//   first word against 9). Replace with the real EBehaviourType enum when one is homed; the
//   gyro-cam enumerator's VALUE (9) is pinned from the asm.
enum EBehaviourTypeGyroCam
{
    eBehaviourGyroCam = 9
};

// FLAG: the upper bound the AttachToRaceCar index assert enforces. The console value for
//   BrnPhysics::Vehicle::ku8MaxNumRaceCars is 8 (the asm compares the race-car index against
//   8). Replace with the real BrnPhysics::Vehicle constant when that TU lands; the VALUE (8)
//   is asm.
// Guarded: BrnBehaviourBystanderCam.h / BrnBehaviourLooseAttachment.h each independently
// (re)declare this same identically-valued unnamed enum in this namespace (a latent ODR risk
// that only surfaces once a single TU includes more than one of them -- as BrnArbStateTakedown.cpp
// now does for GyroCam + LooseAttachment). The guard makes the second inclusion a no-op instead
// of a redefinition error; each sibling header carries the identical guard.
#ifndef BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
#define BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
enum { KU_MAX_NUM_RACE_CARS = 8 };
#endif

class BehaviourGyroCam : public Behaviour
{
public:

    // The gyro-cam parameter block (a behaviour parameter block with the gyro-cam type tag).
    // GetType returns the tag SetParameters asserts on.
    //
    // ⚠️ THIS BLOCK IS DELIBERATELY **NOT** RE-BASED ONTO Behaviour::Parameters, unlike the
    //   behaviour itself. Its head is a 4-byte type tag plus a 4-byte debug-name slot on the
    //   console, and the parameter bank pins this record BYTE-EXACT at 204 bytes on a 204-byte
    //   grid (BrnBehaviourParameterBank.h) while the serialiser pins its interior offsets
    //   (+0x08 / +0x2C / +0x90 / +0x98). Deriving Behaviour::Parameters would widen the
    //   debug-name slot to a real 8-byte pointer and shift every one of those pins.
    //
    // Serialised layout pinned store-for-store from the three Serialise<S> visitor bodies
    //   (debug-menu, read, write): after the type-tag header
    //   (meType/miParamWord1, NOT serialised) the block nests three sub-Parameters blocks -- the
    //   shake tunings @+0x08 ("Shake Params"), the looker tunings @+0x2C ("Looker Params") and the
    //   attachment-truck tunings @+0x90 ("Attachment truck") -- then twelve f32 tunables @+0x98..+0xC4
    //   and four bool flags @+0xC8..+0xCB. Offsets are the a1+OFF displacements the DebugMenu asm
    //   passes to Process<float>/Process<bool> and the read/write asm loads/stores. The whole block
    //   is pointer-free (all sub-blocks are f32/bool/enum aggregates), so every offset is host-
    //   pointer-width invariant and pinned by static_assert in BrnBehaviourGyroCamSerialise.cpp.
    //   The +0x18..+0x2B span between the shake block and the looker block is un-serialised rig
    //   state (reserved here).
    //
    // The three embedded sub-blocks are modelled as size-exact 4-byte-aligned raw storage. Their
    //   canonical types -- Utils::CameraShake::Parameters (16B), Utils::Looker::Parameters (100B) and
    //   AttachmentTruck::Parameters (8B) -- live in the heavyweight BehaviourRig.h / BrnLooker.h /
    //   BrnAttachmentTruck.h; pulling those into this widely-included behaviour header would drag
    //   BehaviourRig.h's inline Tweaker slice into the behaviour-manager TUs that already include the
    //   canonical BrnCameraTweaker.h and ODR-clash. The visitor body in BrnBehaviourGyroCamSerialise
    //   .cpp reinterpret_casts each storage span to its canonical sub-Parameters type BY NAME before
    //   walking it -- faithful to the console which passes a1+8 / a1+0x2C / a1+0x90 straight to each
    //   sub-block's Serialise as that typed pointer. u32 storage guarantees the 4-byte alignment
    //   the casts need.
    class Parameters
    {
    public:
        // console visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (DebugMenuSerialiser / TextFile{Read,Write}Serialiser); the per-instance body
        // is BrnBehaviourGyroCamSerialise.cpp (ONE templated body + one explicit instantiation per S).
        // Declared so the serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeGyroCam GetType() const
        {
            return static_cast<EBehaviourTypeGyroCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  the block's debug-name slot (a 4-byte pointer on the console);
                           //        cached into the behaviour's +0x10 word by SetParameters

        // --- embedded serialised sub-blocks: size-exact 4-byte-aligned raw storage, cast to the
        //     canonical sub-Parameters type in the .cpp (walked as nested named sections) ---
        u32  maShakeParams[16 / 4];                     // +0x08  "Shake Params"     Utils::CameraShake::Parameters (16B -> +0x18)
        u8   maReserved18[0x2C - 0x18];                 // +0x18..+0x2B  un-serialised gyro-cam rig state
        u32  maLookerParams[100 / 4];                   // +0x2C  "Looker Params"    Utils::Looker::Parameters (100B -> +0x90)
        u32  maAttachmentTruck[8 / 4];                  // +0x90  "Attachment truck" AttachmentTruck::Parameters (8B -> +0x98)

        // --- f32 tunables (debug-menu SetStep 0.01) ---
        f32  mfSlowDistance;                 // +0x98  "Slow Distance"
        f32  mfSlowHeight;                   // +0x9C  "Slow Height"
        f32  mfSlowPitch;                    // +0xA0  "Slow Pitch"
        f32  mfFastDistance;                 // +0xA4  "Fast Distance"
        f32  mfFastHeight;                   // +0xA8  "Fast Height"
        f32  mfFastPitch;                    // +0xAC  "Fast Pitch"
        f32  mfField_B0;                     // +0xB0  label unrecovered (the declaration
                                             //        reference names this slot mfFOV; the
                                             //        debug-menu label string is not recovered)
        f32  mfBlendFactorBlendFactor;       // +0xB4  "Blend Factor Blend Factor"
        f32  mfMinimumBlendFactor;           // +0xB8  "Minimum Blend Factor"
        f32  mfMaximumBlendFactor;           // +0xBC  "Maximum Blend Factor"
        f32  mfHeightDistanceBlendFactor;    // +0xC0  "Height Distance Blend Factor"
        f32  mfHeightDistanceVelocityRange;  // +0xC4  "Height Distance Velocity Range"

        // --- bool flags ---
        bool mbUseTruck;                     // +0xC8  "Use Truck"
        bool mbUseSideVector;                // +0xC9  "Use Side Vector"
        bool mbInvertVector;                 // +0xCA  "Invert Vector"
        bool mbStickToGround;                // +0xCB  "Stick to ground"
    };

    // ---- the virtual interface (see the vtable table in the file banner) -------------------

    // PARTIAL (see the gate on the body below).
    void Construct() override;                                                    // slot 0

    // Seed the live height/distance/pitch/blend scalars from the adopted block.
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;        // slot 1

    // Hand back whichever of the two embedded policies is active.
    CollisionPolicy* GetCollisionPolicy() override;                                // slot 5

    // Return this behaviour's debug name.
    const char* GetName() const override;                                          // slot 7

    // ---- non-virtual API -------------------------------------------------------------------

    // Adopt a gyro-cam parameter block: assert it carries the gyro-cam type tag, cache its
    // debug-name slot at +0x10, then store the pointer. NOT a virtual override:
    // it takes the DERIVED Parameters type, so it HIDES the base's non-virtual pair rather
    // than overriding anything.
    void SetParameters(const Parameters* lpParameters);

    // Bind the gyro-cam rig to a race car (the mAttachedTo VehicleRef at +0x510).
    // meRaceCarIndex is an EActiveRaceCarIndex (modelled as s32 here, as the call sites spell it).
    void AttachToRaceCar(s32 meRaceCarIndex);

    // Plant the rig where it stands (the name is the declaration reference's).
    // MomentTumbling::SignalIsGoodTimeToPlant inlines exactly this pair of stores
    // (+0x630/+0x631, both stb 1) on a LEAD-subtype tumble.
    void Plant() { mbIsPlanted = true; mbIsFirstFrameOfPlanted = true; }

    // The spelling MomentTumbling.cpp already calls. Forwards to the console's own Plant().
    // DELETE-WHEN: the moment TU calls Plant() directly.
    void SignalGoodTimeToPlant() { Plant(); }

    // Select the vehicle-attachment collision policy over the visibility one (+0x633).
    // The name is the declaration reference's. The takedown state raises it at five sites (ArbStateTakedown::Prepare,
    // B3ClassicTakedownPlayer::Prepare, both DestructionPathTakedownPlayer rigs and
    // ShutdownTakedownPlayer::Update's look-back arm) right after AttachToRaceCar.
    void SetUseVehicleAttachmentCollision(bool lbUseVehicleAttachmentCollision)
    {
        mbUseVehicleAttachmentCollision = lbUseVehicleAttachmentCollision;
    }

    // Seed the world-space normalized from-car vector: assert it has not already been set and that
    // it is unit length, store it into both the current and desired vector members, and mark it
    // set. lVectorFromCar arrives in the first vector register (v1).
    void SetWorldSpaceNormalizedVectorFromCar(rw::math::vpu::Vector3 lVectorFromCar);

private:

    // ---- layout (declaration-reference member order; every offset asm-pinned -- see the
    //      file banner) ------------------------------------------------------------------
    const Parameters*                mpParameters;      // +0x014  Construct zeroes it; SetParameters stores it

    // FLAG (home): the console packs the parameter block's +0x04 word into the BASE's +0x10
    //   slot, which the original names `Behaviour::mpcDebugParametersName` (a `const char*`) --
    //   i.e. the console line is `SetDebugParametersName(lpParameters->GetDebugName())`, exactly
    //   as the re-based bumper cam spells it. It cannot be spelled that way HERE: this class's
    //   Parameters block is pinned byte-exact at 204 bytes (see the note on Parameters above),
    //   so its debug-name slot is a 4-byte word, and handing a 4-byte word to a `const char*`
    //   setter would be an offset hack with teeth. The cached word therefore keeps a NAMED
    //   member of its own (the x64 gate is semantic parity by named member, so the extra word
    //   costs nothing) and the base field is left alone.
    //   DELETE-WHEN: Parameters carries the typed Behaviour::Parameters head.
    s32                              mParamWord1;       // +0x010  cached lpParameters->miParamWord1

    // The two policies GetCollisionPolicy hands back, BY NAME (the declaration reference's own
    // member names and types).
    // They used to be opaque byte spans reached with a reinterpret_cast; with the real types
    // embedded, slot 5 returns a live polymorphic object instead of raw storage.
    VisibilityCollisionPolicy        mVisibilityCollisionPolicy;         // +0x020
    CollisionPolicyAttachedToVehicle mVehicleAttachmentCollisionPolicy;  // +0x260

    // +0x4B0 .. +0x50F: mTransform (Matrix44Affine) and mLooker -- rig members with no home
    // type yet. Reserved so the members after them keep the console's order.
    u8                               maReserved4B0[0x510 - 0x4B0];

    // +0x510: the vehicle this rig hangs off. Construct seeds it {E_PLAYER_CAR, -1, 0, set} and
    // AttachToRaceCar re-binds it to a race car -- the four stores the console emits inline.
    Behaviour::VehicleRef            mAttachedTo;       // +0x510 .. +0x51F

    rw::math::vpu::Vector3 mCurrentTargetPos;                         // +0x520
    rw::math::vpu::Vector3 mWorldSpaceNormalizedVectorFromCar;        // +0x530
    rw::math::vpu::Vector3 mDesiredWorldSpaceNormalizedVectorFromCar; // +0x540

    // +0x550 .. +0x61F: mOriginalPoint, mPositionLag, mRandom, mShake, mAttachmentTruck --
    // rig members with no home type yet.
    u8                               maReserved550[0x620 - 0x550];

    // The live rig scalars Prepare seeds off the adopted parameter block.
    f32                              mfHeight;          // +0x620  <- mfFastHeight
    f32                              mfDistance;        // +0x624  <- mfFastDistance
    f32                              mfPitch;           // +0x628  <- mfFastPitch
    f32                              mfBlendFactor;     // +0x62C  <- mfMinimumBlendFactor

    bool                             mbIsPlanted;            // +0x630
    bool                             mbIsFirstFrameOfPlanted; // +0x631
    bool                             mbIsWorldSpaceVectorSet; // +0x632  set once the from-car vector is seeded
    bool                             mbUseVehicleAttachmentCollision; // +0x633  selects the active policy
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::Construct
//   li   r5, 0 ; li r31, 1
//   stb  r5, 8..0xC(r6) ; stw r5, 4(r6)                      ; the inlined Behaviour::Construct
//   stw  r5, 0x10(r6)                                        ; the base's debug-name slot --
//                                                              here mParamWord1 (see its FLAG)
//   stw  r5, 0x14(r6)                                        ; mpParameters = 0
//   ... ~40 stores across +0x20..+0x25F                      ; VisibilityCollisionPolicy::Construct,
//                                                              inlined  (GATED -- see below)
//   bl   CollisionPolicyAttachedToVehicle::Construct(r6+0x260, 0)
//   stb  r31(=1), 0x4AC(r6)                                  ; policy-relative +0x24C ==
//                                                              mbTestAgainstWorldOnly
//   ... the Random / Looker / PositionLag / shake / truck seeds inside +0x4B0..+0x61F (GATED)
//   stw  r5,  0x510(r6) ; stw r11(=-1), 0x514(r6) ; stw r5, 0x518(r6) ; stb r31(=1), 0x51C(r6)
//   stb  r5,  0x630/0x631/0x632/0x633(r6)
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::Construct()
{
    Behaviour::Construct();                       // the seven inlined base stores

    // The console's single `stw 0, 0x10` covers the base's debug-name slot AND this class's
    // stand-in for it. Behaviour::Construct zeroes the former; mParamWord1 needs its own seed.
    mParamWord1  = 0;                             // stw 0, 0x10(this)
    mpParameters = 0;                             // stw 0, 0x14(this)

    // ⚠️ GATE: VisibilityCollisionPolicy::Construct(mVisibilityCollisionPolicy) -- the console's
    //   ~40-store block across +0x20..+0x25F (the see-through triple, the two 1.5f / 0.5f leaf
    //   magnitudes, the -1.0f at +0x230 and the can-fail / first-frame pair).
    //   VisibilityCollisionPolicy::Construct is DECLARATION-ONLY in this tree and this TU is not
    //   its home, so calling it would put an unresolved external into every link. The policy is
    //   left as the pool allocation left it, exactly as BehaviourInterpolate::Construct records
    //   for the same sub-object.
    //   CONSEQUENCE: the visibility policy is only reachable through GetCollisionPolicy below,
    //   and only while mbUseVehicleAttachmentCollision is false -- which the takedown states
    //   clear by raising it through SetUseVehicleAttachmentCollision.
    //   DELETE-WHEN: VisibilityCollisionPolicy::Construct is bodied.

    mVehicleAttachmentCollisionPolicy.Construct(false);   // bl ...::Construct(this+0x260, 0)

    // The one store the console emits between the policy construct and the rig seeds. It lands
    // at this+0x4AC, i.e. policy-relative +0x24C -- mbTestAgainstWorldOnly, which the policy's
    // own Construct had just cleared. The gyro cam therefore resolves against the world only,
    // never against other vehicles. (this is still live in the same register across the call;
    // the callee touches neither it nor the constant register the 1 rides in.)
    mVehicleAttachmentCollisionPolicy.SetTestAgainstWorldOnly(true);   // stb 1, 0x4AC(this)

    // ⚠️ GATE: the rig seeds inside maReserved4B0 / maReserved550 (mTransform, mLooker, the
    //   CgsNumeric::Random stream the console reseeds in place, mPositionLag, mShake and
    //   mAttachmentTruck). None of those members has a home type yet, so poking them by offset
    //   is exactly what the x64 rule forbids.
    //   DELETE-WHEN: the gyro rig TU lands and those members are named.

    // mAttachedTo: the inlined VehicleRef construct (kind = the player car, no race car bound).
    mAttachedTo.meType         = BrnDirector::VehicleRef::E_PLAYER_CAR;  // stw 0,  0x510(this)
    mAttachedTo.miRaceCarIndex = -1;                                     // stw -1, 0x514(this)
    mAttachedTo.muRef          = 0;                                      // stw 0,  0x518(this)
    mAttachedTo.mbSet          = true;                                   // stb 1,  0x51C(this)

    mbIsPlanted                     = false;      // stb 0, 0x630(this)
    mbIsFirstFrameOfPlanted         = false;      // stb 0, 0x631(this)
    mbIsWorldSpaceVectorSet         = false;      // stb 0, 0x632(this)
    mbUseVehicleAttachmentCollision = false;      // stb 0, 0x633(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::Prepare
//   li   r11, 0 ; stb r11, 8(r31)     ; SetNotPrepared()  (the store precedes the assert)
//   lwz  r10, 0x14(r31)               ; mpParameters
//   ... assert mpParameters != NULL ...
//   lfs  f0, 0xB8(r11) ; stfs f0, 0x62C(r31)   ; mfBlendFactor = mfMinimumBlendFactor
//   lfs  f0, 0xA8(r11) ; stfs f0, 0x620(r31)   ; mfHeight      = mfFastHeight
//   lfs  f0, 0xA4(r11) ; stfs f0, 0x624(r31)   ; mfDistance    = mfFastDistance
//   lfs  f0, 0xAC(r11) ; stfs f0, 0x628(r31)   ; mfPitch       = mfFastPitch
//   li   r3, 1                                 ; cannot fail
// (the four destination offsets land on the declaration reference's mfHeight/mfDistance/mfPitch/mfBlendFactor
//  in that order, which is what pins them.)
// ----------------------------------------------------------------------------
inline bool
BehaviourGyroCam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();                                        // stb 0, 8(this)

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");

    mfBlendFactor = mpParameters->mfMinimumBlendFactor;      // +0xB8 -> +0x62C
    mfHeight      = mpParameters->mfFastHeight;              // +0xA8 -> +0x620
    mfDistance    = mpParameters->mfFastDistance;            // +0xA4 -> +0x624
    mfPitch       = mpParameters->mfFastPitch;               // +0xAC -> +0x628

    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::GetName
//   lis/addi r3, aBehaviourgyroc   ; return "BehaviourGyroCam"
// ----------------------------------------------------------------------------
inline const char*
BehaviourGyroCam::GetName() const
{
    return "BehaviourGyroCam";
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::SetParameters
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 9            ; == eBehaviourGyroCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1 (the debug-name slot)
//   stw  r4,  0x14(r3)       ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)       ; the behaviour's cached +0x10 word (see the mParamWord1 FLAG)
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourGyroCam,
               "lpParameters->GetType() == eBehaviourGyroCam");
    mpParameters = lpParameters;                 // stw r31, 0x14(this)
    mParamWord1  = lpParameters->miParamWord1;   // lwz r11,4(lpParameters); stw r11, 0x10(this)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::AttachToRaceCar
//   stw  r4,  0x514(r3)      ; mAttachedTo.miRaceCarIndex = meRaceCarIndex
//   stb  1,   0x51C(r3)      ; mAttachedTo.mbSet          = true
//   stw  1,   0x510(r3)      ; mAttachedTo.meType         = E_RACE_CAR
//   stw  0,   0x518(r3)      ; mAttachedTo.muRef          = 0
//   cmpwi r4, 8 ; blt skip   ; assert meRaceCarIndex < ku8MaxNumRaceCars
// (all four stores precede the assert; together they are the inlined VehicleRef::SetToRaceCar,
//  which is declaration-only in this tree -- hence the stores are written out here, as the
//  console emits them.)
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::AttachToRaceCar(s32 meRaceCarIndex)
{
    mAttachedTo.miRaceCarIndex = meRaceCarIndex;                        // stw r4,  0x514(this)
    mAttachedTo.mbSet          = true;                                  // stb r11(=1), 0x51C(this)
    mAttachedTo.meType         = BrnDirector::VehicleRef::E_RACE_CAR;   // stw r11(=1), 0x510(this)
    mAttachedTo.muRef          = 0;                                     // stw r10(=0), 0x518(this)
    CGS_ASSERT(meRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::GetCollisionPolicy
//   lbz  r11, 0x633(r3)      ; mbUseVehicleAttachmentCollision
//   cmplwi r11, 0 ; beq      ; if (set) ...
//   addi r3, r3, 0x260       ;   return &mVehicleAttachmentCollisionPolicy
//   ... else ...
//   addi r3, r3, 0x20        ;   return &mVisibilityCollisionPolicy
// ----------------------------------------------------------------------------
inline CollisionPolicy*
BehaviourGyroCam::GetCollisionPolicy()
{
    if (mbUseVehicleAttachmentCollision)
    {
        return &mVehicleAttachmentCollisionPolicy;   // this + 0x260
    }
    return &mVisibilityCollisionPolicy;              // this + 0x20
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGyroCam::SetWorldSpaceNormalizedVectorFromCar
//   lbz  r11, 0x632(r3)            ; mbIsWorldSpaceVectorSet
//   ... assert !mbIsWorldSpaceVectorSet ("Doesn't make sense to set ... twice") ...
//   vmsum3fp128 ...                ; MagnitudeSquared(lVectorFromCar) (the IsSimilar assert)
//   ... assert IsSimilar(MagnitudeSquared(lVectorFromCar), 1.0f) ...
//   stvx128 v127, r3, 0x530        ; mWorldSpaceNormalizedVectorFromCar        = lVectorFromCar
//   stvx128 v127, r3, 0x540        ; mDesiredWorldSpaceNormalizedVectorFromCar = lVectorFromCar
//   stb  1,  0x632(r3)             ; mbIsWorldSpaceVectorSet = 1
//
// The console computes MagnitudeSquared as a VMX dot product (vmsum3fp128) and the IsSimilar
// tolerance compare with a vcmpgtfp/vperm pair; the reconstruction folds that to the scalar
// rw::math::vpu::MagnitudeSquared and an absolute-difference tolerance compare (the rw "IsSimilar"
// spelling is not yet homed, so the equivalent |x - 1| <= eps form is inlined into the assert).
// ----------------------------------------------------------------------------
inline void
BehaviourGyroCam::SetWorldSpaceNormalizedVectorFromCar(rw::math::vpu::Vector3 lVectorFromCar)
{
    CGS_ASSERT(!mbIsWorldSpaceVectorSet,
               "Doesn't make sense to set WorldSpaceNormalizedVectorFromCar twice");
    CGS_ASSERT(std::fabs(rw::math::vpu::MagnitudeSquared(lVectorFromCar) - 1.0f) <= 1.0e-3f,
               "rw::math::IsSimilar(MagnitudeSquared(lVectorFromCar), 1.0f)");

    mWorldSpaceNormalizedVectorFromCar        = lVectorFromCar;   // stvx128 v127, r30, 0x530
    mDesiredWorldSpaceNormalizedVectorFromCar = lVectorFromCar;   // stvx128 v127, r30, 0x540
    mbIsWorldSpaceVectorSet                   = true;             // stb r9(=1), 0x632(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GYRO_CAM_H
