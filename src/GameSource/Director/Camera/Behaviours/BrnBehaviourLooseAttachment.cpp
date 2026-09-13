// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourLooseAttachment slice this TU owns:
// the five transcribed virtuals (Construct, Prepare, GetCollisionPolicy, SetupTweaker, GetName)
// and the two reference binders (AttachTo / SetTarget). SetParameters and LockTargetVector are
// defined inline in the header. The rest of the behaviour (Update and the
// full rig) lands with its own TU.
//
// The loose-attachment Parameters field-walk visitor and its three explicit instantiations
// live in the sibling BrnBehaviourLooseAttachmentParameters.cpp, so this TU can be mounted
// without dragging in the camera-tunings serialiser types.
//
// The behaviour is installed by the new-car-joined moment, by the shutdown-takedown state (its
// lookback rig plus the three zoom beats) and by the arbitrator testbed.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker::Construct (slot 6)

#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// The pool bucket this behaviour routes to. AllocateBehaviour<TBehaviour> bakes the choice from
// sizeof(TBehaviour): at or under the small pool's 1600-byte bucket it takes one of the twenty
// small slots, otherwise one of the eight large ones. The console record is 0x330 and this
// reconstruction measures 0x340 -- the base head, the embedded policy and the adopted-parameter
// pointer all widen on the host -- so it stays well inside the small bucket and routes exactly
// where the console routes it. That matters here: the shutdown-takedown arm holds up to four of
// these at once (the lookback rig plus the three zoom beats), and unlike the gyro rig (which the
// host widening pushed out of the small bucket onto the eight-slot large pool) this one costs the
// large pool nothing.
static_assert(sizeof(BehaviourLooseAttachment) <= 100u * 16u,
              "BehaviourLooseAttachment fits the small behaviour pool bucket");

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::Construct -- seed a freshly pooled instance.
//
//   li     r5, 0
//   stb    r5,  +0x008(r6)    ; \
//   stb    r5,  +0x009(r6)    ;  |
//   stb    r5,  +0x00A(r6)    ;  |- the base head: these seven stores ARE Behaviour::Construct,
//   stb    r5,  +0x00B(r6)    ;  |  inlined (meTimestepType, the five flag bytes and the debug
//   stb    r5,  +0x00C(r6)    ;  |  parameters name, all zero)
//   stw    r5,  +0x004(r6)    ;  |
//   stw    r5,  +0x010(r6)    ; /
//   stb    r8,  +0x2A0(r6)    ; lag +0x20 mbFirstFrame  = true   (r8 == 1)   \ PositionLag::
//   stb    r8,  +0x2A1(r6)    ; lag +0x21 mbConstructed = true               / Construct
//   stfs   f0,  +0x2F0(r6) .. +0x300(r6)   ; the five zeroed floats of the impact effect
//   stfs   f0,  +0x2E0(r6) .. +0x2EC(r6)   ; CameraShake::Construct (four wobble floats)
//   <the Random::Construct spine at +0x2B0: seed, eight ring draws, index bump>
//   stb    r5,  +0x310(r6)    ; mTarget     set-flag = false
//   stb    r5,  +0x320(r6)    ; mAttachment set-flag = false
//   addi   r3,  r6, 0x20 ; li r4, 0 ; bl CollisionPolicyAttachedToVehicle::Construct
//   stb    r5,  +0x32C(r6)    ; mbDetached         = false
//   stb    r5,  +0x32E(r6)    ; mbTargetVectorLock = false
//
// The one read-only constant is 0.0f (a big-endian 00000000 at the referenced slot); the two
// 64-bit literals the Random spine builds are the engine's default seed and LCG multiplier,
// which is what identifies the spine as Random::Construct rather than an open-coded draw.
//
// NOT written here, faithfully: mWorldSpaceOffsetFromCar, mpParameters (SetParameters adopts
// it), mfDetachedAmount and mbFirstFrame (Prepare arms the latch).
// ----------------------------------------------------------------------------
void BehaviourLooseAttachment::Construct()
{
    Behaviour::Construct();

    // The four rig sub-objects, each seeded by its own Construct -- which is what the console
    // does too, since every one of them is inlined here rather than called.
    mPositionLag.Construct();                   // the two flag bytes at the lag's +0x20 / +0x21
    mRandom.Construct();                        // the ring, the default seed and the index bump
    mShake.Construct();                         // the four wobble floats

    // The impact effect's own Construct is not homed, and the console does not call it either:
    // it emits the five zero stores directly. Written here through the effect's two named
    // handles so no byte of it is reached by displacement.
    mImpact.SetImpactFactor(0.0f);              // effect +0x00
    mImpact.GetCameraShake().Construct();       // effect +0x04 (the embedded 16-byte shake)

    // Both vehicle references start unbound. The console writes only the set-flag byte of each
    // (it does NOT run VehicleRef::Construct here), so only that byte is written.
    mTarget.mbSet     = false;                  // stb 0, +0x310
    mAttachment.mbSet = false;                  // stb 0, +0x320

    // The embedded vehicle-attached policy, with the literal false every loose-attachment site
    // passes (the vehicle-collision flag at the policy's own +0x24F).
    mCollisionPolicy.Construct(false);           // bl ..., r4 == 0

    mbDetached         = false;                 // stb 0, +0x32C
    mbTargetVectorLock = false;                 // stb 0, +0x32E
}

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::Prepare
//   lbz    r11, +0x320(r31)   ; mAttachment set-flag
//   cmplwi r11, 0
//   bne    ...                ; assert mAttachment.HasBeenSet()
//   li     r3,  1             ; the return value: it cannot fail
//   stb    r11, +0x008(r31)   ; SetNotPrepared()
//   stb    r10, +0x32D(r31)   ; mbFirstFrame = true   (r10 == 1)
//
// The shared prepare/release info block is not read. Dropping the prepared latch and arming the
// first-frame latch is the same first-frame idiom the road-runner and interpolate behaviours
// use -- Update re-seeds the rig on the frame after (it is mbFirstFrame that lets the position
// lag run on the first frame of a detached rig, and Update clears it again).
// ----------------------------------------------------------------------------
bool BehaviourLooseAttachment::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    CGS_ASSERT(mAttachment.mbSet, "mAttachment.HasBeenSet()");

    SetNotPrepared();                                       // stb 0, +0x08
    mbFirstFrame = true;                                    // stb 1, +0x32D

    return true;                                            // li r3, 1
}

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::GetCollisionPolicy
//   lbz    r11, +0x32C(r3)    ; mbDetached
//   addi   r3,  r3, 0x20      ; r3 = &this->mCollisionPolicy (the candidate return)
//   cmplwi r11, 0
//   beqlr                     ; flag clear -> return &mCollisionPolicy
//   li     r3, 0              ; flag set   -> return null
//   blr
//
// The `addi r3, r3, 0x20` is the derived-to-base adjustment folded into the return: on the
// console CollisionPolicyAttachedToVehicle's CollisionPolicy sub-object is at its own +0x00, so
// the policy's address and the interface pointer coincide. On the host the compiler emits
// whatever adjustment the real base sub-object needs; parity is by named member.
//
// This is the slot-5 virtual the retired slice modelled as a hand-written `Get()` returning an
// invented `SubObject*`.
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourLooseAttachment::GetCollisionPolicy()
{
    if (mbDetached)                                         // lbz +0x32C; bne -> null
    {
        return 0;
    }
    return &mCollisionPolicy;                               // this + 0x20
}

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::SetupTweaker
//   mr     r3, r4
//   b      Utils::Tweaker::Construct
//
// A tail call into Construct on the SUPPLIED tweaker: the loose-attachment rig resets the
// tweaker it is handed and exposes no tweakable of its own. (The image folds this body with
// BehaviourIceAnim::SetupTweaker, which is byte-identical.)
// ----------------------------------------------------------------------------
void BehaviourLooseAttachment::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::GetName -- returns the class's own literal.
// ----------------------------------------------------------------------------
const char* BehaviourLooseAttachment::GetName() const
{
    return "BehaviourLooseAttachment";
}

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::AttachTo
//   stw  r4,  +0x318(r3)     ; mAttachment.miRaceCarIndex = leRaceCarIndex
//   stb  1,   +0x320(r3)     ; mAttachment.mbSet          = true
//   stw  1,   +0x314(r3)     ; mAttachment.meType         = E_RACE_CAR
//   stw  0,   +0x31C(r3)     ; mAttachment.muRef          = 0
//   cmpwi r4, 8 ; blt skip   ; assert "meRaceCarIndex < ...ku8MaxNumRaceCars"
//
// All four stores precede the assert, and they are VehicleRef's own race-car binder inlined --
// which is why the assert cites the reference's header, not this one -- and why its text names
// the reference's own member meRaceCarIndex rather than this parameter. Do not "correct" the
// literal to the parameter spelling: it is the attested string. Written through the named
// reference rather than as a call: the binder has no body in this tree yet, and forwarding to a
// declaration-only method would emit an unresolved external at every link.
// ----------------------------------------------------------------------------
void BehaviourLooseAttachment::AttachTo(s32 leRaceCarIndex)
{
    mAttachment.miRaceCarIndex = leRaceCarIndex;            // stw r4, +0x318
    mAttachment.mbSet          = true;                      // stb 1,  +0x320
    mAttachment.meType         = BrnDirector::VehicleRef::E_RACE_CAR;   // stw 1, +0x314
    mAttachment.muRef          = 0;                         // stw 0,  +0x31C

    CGS_ASSERT(leRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::SetTarget -- the identical binder on the other reference.
//   stw  r4,  +0x308(r3)     ; mTarget.miRaceCarIndex = leRaceCarIndex
//   stb  1,   +0x310(r3)     ; mTarget.mbSet          = true
//   stw  1,   +0x304(r3)     ; mTarget.meType         = E_RACE_CAR
//   stw  0,   +0x30C(r3)     ; mTarget.muRef          = 0
//   cmpwi r4, 8 ; blt skip   ; assert "meRaceCarIndex < ...ku8MaxNumRaceCars"
// ----------------------------------------------------------------------------
void BehaviourLooseAttachment::SetTarget(s32 leRaceCarIndex)
{
    mTarget.miRaceCarIndex = leRaceCarIndex;                // stw r4, +0x308
    mTarget.mbSet          = true;                          // stb 1,  +0x310
    mTarget.meType         = BrnDirector::VehicleRef::E_RACE_CAR;       // stw 1, +0x304
    mTarget.muRef          = 0;                             // stw 0,  +0x30C

    CGS_ASSERT(leRaceCarIndex < KU_MAX_NUM_RACE_CARS,
               "meRaceCarIndex < BrnPhysics::Vehicle::ku8MaxNumRaceCars");
}

// Pin the field-walk offsets of the parameter block (host-pointer-width invariant -- the walked
// region holds no pointers): the "Impact" sub-block at +0x2C and the loose-attachment tunables
// at the +0x48..+0x60 offsets the write/read/menu assembly loads and stores.
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mPositionLagParams) == 0x08, "position-lag block @ +0x08");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mShakeParams)       == 0x1C, "shake block @ +0x1C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mImpact)            == 0x2C, "Impact block @ +0x2C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfPitch)            == 0x48, "Pitch @ +0x48");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfHeight)           == 0x4C, "Height @ +0x4C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfDistance)         == 0x50, "Distance @ +0x50");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfField54)          == 0x54, "unk label field @ +0x54");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfDutch)            == 0x58, "Dutch @ +0x58");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mfDetachLerpAmount) == 0x5C, "Detach Lerp Amount @ +0x5C");
static_assert(offsetof(BehaviourLooseAttachment::Parameters, mbLookFromTarget)   == 0x60, "Look from target @ +0x60");

} // namespace Camera
} // namespace BrnDirector
