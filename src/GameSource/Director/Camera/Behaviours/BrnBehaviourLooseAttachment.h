#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"         // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"           // CollisionPolicy(+AttachedToVehicle)
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"         // Utils::PositionLag (+ ::Parameters)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"         // Utils::CameraShake (+ ::Parameters), Random
#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"  // Utils::CameraImpactEffect (+ ::Parameters)

#include <cstddef>   // offsetof (the compile-time layout pins)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h
//
// BrnDirector::Camera::BehaviourLooseAttachment -- the "loose attachment" camera behaviour: a
// camera softly tethered to one race car while it looks at another. Installed by the new-car-
// joined moment, by the shutdown/revenge takedown state (its lookback rig plus the three zoom
// beats) and by the arbitrator testbed.
//
// RE-BASED. This class used to be a raw-offset SLICE: an opaque `void* mpVTable` head, a
// hand-placed timestep word and cached parameter word, reserved byte spans, an invented nested
// `SubObject` type with an invented `Get()` accessor over an untyped `u8 maSubObject[]`, and a
// hand-rolled `SetTimestepType` duplicating the base's. It now derives the canonical
// BrnDirector::Camera::Behaviour and carries the recovered member list by name.
//
// WHY IT HAD TO BE RE-BASED: the shutdown-takedown state pools THREE of these through
// BehaviourManager::NewBehaviour<BehaviourLooseAttachment> (one per zoom beat) on top of the
// lookback rig, and each is placement-new'd into a raw pool slot; BehaviourManager::
// BehaviourHelper::Prepare then dispatches vtable slot 0. For a NON-POLYMORPHIC class
// placement-new installs no vtable, so that dispatch read a null vptr -- the identical
// EXCEPTION_ACCESS_VIOLATION the aftertouch-crash and gyro rigs hit before they were re-based.
//
// ----------------------------------------------------------------------------
// THE LAYOUT CHAIN (member NAMES, types and order from the declaration-shape reference; every
// offset independently re-derived from the assembly of Construct / Prepare / GetCollisionPolicy
// / Update, and the chain closes exactly with no slack):
//
//   base head                   +0x000 .. +0x013   Behaviour
//   mCollisionPolicy            +0x020   CollisionPolicyAttachedToVehicle (0x250)
//   mWorldSpaceOffsetFromCar    +0x270   Vector3
//   mPositionLag                +0x280   Utils::PositionLag               (0x30)
//   mRandom                     +0x2B0   CgsNumeric::Random               (0x30)
//   mShake                      +0x2E0   Utils::CameraShake               (0x10)
//   mImpact                     +0x2F0   Utils::CameraImpactEffect        (0x14)
//   mTarget                     +0x304   Behaviour::VehicleRef            (0x10)
//   mAttachment                 +0x314   Behaviour::VehicleRef            (0x10)
//   mpParameters                +0x324
//   mfDetachedAmount            +0x328
//   mbDetached                  +0x32C
//   mbFirstFrame                +0x32D
//   mbTargetVectorLock          +0x32E
//
// Four independent anchors pin the chain, and each is one the retired slice got wrong:
//   * Construct hands `this + 0x20` to CollisionPolicyAttachedToVehicle::Construct with a
//     literal false. The slice modelled +0x20 as an untyped byte blob behind an invented
//     `Get()` accessor -- it is the vehicle-attached collision policy, and that "accessor" is
//     the base's own slot-5 virtual GetCollisionPolicy (it returns `this + 0x20`, or null when
//     the +0x32C byte is set, which is exactly the aftertouch-crash rig's shape).
//   * Construct raises two adjacent bytes at +0x2A0 / +0x2A1 -- verbatim PositionLag::Construct
//     (mbFirstFrame / mbConstructed at the lag's own +0x20 / +0x21), so the lag sits at +0x280.
//     Update corroborates it: it hands `this + 0x280` straight to PositionLag::Update.
//   * Construct then inlines Random::Construct at +0x2B0 (the eight-slot float ring, the 64-bit
//     seed at ring +0x20 and the oldest-slot index at ring +0x28, stepped by the engine's LCG
//     multiplier), a four-float CameraShake::Construct at +0x2E0, and five zeroed floats at
//     +0x2F0 -- the impact factor plus its embedded shake, i.e. the 20-byte impact effect.
//   * the impact effect therefore closes at +0x303 and the target reference picks up at +0x304
//     with no padding; the two 16-byte vehicle references then carry mpParameters to its
//     attested +0x324, which SetParameters stores into.
//
// x64: parity is BY NAMED MEMBER (the base head, the embedded policy and mpParameters all
// widen); the offsets above are provenance and are never used as casts.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   loose-attachment one. The value 11 is attested (the console compares the block's first word
//   against 11). There is still no single homed EBehaviourType enum -- each behaviour's tag is
//   only observable in its own assert.
enum EBehaviourTypeLooseAttachment
{
    eBehaviourLooseAttachment = 11
};

// FLAG: the upper bound the race-car index asserts enforce. The console value for
//   BrnPhysics::Vehicle::ku8MaxNumRaceCars is 8 (the race-car index is compared against 8 in
//   both reference binders). Replace with the real BrnPhysics::Vehicle constant when that TU
//   lands; the VALUE (8) is assembly.
// Guarded: see the identical guard note in BrnBehaviourGyroCam.h / BrnBehaviourBystanderCam.h --
// this same unnamed enum is independently (re)declared in each; the guard makes a second
// inclusion in one TU (e.g. BrnArbStateTakedown.cpp, which needs both GyroCam and
// LooseAttachment) a no-op instead of a redefinition error.
#ifndef BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
#define BRNDIRECTOR_CAMERA_KU_MAX_NUM_RACE_CARS_DEFINED
enum { KU_MAX_NUM_RACE_CARS = 8 };
#endif

class BehaviourLooseAttachment : public Behaviour
{
public:

    // The loose-attachment parameter block: a type tag in its leading word plus behaviour-
    // specific data. GetType returns the tag SetParameters asserts on.
    //
    // The field-walk region (the embedded "Impact" sub-block + the loose-attachment tunables) is
    // pinned store-for-store from the three Serialise<S> visitor bodies (write, read and
    // debug-menu): a by-value CameraImpactEffect::Parameters sub-block at +0x2C (walked as the
    // nested "Impact" section) followed by the loose-attachment f32/bool tunables at the
    // +0x48..+0x60 displacements the write/read/menu assembly loads and stores. No pointers in
    // the walked region, so the offsets are host-pointer-width invariant (pinned in the .cpp).
    //
    // PARK: this block cannot derive Behaviour::Parameters (which is what the recovered
    //   declaration has) until the parameter-bank lane re-expresses BrnBehaviourParameterBank.h's
    //   100-byte stride pin as sizeof(Camera::BehaviourLooseAttachment::Parameters) instead of a
    //   console literal -- deriving it would widen the head by the debug-name pointer and move
    //   every authored block in the bank.
    class Parameters
    {
    public:
        // Console visitor: `void Serialise<S>(S&)` -- walks this block's fields into the
        // camera-tunings serialiser S (DebugMenuSerialiser / TextFile{Read,Write}Serialiser),
        // recursing into the embedded impact block for the "Impact" section. The ONE templated
        // field-walk body + its three explicit instantiations are bodied in this TU's .cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        // Seed the block to its defaults. A leaf with no calls: it writes the type tag, clears
        // the second word, seeds both sub-blocks at +0x08 / +0x1C and the impact block at +0x2C,
        // then the +0x48..+0x60 tunables. Defined below, beside the other inline members.
        // Called by MomentNewCarJoined::Construct on its own by-value parameter block, by the
        // takedown state on its lookback block, and by the parameter bank on the three
        // shutdown-takedown zoom blocks.
        void Construct();

        EBehaviourTypeLooseAttachment GetType() const
        {
            return static_cast<EBehaviourTypeLooseAttachment>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word

        // +0x08..+0x2B is two by-value sub-blocks, not opaque bytes: the behaviour's Update
        // hands &(params +0x08) to PositionLag::Update and &(params +0x1C) to CameraShake::
        // Update. Neither is reached by a Serialise<S> visitor -- the field-walk starts at the
        // +0x2C "Impact" block -- which is why the tunings file carries no section for either.
        Utils::PositionLag::Parameters        mPositionLagParams;   // +0x08  camera position smoother (20B)
        Utils::CameraShake::Parameters        mShakeParams;         // +0x1C  the rig's own shake block (16B)

        Utils::CameraImpactEffect::Parameters mImpact;   // +0x2C  embedded impact-shake block ("Impact")
        f32 mfPitch;                                     // +0x48  "Pitch"
        f32 mfHeight;                                    // +0x4C  "Height"
        f32 mfDistance;                                  // +0x50  "Distance"
        f32 mfField54;                                   // +0x54  field label rodata unrecovered
                                                         //  The declaration reference names this slot
                                                         //        mfFOV, which the seeds corroborate: 90.0f by
                                                         //        default, 40.0f for the new-car-joined moment,
                                                         //        100.0f for the shutdown takedown zoom beats --
                                                         //        all field-of-view degrees. NOT renamed here:
                                                         //        two TUs outside this header's ownership spell
                                                         //        it mfField54 (this class's own .cpp serialiser
                                                         //        and BrnMomentNewCarJoined_wO_01.cpp), so the
                                                         //        rename must land with them in one change.
        f32 mfDutch;                                     // +0x58  "Dutch"
        f32 mfDetachLerpAmount;                          // +0x5C  "Detach Lerp Amount"
        bool mbLookFromTarget;                           // +0x60  "Look from target"
    };

    // ---- the virtual interface ----------------------------------------------------------
    // The base's interface is EIGHT slots (Construct / Prepare / Update / PostCollisionUpdate /
    // Release / GetCollisionPolicy / SetupTweaker / GetName; GetParameters/SetParameters are not
    // virtual and there is no destructor slot). The class vtable read out of the image is eight
    // words long and appends NO virtuals of its own, so the derived table is the base's eight
    // slots with SIX re-pointed -- slots 0, 1, 2, 5, 6 and 7 hold this class's own bodies, while
    // slots 3 and 4 hold the base defaults, which are two DISTINCT one-and-two-instruction
    // bodies: slot 3 loads 1 into the return register and returns (PostCollisionUpdate is a
    // default `return true`), slot 4 is a bare return (Release does nothing). They do not fold
    // together. The transcribed ones are declared below in that slot
    // order, each with `override` so the compiler proves the signature still lands on the base
    // slot it is meant to fill.

    // Seed the whole behaviour: the base head, the embedded collision policy, the four rig
    // sub-objects, and the two reference-set bytes plus the two state flags.       (slot 0)
    void Construct() override;

    // Assert the attachment reference was bound, drop the prepared latch and arm the
    // first-frame latch the rig seeds itself from. Cannot fail.                    (slot 1)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;

    // Hand back the vehicle-attached collision policy embedded after the base, or null once
    // the rig has detached.                                                        (slot 5)
    CollisionPolicy* GetCollisionPolicy() override;

    // Reset the tweaker this behaviour is handed; it exposes no tweakable of its own. (slot 6)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;

    //                                                                              (slot 7)
    const char* GetName() const override;

    // Update resolves the attachment/target, detaches, and applies lag, shake and impacts.

    // Adopt a loose-attachment parameter block: assert it carries the loose-attachment type
    // tag, then store the pointer. NOT a virtual override: it is declared over the DERIVED
    // Parameters type, so it HIDES the base name rather than overriding it.
    void SetParameters(const Parameters* lpParameters);

    // ---- the non-virtual API the moments and the takedown state drive -------------------

    // Bind the ATTACHMENT reference to a race car: the rig hangs off this car. The console
    // inlines VehicleRef's own race-car binder here (index, set-flag, type, ref word -- all
    // four stores precede the range assert), which is why there is no call in the assembly.
    void AttachTo(s32 leRaceCarIndex);

    // Bind the TARGET reference to a race car: the rig looks at this car. The same inlined
    // binder, on the other reference.
    void SetTarget(s32 leRaceCarIndex);

    // The embedded impact-effect sub-object. Exposed by name so an arbitrator state never forms
    // that displacement itself (the committed shutdown-takedown beats reach the effect through
    // this accessor and then call RegisterImpact on it).
    Utils::CameraImpactEffect&       GetImpactEffect()       { return mImpact; }
    const Utils::CameraImpactEffect& GetImpactEffect() const { return mImpact; }

    // Pin the rig's target vector. MomentNewCarJoined::Update raises this byte when the return
    // blend (loose -> gameplay) starts; the console inlines the setter to its single store.
    void LockTargetVector() { mbTargetVectorLock = true; }

    // FLAG (not transcribed): the recovered declaration also carries `void Detach()`, the
    //   counterpart that raises mbDetached (the byte GetCollisionPolicy and Update both gate
    //   on). No call site in this tree reaches it and no standalone body survives in the image
    //   (every caller inlines it), so it is left undeclared rather than guessed at.
    //   DELETE-WHEN: the rig TU lands and a call site pins the store set.

private:

    // ---- layout (member NAMES, types and order recovered; see the file banner) -----------

    CollisionPolicyAttachedToVehicle mCollisionPolicy;           // +0x020 (0x250)
    Vector3                          mWorldSpaceOffsetFromCar;   // +0x270
    Utils::PositionLag               mPositionLag;               // +0x280 (0x30)
    CgsNumeric::Random               mRandom;                    // +0x2B0 (0x30, 16-aligned)
    Utils::CameraShake               mShake;                     // +0x2E0 (0x10)
    Utils::CameraImpactEffect        mImpact;                    // +0x2F0 (0x14)
    Behaviour::VehicleRef            mTarget;                    // +0x304 (0x10)
    Behaviour::VehicleRef            mAttachment;                // +0x314 (0x10)
    const Parameters*                mpParameters;               // +0x324
    f32                              mfDetachedAmount;           // +0x328
    bool                             mbDetached;                 // +0x32C
    bool                             mbFirstFrame;               // +0x32D
    bool                             mbTargetVectorLock;         // +0x32E

    // Never called, but every pin below is a static_assert: the compiler evaluates them while
    // it compiles this body, so the derived run is pinned at build time. The ABSOLUTE offsets
    // are NOT host-stable (the base head, the embedded policy and mpParameters all widen), so
    // every pin here is written size-stably -- the first derived member against the base's own
    // size, the rig run as DISPLACEMENTS between consecutive sub-objects, and the tail as
    // displacements from the detach scalar.
    static void _AssertLayout()
    {
        // mCollisionPolicy sits immediately after the base, rounded up to its own 16-byte
        // alignment -- the step that puts it at the attested +0x20 on the console.
        static_assert(offsetof(BehaviourLooseAttachment, mCollisionPolicy)
                          == ((sizeof(Behaviour) + 15u) & ~static_cast<size_t>(15u)),
                      "mCollisionPolicy follows the Behaviour base, 16-aligned");

        // The embedded policy's own stride closes the gap to the world offset with no padding.
        // Without this link the chain would skip from the base straight to the rig run, and a
        // future widening of the policy would silently shift every member after it.
        static_assert(offsetof(BehaviourLooseAttachment, mWorldSpaceOffsetFromCar)
                       - offsetof(BehaviourLooseAttachment, mCollisionPolicy)
                          == sizeof(CollisionPolicyAttachedToVehicle),
                      "the world-space offset vector follows the embedded policy with no padding");

        // ...and the policy still measures the console stride on the host (it carries no
        // pointer that would widen), which is what holds the rig run at its attested run of
        // offsets. If the policy is ever widened this fires rather than silently sliding
        // every member from the world offset onward.
        static_assert(sizeof(CollisionPolicyAttachedToVehicle) == 0x250,
                      "the embedded vehicle-attached policy holds its console stride");

        // The rig run: one 16-byte world offset, then the four sub-objects back to back.
        static_assert(offsetof(BehaviourLooseAttachment, mPositionLag)
                       - offsetof(BehaviourLooseAttachment, mWorldSpaceOffsetFromCar) == 0x10,
                      "mPositionLag follows the world-space offset vector");
        static_assert(offsetof(BehaviourLooseAttachment, mRandom)
                       - offsetof(BehaviourLooseAttachment, mPositionLag) == 0x30,
                      "mRandom follows the position lag");
        static_assert(offsetof(BehaviourLooseAttachment, mShake)
                       - offsetof(BehaviourLooseAttachment, mRandom) == 0x30,
                      "mShake follows the random generator");
        static_assert(offsetof(BehaviourLooseAttachment, mImpact)
                       - offsetof(BehaviourLooseAttachment, mShake) == 0x10,
                      "mImpact follows the shake");

        // ...and the impact effect closes exactly where the target reference begins, which is
        // what leaves the two references no slack between them.
        static_assert(offsetof(BehaviourLooseAttachment, mTarget)
                       - offsetof(BehaviourLooseAttachment, mImpact) == 0x14,
                      "mTarget follows the 20-byte impact effect with no padding");
        static_assert(offsetof(BehaviourLooseAttachment, mAttachment)
                       - offsetof(BehaviourLooseAttachment, mTarget) == 0x10,
                      "mAttachment follows the 16-byte target reference");

        // The state tail: the detach scalar, then the three flag bytes, contiguous.
        static_assert(offsetof(BehaviourLooseAttachment, mbDetached)
                       - offsetof(BehaviourLooseAttachment, mfDetachedAmount) == 0x04,
                      "mbDetached is the first flag byte after the detach scalar");
        static_assert(offsetof(BehaviourLooseAttachment, mbFirstFrame)
                       - offsetof(BehaviourLooseAttachment, mfDetachedAmount) == 0x05,
                      "mbFirstFrame is the second flag byte");
        static_assert(offsetof(BehaviourLooseAttachment, mbTargetVectorLock)
                       - offsetof(BehaviourLooseAttachment, mfDetachedAmount) == 0x06,
                      "mbTargetVectorLock is the third flag byte");
    }
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::Parameters::Construct
//   Seed the whole block. Twenty-three stores, every one to a distinct slot (nothing is written
//   twice, so the console's scheduling order carries no meaning and the seeds are grouped by
//   sub-block here). Two of the three sub-blocks are seeded with exactly the values their own
//   Construct writes -- PositionLag::Parameters (1/1/1 responses, 0.5 smoothing, muVersion left
//   alone) and CameraShake::Parameters (0.06 / 0.0 / 1.15 / 0.11) -- written out field by field
//   because the console inlines both rather than calling them.
// ----------------------------------------------------------------------------
inline void
BehaviourLooseAttachment::Parameters::Construct()
{
    meType       = eBehaviourLooseAttachment;   // stw 11, +0x00
    miParamWord1 = 0;                           // stw 0,  +0x04

    // +0x08 mPositionLagParams -- the PositionLag::Parameters seed. muVersion (+0x08) is NOT
    // written, exactly as PositionLag::Parameters::Construct leaves it (the serialiser stamps it).
    mPositionLagParams.mfXResponse = 1.0f;      // +0x0C
    mPositionLagParams.mfYResponse = 1.0f;      // +0x10
    mPositionLagParams.mfZResponse = 1.0f;      // +0x14
    mPositionLagParams.mfSmoothing = 0.5f;      // +0x18

    // +0x1C mShakeParams -- the CameraShake::Parameters seed.
    mShakeParams.mfXYShakeMagnitudeDegs  = 0.06f;   // +0x1C
    mShakeParams.mfZShakeMagnitudeDegs   = 0.0f;    // +0x20
    mShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;   // +0x24
    mShakeParams.mfWobbleCenteringFactor = 0.11f;   // +0x28

    // +0x2C mImpact -- the same shake seed again, then the three impact tunables.
    mImpact.mShakeParams.mfXYShakeMagnitudeDegs  = 0.06f;   // +0x2C
    mImpact.mShakeParams.mfZShakeMagnitudeDegs   = 0.0f;    // +0x30
    mImpact.mShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;   // +0x34
    mImpact.mShakeParams.mfWobbleCenteringFactor = 0.11f;   // +0x38
    mImpact.mfShakeDecayFactor    = 0.05f;      // +0x3C
    mImpact.mfShakeMagnitude      = 15.0f;      // +0x40
    mImpact.mfShakeFrequencyScale = 5.0f;       // +0x44

    mfPitch            = 5.0f;                  // +0x48
    mfHeight           = 1.0f;                  // +0x4C
    mfDistance         = 4.0f;                  // +0x50
    mfField54          = 90.0f;                 // +0x54
    mfDutch            = 0.0f;                  // +0x58
    mfDetachLerpAmount = 0.1f;                  // +0x5C
    mbLookFromTarget   = false;                 // +0x60  (a byte store, the only non-f32 seed)
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourLooseAttachment::SetParameters
//   lwz    r11, 0(r4)        ; lpParameters->meType
//   cmplwi r11, 0xB          ; == eBehaviourLooseAttachment
//   ... assert on mismatch ...
//   lwz    r11, 4(r4)        ; the parameter block's second word
//   stw    r4,  +0x324(r3)   ; mpParameters = lpParameters
//   stw    r11, +0x010(r3)   ; the BASE's mpcDebugParametersName
//
// PARK: the second store is the base's SetDebugParametersName(lpParameters->GetDebugName()),
//   and restoring it needs the Parameters PARK above (the parameter-bank stride pin) closed
//   first -- the block's second word only becomes a `const char*` once it derives
//   Behaviour::Parameters. Omitted rather than forged through the s32 word -- it feeds only the
//   tweaker and the debug printers, so nothing on the live camera path reads it.
// ----------------------------------------------------------------------------
inline void
BehaviourLooseAttachment::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourLooseAttachment,
               "lpParameters->GetType() == eBehaviourLooseAttachment");
    mpParameters = lpParameters;               // stw r4, +0x324(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_LOOSE_ATTACHMENT_H
