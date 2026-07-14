#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters + Utils::Looker::Parameters (embedded param sub-blocks)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h
//
// BrnDirector::Camera::BehaviourFailsafe -- the "failsafe" camera behaviour the arbitrator
// testbed falls back to: an orientation-lagged tracking camera with its own vehicle-attached
// collision policy and a key-frame animation controller. HOME for the BehaviourFailsafe slice
// this TU bodies (SetParameters + GetCollisionPolicy). The full behaviour (Construct/Prepare/
// Update and the rest of the rig) and its Behaviour base land with their own TUs; this header
// models only the slice these two functions need, BY NAME, at their asm-attested offsets.
//
// ----------------------------------------------------------------------------
// Functions homed here:
//   SetParameters       @0x821F4308 -- assert the block is a failsafe block (type tag ==
//       eBehaviourFailsafe == 12), cache its +0x04 word at +0x10, store the block pointer at
//       +0xA50.
//   GetCollisionPolicy  @0x821FABE0 -- return &mCollisionPolicy (this+0x90); a one-instruction
//       `addi r3, r3, 0x90; blr`.
//
// The member layout (DWARF BrnBehaviourFailsafe.h:56) is mOrientationLag, mOrientationLagParams,
// mCollisionPolicy, mKeyAnimController, mpParameters, mfRotationOffset on top of the Behaviour
// base; the two written offsets (+0x10 cached param word, +0xA50 param pointer) and the
// collision-policy offset (+0x90) are pinned from the asm with reserved byte spans for the rig
// members not modelled in this slice (the full layout/method set replaces this scaffold when
// the full BehaviourFailsafe TU lands).
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   failsafe one. The console value for eBehaviourFailsafe is 12 (the asm at 0x821F4328 compares
//   the block's first word against 0xC). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands; the failsafe enumerator's VALUE (12) is pinned from the asm.
enum EBehaviourTypeFailsafe
{
    eBehaviourFailsafe = 12
};

// FLAG: minimal slice of the vehicle-attached collision policy. GetCollisionPolicy returns a
//   pointer to this sub-object at +0x90; the policy's own layout/methods land with its own TU.
//   Modelled here as an opaque sized placeholder ONLY so &mCollisionPolicy resolves by name at
//   the asm-attested offset (the returned pointer's interior is not touched by this slice).
class CollisionPolicyAttachedToVehicleStub
{
public:
    u8 maOpaque[0x04];   // opaque policy head (interior not modelled in this slice)
};

class BehaviourFailsafe
{
public:

    // The failsafe parameter block: a behaviour-type tag + debug-name head, two embedded camera
    // post-process parameter sub-blocks (shake + looker), then the failsafe-specific distance/
    // height/pitch/blend tunables. GetType returns the tag SetParameters asserts on.
    //
    // Layout pinned from BehaviourFailsafe::Parameters::Serialise<S> @0x8224E9F8 (write) /
    //   @0x822324B0 (read) / @0x8224C960 (debug-menu): the shake sub-block is serialised at this+8,
    //   the looker sub-block at this+44 (0x2C), then f32 fields at this+144..+188 and a trailing
    //   bool at this+192. CameraShake::Parameters (16B) and Looker::Parameters (100B) are pointer-
    //   free, so embedding them by value reproduces the console byte offsets exactly on the LLP64
    //   host (see the _AssertLayout pins in BrnBehaviourFailsafeSerialise.cpp).
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (DebugMenu / TextFile{Read,Write}Serialiser); the per-instance body is a
        // separate TU (BrnBehaviourFailsafeSerialise.cpp). Declared so the serialiser's
        // Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeFailsafe GetType() const
        {
            return static_cast<EBehaviourTypeFailsafe>(meType);
        }

        // ---- behaviour-Parameters head (mirrors the Behaviour::Parameters base: type tag @+0x00,
        //   debug-name word @+0x04). Kept as raw size-stable words so the head is 8 bytes on the
        //   LLP64 host too (a real 8-byte name pointer would shift every later console offset). ----
        s32 meType;        // +0x00  the behaviour type tag (eBehaviourFailsafe)
        s32 miParamWord1;  // +0x04  the block's +0x04 word (the Behaviour debug-name slot; cached by SetParameters)

        // ---- embedded camera post-process parameter sub-blocks (serialised as nested sections) ----
        Utils::CameraShake::Parameters mShakeParams;   // +0x08  "Shake Params"  (CameraShake::Parameters, 16B)

        // FLAG: unrecovered failsafe fields between the shake sub-block (ends +0x18) and the looker
        //   sub-block (+0x2C). Serialise<S> walks straight from "Shake Params" to "Looker Params",
        //   so these 20 bytes are NOT serialised and no name/type is attested for them in this TU's
        //   asm; modelled as an opaque reserved span so mLookerParams lands at the asm-attested
        //   console offset (+0x2C). Name them when the full BehaviourFailsafe rig TU lands.
        u8 maUnrecovered18[0x2C - 0x18];               // +0x18 .. +0x2B  (unrecovered, not serialised)

        Utils::Looker::Parameters mLookerParams;       // +0x2C  "Looker Params" (Looker::Parameters, 100B)

        // ---- failsafe distance/height/pitch + blend-factor tunables (f32 leaves, ascending offset) ----
        f32 mfSlowDistance;                 // +0x90 (144)  "Slow Distance"
        f32 mfSlowHeight;                   // +0x94 (148)  "Slow Height"
        f32 mfSlowPitch;                    // +0x98 (152)  "Slow Pitch"
        f32 mfFastDistance;                 // +0x9C (156)  "Fast Distance"
        f32 mfFastHeight;                   // +0xA0 (160)  "Fast Height"
        f32 mfFastPitch;                    // +0xA4 (164)  "Fast Pitch"
        f32 mfField168;                     // +0xA8 (168)  label unrecovered (rodata @0x820051C0 -- see FLAG in .cpp)
        f32 mfBlendFactorBlendFactor;       // +0xAC (172)  "Blend Factor Blend Factor" (label verbatim from asm)
        f32 mfMinimumBlendFactor;           // +0xB0 (176)  "Minimum Blend Factor"
        f32 mfMaximumBlendFactor;           // +0xB4 (180)  "Maximum Blend Factor"
        f32 mfHeightDistanceBlendFactor;    // +0xB8 (184)  "Height Distance Blend Factor"
        f32 mfHeightDistanceVelocityRange;  // +0xBC (188)  "Height Distance Velocity Range"
        bool mbStickToGround;               // +0xC0 (192)  "Stick to ground"
    };

    // Adopt a failsafe parameter block: assert it carries the failsafe type tag, cache its
    // first word, then store the pointer. @0x821F4308.
    void SetParameters(const Parameters* lpParameters);

    // Return this behaviour's collision policy sub-object. @0x821FABE0 (`addi r3,r3,0x90`).
    CollisionPolicyAttachedToVehicleStub* GetCollisionPolicy() { return &mCollisionPolicy; }

private:

    // FLAG: only the members the two homed functions touch are modelled at their asm-attested
    //   offsets; the rest of the failsafe rig (mOrientationLag, mOrientationLagParams,
    //   mKeyAnimController, mfRotationOffset) lands with the full behaviour TU. SIZE-STABLE PIN:
    //   the X360 stores 4-byte pointers via `stw`; this PC reconstruction is 64-bit, so the
    //   vtable + the adopted-parameter pointer use size-stable 32-bit raw slots and the typed
    //   parameter pointer (mpParameters) is appended at the tail and reached by name. The cached
    //   param word is at +0x10 (stw r11, 0x10(this)); the collision policy is at +0x90 (the
    //   GetCollisionPolicy `addi r3,r3,0x90`); the param pointer slot is at +0xA50 (stw r31,
    //   0xA50(this)). Reserved byte spans place each field exactly.
    u8                                     maHead000[0x10];            // +0x00 .. +0x0F  vtable + rig head (X360 4B ptr slot)
    s32                                    mParamWord1;                // +0x10  cached lpParameters->miParamWord1
    u8                                     maReserved14[0x90 - 0x14];  // +0x14 .. +0x8F (mOrientationLag etc.)
    CollisionPolicyAttachedToVehicleStub   mCollisionPolicy;           // +0x90  vehicle-attached collision policy
    u8                                     maReserved94[0xA50 - 0x94]; // +0x94 .. +0xA4F (rig + key-anim controller)
    u32                                    muParametersSlot;           // +0xA50 adopted parameter block (X360 4B ptr slot)

    // x64 typed view of the adopted parameter pointer (the by-name, type-correct store target).
    // Appended at the tail so it never disturbs the pinned offsets above; the X360 packs the same
    // pointer into the 4-byte slot at +0xA50.
    const Parameters*                      mpParameters;
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourFailsafe::SetParameters @0x821F4308
//   lwz    r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 0xC            ; == eBehaviourFailsafe
//   ... assert on mismatch ...
//   lwz    r11, 4(r4)          ; lpParameters->miParamWord1
//   stw    r4,  0xA50(r3)      ; mpParameters = lpParameters
//   stw    r11, 0x10(r3)       ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourFailsafe::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourFailsafe,
               "lpParameters->GetType() == eBehaviourFailsafe");
    mpParameters = lpParameters;                   // stw r31, 0xA50(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H
