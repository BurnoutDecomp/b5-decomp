#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert + _AssertLayout)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"   // Utils::CameraShake::Parameters (the "Shake Params" sub-block, embedded by value @+0x08)
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"   // Utils::PositionLag::Parameters (the lag sub-block, embedded by value @+0x18)

#include <cstddef>   // offsetof (the never-called _AssertLayout pin)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h
//
// BrnDirector::Camera::BehaviourAftertouchCrash -- the "aftertouch crash" camera behaviour
// (the crash-mode / takedown aftertouch camera the crash/takedown arbitrator states and the
// testbed install). HOME for the BehaviourAftertouchCrash class slice this TU bodies
// (SetParameters and the gated Get* sub-object accessor). The full
// behaviour (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with
// their own TUs; this header models only the members these two functions touch, BY NAME, at
// their attested offsets.
//
// ----------------------------------------------------------------------------
// SetParameters: asserts the supplied parameter block is an aftertouch-crash block
//   (its type tag == eBehaviourAftertouchCrash == 13), caches the block's first word at +0x10,
//   and stores the pointer at +0x3D8.
// Get*: returns &this + 0x60 (a pointer to an embedded sub-object at +0x60) UNLESS
//   the byte flag at +0x3C2 is set, in which case it returns null.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   aftertouch-crash one. The console value for eBehaviourAftertouchCrash is 13 (the console compares the block's first word against 13). Replace with the real
//   EBehaviourType enum when the Behaviour base TU lands; the enumerator's VALUE (13) is attested.
enum EBehaviourTypeAftertouchCrash
{
    eBehaviourAftertouchCrash = 13
};

class BehaviourAftertouchCrash
{
public:

    // The aftertouch-crash parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
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

        // Never called: pin the serialised-field offsets against the console's stores. Every field here
        // precedes any pointer member, so these offsets are host-pointer-width invariant.
        static void _AssertLayout()
        {
            CGS_ASSERT(offsetof(Parameters, mShakeParams) == 0x08,
                       "mShakeParams @ +0x08");
            CGS_ASSERT(offsetof(Parameters, mfSlowDistance) == 0x2C,
                       "mfSlowDistance @ +0x2C");
            CGS_ASSERT(offsetof(Parameters, mfPitch) == 0x3C,
                       "mfPitch @ +0x3C");
            CGS_ASSERT(offsetof(Parameters, mfFOV) == 0x40,
                       "mfFOV @ +0x40");
            CGS_ASSERT(offsetof(Parameters, mfHeightDistanceVelocityRange) == 0x58,
                       "mfHeightDistanceVelocityRange @ +0x58");
            CGS_ASSERT(offsetof(Parameters, mfTimeBetweenDecisions) == 0x6C,
                       "mfTimeBetweenDecisions @ +0x6C");
        }
    };

    // FLAG: the +0x60 sub-object the Get* accessor exposes. The truncated dossier name ("Get")
    //   and the `lbz +0x3C2 / addi +0x60` body attest only that it returns the address of an
    //   embedded member at +0x60 (or null when the +0x3C2 flag is set); the member's concrete
    //   type lands with the full behaviour TU. Modelled as an opaque embedded sub-object so the
    //   accessor returns a typed pointer to it at the attested offset.
    class GettableSubObject;

    // Return the address of the embedded sub-object at +0x60, or null when the gating flag at
    // +0x3C2 is set..
    GettableSubObject* Get();

    // Adopt an aftertouch-crash parameter block: assert it carries the aftertouch-crash type
    // tag, then cache its first word and store the pointer..
    void SetParameters(const Parameters* lpParameters);

    // ---- per-frame outputs the crash-mode arbitrator state drives -----------------------
    // The two named operations BrnArbStateCrashMode::Update / ::DoCloseup invoke on the live
    // behaviour each frame (SetRollAngleRads / SetCloseupAmount0To1). Neither has
    // a console symbol of its own: both call sites inline the setter to its single store, so the
    // bodies below ARE the whole function. The crash-mode state pokes only these two scalars,
    // by name.
    //
    // There is no GetCamera() on this class. The produced camera is NOT read off the behaviour:
    // the crash-mode state copies it through its BehaviourHandle's GetProducedCamera(), which
    // resolves the manager's BehaviourHelper slot and returns the helper's own embedded Camera.
    // (The behaviour's +0x10 word is mParamWord1 below -- SetParameters writes it -- so the old
    // "produced camera at behaviour +0x10" note was reading the handle-side accessor's offset as
    // a behaviour-side one.)

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

    // FLAG: only the members these two functions touch are modelled at their attested
    //   offsets; the rest of the aftertouch-crash rig lands with the full behaviour TU. Reserved
    //   byte spans place them exactly. The vtable/base head occupies +0x00; the cached param word
    //   at +0x10; the +0x60 sub-object Get* returns; the byte gating flag at +0x3C2; the param
    //   pointer at +0x3D8.
    void*             mpVTable;                       // +0x00   behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];      // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10   cached lpParameters->miParamWord1
    u8                maReserved14[0x60 - 0x14];      // +0x14 .. +0x5F (rig members not modelled here)
    u8                maSubObject[0x3C2 - 0x60];      // +0x60   sub-object Get* returns (opaque)
    u8                mbGetGated;                     // +0x3C2  when set, Get* returns null
    u8                maReserved3C3[0x3D0 - 0x3C3];   // +0x3C3 .. +0x3CF (rig members not modelled here)
    f32               mfRollAngleRads;                // +0x3D0  the camera roll SetRollAngleRads drives
    f32               mfCloseupAmount0To1;            // +0x3D4  the close-up blend SetCloseupAmount0To1 drives
    const Parameters* mpParameters;                   // +0x3D8  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourAftertouchCrash::Get
//   lbz    r11, 0x3C2(r3)       ; mbGetGated
//   addi   r3,  r3, 0x60        ; r3 = &this->maSubObject (the candidate return)
//   cmplwi r11, 0
//   beqlr                       ; flag clear -> return &maSubObject
//   li     r3, 0                ; flag set   -> return null
//   blr
// ----------------------------------------------------------------------------
inline BehaviourAftertouchCrash::GettableSubObject*
BehaviourAftertouchCrash::Get()
{
    if (mbGetGated)                                                  // lbz +0x3C2; bne -> null
    {
        return 0;
    }
    return reinterpret_cast<GettableSubObject*>(maSubObject);        // this + 0x60
}

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
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 0xD          ; == eBehaviourAftertouchCrash
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1
//   stw  r4,  +0x3D8(r3)     ; mpParameters = lpParameters
//   stw  r11, +0x10(r3)      ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourAftertouchCrash::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourAftertouchCrash,
               "lpParameters->GetType() == eBehaviourAftertouchCrash");
    mpParameters = lpParameters;                   // stw r4,  0x3D8(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H
