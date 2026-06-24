#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

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

    // The failsafe parameter block: a type tag in its leading word plus behaviour-specific data.
    // GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourTypeFailsafe GetType() const
        {
            return static_cast<EBehaviourTypeFailsafe>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
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
