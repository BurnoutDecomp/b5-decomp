#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                         // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"        // THE canonical Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"          // CollisionPolicy(+AttachedToVehicle)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"     // Utils::CameraShake::Parameters (the
                                                                    //   "Shake Params" sub-block, by value)
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"        // Utils::PositionLag::Parameters (the lag
                                                                    //   sub-block, by value)

#include <cstddef>   // offsetof (the compile-time layout pins)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h
//
// BrnDirector::Camera::BehaviourAftertouchCrash -- the "aftertouch crash" camera behaviour: the
// crash-mode / takedown aftertouch camera the crash-mode and takedown arbitrator states and the
// arbitrator testbed install.
//
// RE-BASED. This class used to be a raw-offset SLICE: an opaque `void* mpVTable` head, a
// hand-placed `s32 mParamWord1`, reserved byte spans, an invented nested `GettableSubObject`
// type and an invented `Get()` accessor over an untyped `u8 maSubObject[]`. It now derives the
// canonical BrnDirector::Camera::Behaviour and carries the recovered member list by name.
//
// WHY IT HAD TO BE RE-BASED: the takedown state pools this behaviour through
// BehaviourManager::NewBehaviour<BehaviourAftertouchCrash>, which placement-news it into a raw
// pool slot; BehaviourManager::BehaviourHelper::Prepare then dispatches vtable slot 0. For a
// NON-POLYMORPHIC class placement-new installs no vtable, so that dispatch read a null vptr --
// EXCEPTION_ACCESS_VIOLATION reading 0 the moment the first takedown started.
//
// ----------------------------------------------------------------------------
// THE LAYOUT CHAIN (member NAMES and order from the declaration-shape reference; every offset is
// independently re-derived from the assembly, and the chain closes exactly):
//
//   base head                                  +0x000 .. +0x013   Behaviour
//   mIceCarRelativeBasis                       +0x020   Matrix44Affine (0x40, 16-aligned)
//   mCollisionPolicy                           +0x060   CollisionPolicyAttachedToVehicle (0x250)
//   mCurrentTargetPos                          +0x2B0   Vector3
//   mWorldSpaceNormalizedVectorFromCar         +0x2C0
//   mDesiredWorldSpaceNormalizedVectorFromCar  +0x2D0
//   mCrashPoint                                +0x2E0
//   mCameraPositionLastFrame                   +0x2F0
//   mPositionLag/mRandom/mShake/mImpactEffect  +0x300 .. +0x383  (reserved run, see below)
//   mfHeight                                   +0x384
//   mfDistance                                 +0x388
//   mfBlendFactor                              +0x38C
//   mfTimeSinceLastDecision                    +0x390
//   mfTimeSinceLastManualControl               +0x394
//   mManualCameraDirection                     +0x3A0   Vector3
//   mfManualHeightAdjustment                   +0x3B0   VecFloat (reserved, see below)
//   mbManualCameraControl                      +0x3C0
//   mbWasFallingDownwards                      +0x3C1
//   mbDisableCollision                         +0x3C2
//   mbIsTempDebugCrashCamera                   +0x3C3
//   mbIsRandomStartTempDebugCrashCamera        +0x3C4
//   mfDebugCrashCameraParam0to1                +0x3C8
//   mfBounceShakeMultiplier                    +0x3CC
//   mfRollAngleRads                            +0x3D0
//   mfCloseupAmount0To1                        +0x3D4
//   mpParameters                               +0x3D8
//
// Two anchors pin the whole chain, and they are what the retired slice got wrong:
//   * mCollisionPolicy lands at +0x60 only because the base head is 0x14 and the 16-aligned
//     Matrix44Affine that follows occupies +0x20 .. +0x5F -- i.e. 0x20 + 0x40 == 0x60. The
//     retired slice modelled +0x60 as an untyped byte blob behind an invented accessor; it is
//     the vehicle-attached collision policy, and the accessor is the base's own slot-5 virtual.
//   * the five flag bytes land at +0x3C0 .. +0x3C4 exactly, which is what makes the +0x3C2 byte
//     the takedown state raises mbDisableCollision and the +0x3C3 byte mbIsTempDebugCrashCamera.
//
// x64: parity is BY NAMED MEMBER (pointers and the embedded policy widen); the offsets above are
// provenance and are never used as casts.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   aftertouch-crash one. The value 13 is attested (the console compares the block's first word
//   against 13). There is still no single homed EBehaviourType enum -- each behaviour's tag is
//   only observable in its own assert.
enum EBehaviourTypeAftertouchCrash
{
    eBehaviourAftertouchCrash = 13
};

class BehaviourAftertouchCrash : public Behaviour
{
public:

    // The aftertouch-crash parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    //
    // PARK: this block cannot derive Behaviour::Parameters (which is what the recovered
    //   declaration has) until the parameter-bank lane re-expresses BrnBehaviourParameterBank.h's
    //   stride pin, and the reserved span it sizes, as
    //   sizeof(Camera::BehaviourAftertouchCrash::Parameters) instead of a console literal.
    class Parameters
    {
    public:
        // Console visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (DebugMenuSerialiser / TextFile{Read,Write}Serialiser). The ONE templated
        // field-walk body + its three explicit instantiations (DebugMenu, write, read) are bodied
        // in this TU's .cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeAftertouchCrash GetType() const
        {
            return static_cast<EBehaviourTypeAftertouchCrash>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word

        // The shake post-process tunings sub-block ("Shake Params" section) the field-walk visitor
        // recurses into first (DebugMenu AddToPath+recurse, write, read
        //). Homed, by value, in BehaviourRig.h (four f32 tunables, +0x08..+0x17).
        Utils::CameraShake::Parameters mShakeParams;   // +0x08 .. +0x17

        // The position-lag tunings sub-block at +0x18 .. +0x2B, which the field-walk does NOT
        // visit (it jumps straight from the shake sub-block to the +0x2C f32 tunables) but which
        // Parameters::Construct seeds. Named + typed from this struct's own declaration (mLagParams);
        // its 0x14-byte stride is what carries the walked tunables to their attested +0x2C.
        Utils::PositionLag::Parameters mLagParams;   // +0x18 .. +0x2B

        // The serialised f32 tunables at their attested offsets. The write serialiser's
        // a1[N] float displacements confirm each offset (a1[11]=+0x2C .. a1[22]=+0x58); note the walk
        // visits +0x40 (mfFOV) BEFORE +0x3C (mfPitch), and skips the +0x50 word (a1[20] unused).
        // The tunable NAMES are this struct's own; the four distance/height ones keep the
        // serialiser's own "Slow"/"Fast" labels, which is what this build's field-walk strings say.
        f32 mfSlowDistance;                 // +0x2C  "Slow Distance"
        f32 mfSlowHeight;                   // +0x30  "Slow Height"
        f32 mfFastDistance;                 // +0x34  "Fast Distance"
        f32 mfFastHeight;                   // +0x38  "Fast Height"
        f32 mfPitch;                        // +0x3C  "Pitch"
        f32 mfFOV;                          // +0x40  <label unrecovered -- see .cpp FLAG>
        f32 mfBlendFactorBlendFactor;       // +0x44  "Blend Factor Blend Factor"
        f32 mfMinimumBlendFactor;           // +0x48  "Minimum Blend Factor"
        f32 mfMaximumBlendFactor;           // +0x4C  "Maximum Blend Factor"
        f32 mfManualBlendFactor;            // +0x50  rig word the field-walk skips (write a1[20] unused)
        f32 mfHeightDistanceBlendFactor;    // +0x54  "Height Distance Blend Factor"
        f32 mfHeightDistanceVelocityRange;  // +0x58  "Height Distance Velocity Range"

        // The rival-selection tail. Not visited by the field-walk (it stops at +0x58), but every
        // one of these is written by Parameters::Construct below, which is what pins the block's
        // 0x70-byte stride -- the stride the parameter bank's head run rests on.
        f32 mfTimeToRivalImpactUncertaintyPadding;    // +0x5C
        f32 mfMaximumDistanceForConsiderationOfRivals; // +0x60
        f32 mfTimingSimilarityThreshold;              // +0x64
        f32 mfDistanceSimilarityThreshold;            // +0x68
        f32 mfTimeBetweenDecisions;                   // +0x6C

        // Seed this block with its authored defaults (including the type tag SetParameters
        // asserts on). A straight-line run of constant stores.
        void Construct();

        // Never called, but every pin below is a static_assert: the compiler evaluates them while
        // it compiles this body, so the serialised-field offsets are enforced at build time.
        // Every field here precedes any pointer member, so these offsets are host-pointer-width
        // invariant and can be pinned absolutely.
        static void _AssertLayout()
        {
            static_assert(offsetof(Parameters, mShakeParams) == 0x08,
                          "mShakeParams @ +0x08");
            static_assert(offsetof(Parameters, mfSlowDistance) == 0x2C,
                          "mfSlowDistance @ +0x2C");
            static_assert(offsetof(Parameters, mfPitch) == 0x3C,
                          "mfPitch @ +0x3C");
            static_assert(offsetof(Parameters, mfFOV) == 0x40,
                          "mfFOV @ +0x40");
            static_assert(offsetof(Parameters, mfHeightDistanceVelocityRange) == 0x58,
                          "mfHeightDistanceVelocityRange @ +0x58");
            static_assert(offsetof(Parameters, mfTimeBetweenDecisions) == 0x6C,
                          "mfTimeBetweenDecisions @ +0x6C");
        }
    };

    // ---- the virtual interface ----------------------------------------------------------
    // The base's interface is EIGHT slots (Construct / Prepare / Update / PostCollisionUpdate /
    // Release / GetCollisionPolicy / SetupTweaker / GetName; GetParameters/SetParameters are not
    // virtual and there is no destructor slot). The declaration-shape reference has this class
    // overriding SIX of
    // them -- slots 0, 1, 2, 5, 6 and 7 -- and appending NO extra virtuals of its own, so the
    // derived vtable is the base's eight slots with those six re-pointed. The four transcribed
    // below are declared in that slot order, each with `override` so the compiler proves the
    // signature still lands on the base slot it is meant to fill.

    // Seed the whole behaviour: the base head, the embedded collision policy plus its four
    // authored flag overrides, the rig sub-objects, and the flag/scalar tail.        (slot 0)
    void Construct() override;

    // Drop the prepared latch, then seed the running blend/height/distance from the adopted
    // parameter block and re-arm the debug-crash-camera parameter. Cannot fail.      (slot 1)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;

    // Hand back the vehicle-attached collision policy embedded after the basis, or null when
    // collision has been disabled on this instance.                                  (slot 5)
    CollisionPolicy* GetCollisionPolicy() override;

    //                                                                                (slot 7)
    const char* GetName() const override;

    // FLAG (not transcribed): the recovered declaration also carries
    //   `virtual bool Update(Camera&, const BehaviourSharedInfo&)` -- the ~430-line aftertouch
    //   crash rig -- and `virtual void SetupTweaker(Tweaker&)`, plus the private helper
    //   `bool CheckForPlayerCarBouncing(const BehaviourSharedInfo&)`. Neither is declared here,
    //   so slots 2 and 6 keep the base's defaults (Update returns true and leaves the camera
    //   untouched; SetupTweaker does nothing). That is a DOCUMENTED GAP, not a fabrication --
    //   the alternative would be inventing a camera rig.
    //   DELETE-WHEN: the rig TU lands and bodies Update/SetupTweaker.

    // Adopt an aftertouch-crash parameter block: assert it carries the aftertouch-crash type
    // tag, then store the pointer. NOT a virtual override: it is declared over the DERIVED
    // Parameters type, so it HIDES the base name rather than overriding it.
    void SetParameters(const Parameters* lpParameters);

    // ---- the two flags the takedown state raises on the debug crash cam -----------------
    // The takedown state's Prepare raises both bytes immediately after SetParameters, with a
    // literal 1 in each (`stb r26, +0x3C2` / `stb r26, +0x3C3`, with r26 loaded `li r26, 1` in
    // the prologue). Both member names and both setter declarations are recovered, not invented.

    // Suppress this instance's collision policy -- GetCollisionPolicy then returns null.
    void DisableCollision() { mbDisableCollision = true; }                 // stb 1, +0x3C2

    // Mark this instance as the temporary debug crash camera.
    void SetIsTempDebugCrashCamera() { mbIsTempDebugCrashCamera = true; }  // stb 1, +0x3C3

    // ---- per-frame outputs the crash-mode arbitrator state drives -----------------------
    // The two named operations the crash-mode state's Update / DoCloseup invoke on the live
    // behaviour each frame. Neither has a console symbol of its own: both call sites inline the
    // setter to its single store, so the bodies below ARE the whole function.
    //
    // There is no GetCamera() on this class. The produced camera is NOT read off the behaviour:
    // the crash-mode state copies it through its BehaviourHandle's GetProducedCamera(), which
    // resolves the manager's BehaviourHelper slot and returns the helper's own embedded Camera.

    // Set the camera roll angle (radians) the crash-mode tilt oscillation drives.
    void SetRollAngleRads(f32 lfRollAngleRads)
    {
        mfRollAngleRads = lfRollAngleRads;          // stfs +0x3D0
    }

    // Set the slow-mo close-up blend [0..1] the crash-mode close-up ramps.
    void SetCloseupAmount0To1(f32 lfCloseupAmount0To1)
    {
        mfCloseupAmount0To1 = lfCloseupAmount0To1;  // stfs +0x3D4
    }

private:

    // ---- layout (member NAMES and order recovered; see the file banner) -----------------
    // The Behaviour base occupies the head. Three reserved runs stand in for rig members whose
    // types are named in the recovered declaration but are not homed for this slice to embed; they are
    // placeholders for REAL members, not invented ones, and nothing reads them.

    Matrix44Affine                   mIceCarRelativeBasis;                      // +0x020 (0x40)
    CollisionPolicyAttachedToVehicle mCollisionPolicy;                          // +0x060 (0x250)
    Vector3                          mCurrentTargetPos;                         // +0x2B0
    Vector3                          mWorldSpaceNormalizedVectorFromCar;        // +0x2C0
    Vector3                          mDesiredWorldSpaceNormalizedVectorFromCar; // +0x2D0
    Vector3                          mCrashPoint;                               // +0x2E0
    Vector3                          mCameraPositionLastFrame;                  // +0x2F0

    // +0x300 .. +0x383 -- mPositionLag (PositionLag), mRandom (Random), mShake (CameraShake) and
    // mImpactEffect (CameraImpactEffect), in that declared order. Held as one run until those four
    // types are homed; the rig TU carves them out.
    u8                               maReservedRigSubObjects[0x384 - 0x300];

    f32                              mfHeight;                      // +0x384
    f32                              mfDistance;                    // +0x388
    f32                              mfBlendFactor;                 // +0x38C
    f32                              mfTimeSinceLastDecision;       // +0x390
    f32                              mfTimeSinceLastManualControl;  // +0x394

    // +0x398 .. +0x39F -- the alignment gap ahead of the 16-aligned Vector3 below.
    u8                               maReservedAlign398[0x3A0 - 0x398];

    Vector3                          mManualCameraDirection;        // +0x3A0

    // +0x3B0 .. +0x3BF -- mfManualHeightAdjustment (VecFloat, a 16-byte vector scalar). Held as a
    // run until VecFloat is homed here; nothing in this slice reads it.
    u8                               maReservedManualHeight[0x3C0 - 0x3B0];

    bool                             mbManualCameraControl;               // +0x3C0
    bool                             mbWasFallingDownwards;               // +0x3C1
    bool                             mbDisableCollision;                  // +0x3C2
    bool                             mbIsTempDebugCrashCamera;            // +0x3C3
    bool                             mbIsRandomStartTempDebugCrashCamera; // +0x3C4
    u8                               maReservedAlign3C5[0x3C8 - 0x3C5];

    f32                              mfDebugCrashCameraParam0to1;   // +0x3C8
    f32                              mfBounceShakeMultiplier;       // +0x3CC
    f32                              mfRollAngleRads;               // +0x3D0
    f32                              mfCloseupAmount0To1;           // +0x3D4
    const Parameters*                mpParameters;                  // +0x3D8

    // Never called, but every pin below is a static_assert: the compiler evaluates them while it
    // compiles this body, so the derived run is pinned at build time. The ABSOLUTE offsets are
    // NOT host-stable (the base head and the embedded policy both widen), so every pin here is
    // written size-stably -- the first derived member against the base's own size, and the tail
    // run as DISPLACEMENTS from the first of the five flag bytes, which is the run the takedown
    // state, the crash-mode state and Prepare all reach into.
    static void _AssertLayout()
    {
        // mIceCarRelativeBasis sits immediately after the base, rounded up to its own 16-byte
        // alignment -- the step that puts mCollisionPolicy at the attested +0x60 on the console.
        static_assert(offsetof(BehaviourAftertouchCrash, mIceCarRelativeBasis)
                          == ((sizeof(Behaviour) + 15u) & ~static_cast<size_t>(15u)),
                      "mIceCarRelativeBasis follows the Behaviour base, 16-aligned");

        // The five flag bytes are contiguous.
        static_assert(offsetof(BehaviourAftertouchCrash, mbWasFallingDownwards)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x01,
                      "mbWasFallingDownwards is the second flag byte");
        static_assert(offsetof(BehaviourAftertouchCrash, mbDisableCollision)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x02,
                      "mbDisableCollision is the third flag byte");
        static_assert(offsetof(BehaviourAftertouchCrash, mbIsTempDebugCrashCamera)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x03,
                      "mbIsTempDebugCrashCamera is the fourth flag byte");
        static_assert(offsetof(BehaviourAftertouchCrash, mbIsRandomStartTempDebugCrashCamera)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x04,
                      "mbIsRandomStartTempDebugCrashCamera is the fifth flag byte");

        // ...and the scalar tail follows them at its attested displacements.
        static_assert(offsetof(BehaviourAftertouchCrash, mfDebugCrashCameraParam0to1)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x08,
                      "mfDebugCrashCameraParam0to1 sits at the flag run +0x08");
        static_assert(offsetof(BehaviourAftertouchCrash, mfRollAngleRads)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x10,
                      "mfRollAngleRads sits at the flag run +0x10");
        static_assert(offsetof(BehaviourAftertouchCrash, mfCloseupAmount0To1)
                       - offsetof(BehaviourAftertouchCrash, mbManualCameraControl) == 0x14,
                      "mfCloseupAmount0To1 sits at the flag run +0x14");

        // The running rig scalars Prepare seeds are a contiguous f32 run.
        static_assert(offsetof(BehaviourAftertouchCrash, mfDistance)
                       - offsetof(BehaviourAftertouchCrash, mfHeight) == 0x04,
                      "mfDistance follows mfHeight");
        static_assert(offsetof(BehaviourAftertouchCrash, mfBlendFactor)
                       - offsetof(BehaviourAftertouchCrash, mfHeight) == 0x08,
                      "mfBlendFactor follows mfDistance");
    }
};

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Parameters::Construct -- the block's authored defaults, a
// straight-line run of constant stores covering every word from +0x00 to +0x6C except the lag
// block's leading version word. This is what the parameter bank calls on the two aftertouch-crash
// blocks in its head run, and it is what makes SetParameters' type-tag tripwire pass.
// ----------------------------------------------------------------------------
inline void
BehaviourAftertouchCrash::Parameters::Construct()
{
    meType       = eBehaviourAftertouchCrash;   // stw 13, +0x00
    miParamWord1 = 0;                           // stw 0,  +0x04

    // The shake sub-block's four words are exactly its own Construct's seed.
    mShakeParams.Construct();                   // +0x08 .. +0x17

    // The lag sub-block: the console writes only its four response/smoothing words and leaves
    // muVersion (+0x18) alone -- it does NOT call PositionLag::Parameters::Construct here.
    mLagParams.mfXResponse = 1.0f;              // +0x1C
    mLagParams.mfYResponse = 1.0f;              // +0x20
    mLagParams.mfZResponse = 1.0f;              // +0x24
    mLagParams.mfSmoothing = 0.5f;              // +0x28

    mfSlowDistance              = 4.0f;         // +0x2C
    mfSlowHeight                = 1.75f;        // +0x30
    mfFastDistance              = 9.0f;         // +0x34
    mfFastHeight                = 2.0f;         // +0x38
    mfPitch                     = 0.0f;         // +0x3C
    mfFOV                       = 80.0f;        // +0x40
    mfBlendFactorBlendFactor    = 0.0099999998f; // +0x44
    mfMinimumBlendFactor        = 0.000099999997f; // +0x48
    mfMaximumBlendFactor        = 0.0099999998f; // +0x4C
    mfManualBlendFactor         = 0.94999999f;  // +0x50
    mfHeightDistanceBlendFactor = 0.1f;         // +0x54
    mfHeightDistanceVelocityRange = 25.0f;      // +0x58

    mfTimeToRivalImpactUncertaintyPadding    = 1.0f;       // +0x5C
    mfMaximumDistanceForConsiderationOfRivals = 91.666664f; // +0x60
    mfTimingSimilarityThreshold              = 0.5f;       // +0x64
    mfDistanceSimilarityThreshold            = 10.0f;      // +0x68
    mfTimeBetweenDecisions                   = 1.0f;       // +0x6C
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourAftertouchCrash::SetParameters
//   lwz    r11, 0(r4)        ; lpParameters->meType
//   cmplwi r11, 0xD          ; == eBehaviourAftertouchCrash
//   ... assert on mismatch ...
//   lwz    r11, 4(r4)        ; the parameter block's second word
//   stw    r4,  +0x3D8(r3)   ; mpParameters = lpParameters
//   stw    r11, +0x10(r3)    ; the BASE's mpcDebugParametersName
//
// PARK: the second store is the base's SetDebugParametersName(lpParameters->GetDebugName()), and
//   restoring it needs the Parameters PARK above (the parameter-bank stride pin) closed first.
//   Omitted rather than forged through the s32 word -- it feeds only the tweaker and the debug
//   printers, so nothing on the live camera path reads it.
// ----------------------------------------------------------------------------
inline void
BehaviourAftertouchCrash::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourAftertouchCrash,
               "lpParameters->GetType() == eBehaviourAftertouchCrash");
    mpParameters = lpParameters;                   // stw r4, +0x3D8(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H
