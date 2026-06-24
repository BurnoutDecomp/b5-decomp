#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_RIG_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_RIG_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRig.h
//
// BrnDirector::Camera::BehaviourRig -- the "rig" camera behaviour: a composable camera mount
// that drives the camera off an authored rig (used by the takedown-lookback moment, the
// arbitrator testbed and the camera-script helpers sub_82267D88 / sub_82267ED0). HOME for the
// BehaviourRig class slice this TU bodies (the SetParameters inline). The full behaviour
// (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with their
// own TUs; this header models only the slice SetParameters needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is SetParameters @0x821F3B10: it asserts the supplied parameter
// block is a rig block (its type tag == eBehaviourRig == 2) then caches the block's +0x04 word
// at +0x10, clears a byte flag at +0x08, and stores the block pointer at +0x460. The members
// below are the minimal named scaffolding that inline needs to compile.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the rig
//   one. The console value for eBehaviourRig is 2 (the asm at 0x821F3B30 compares the block's
//   first word against 2). Replace with the real EBehaviourType enum when the Behaviour base TU
//   lands; the rig enumerator's VALUE (2) is pinned from the asm.
enum EBehaviourTypeRig
{
    eBehaviourRig = 2
};

class BehaviourRig
{
public:

    // The rig parameter block: a type tag in its leading word plus behaviour-specific data.
    // GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourTypeRig GetType() const
        {
            return static_cast<EBehaviourTypeRig>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

    // Adopt a rig parameter block: assert it carries the rig type tag, then cache its first
    // word, clear a byte flag and store the pointer. @0x821F3B10.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the rig lands with the full behaviour TU. The vtable/base head occupies
    //   +0x00; a byte flag is cleared at +0x08 (stb r10, 8(this)); the cached param word is at
    //   +0x10 (stw r11, 0x10(this)); the param pointer is at +0x460 (stw r30, 0x460(this)).
    //   Reserved byte spans place them exactly.
    void*             mpVTable;                       // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x08 - 0x04];      // +0x04 .. +0x07 (rig members not modelled here)
    u8                mbFlag08;                        // +0x08  byte flag cleared on adopt
    u8                maReserved09[0x10 - 0x09];      // +0x09 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10  cached lpParameters->miParamWord1
    u8                maReserved14[0x460 - 0x14];     // +0x14 .. +0x45F (rig members not modelled here)
    const Parameters* mpParameters;                   // +0x460  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourRig::SetParameters @0x821F3B10
//   lwz  r11, 0(r4)         ; lpParameters->meType
//   cmplwi r11, 2           ; == eBehaviourRig
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->miParamWord1
//   li   r10, 0
//   stw  r4,  0x460(r3)     ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; mParamWord1  = lpParameters->miParamWord1
//   stb  r10, 8(r3)         ; mbFlag08     = 0
// ----------------------------------------------------------------------------
inline void
BehaviourRig::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourRig,
               "lpParameters->GetType() == eBehaviourRig");
    mpParameters = lpParameters;                   // stw r4,  0x460(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
    mbFlag08     = 0;                                // stb r10(=0), 8(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_RIG_H
