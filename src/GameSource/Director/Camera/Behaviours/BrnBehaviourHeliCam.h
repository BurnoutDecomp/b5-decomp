#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h
//
// BrnDirector::Camera::BehaviourHeliCam -- the "helicopter cam" camera behaviour: a high,
// distant tracking camera that orbits/follows the tracked car as if shot from a circling
// helicopter (used by scripted moments and the arbitrator testbed). HOME for the
// BehaviourHeliCam class slice this TU bodies (the SetParameters inline). The full behaviour
// (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with their
// own TUs; this header models only the slice SetParameters needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is SetParameters @0x821F3AA0: it asserts the supplied parameter
// block is a heli-cam block (its type tag == eBehaviourHeliCam == 6) then caches the block's
// +0x04 word at +0x10 and stores the block pointer at +0xE0. The members below are the minimal
// named scaffolding that inline needs to compile.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   heli-cam one. The console value for eBehaviourHeliCam is 6 (the asm at 0x821F3AC0 compares
//   the block's first word against 6). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands; the heli-cam enumerator's VALUE (6) is pinned from the asm.
enum EBehaviourTypeHeliCam
{
    eBehaviourHeliCam = 6
};

class BehaviourHeliCam
{
public:

    // The heli-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourTypeHeliCam GetType() const
        {
            return static_cast<EBehaviourTypeHeliCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

    // Adopt a heli-cam parameter block: assert it carries the heli-cam type tag, then cache its
    // first word and store the pointer. @0x821F3AA0.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the heli-cam rig lands with the full behaviour TU. The vtable/base head
    //   occupies +0x00; the cached param word is at +0x10 (stw r11, 0x10(this)) and the param
    //   pointer is at +0xE0 (stw r31, 0xE0(this)). Reserved byte spans place them exactly.
    void*             mpVTable;                       // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];      // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10  cached lpParameters->miParamWord1
    u8                maReserved14[0xE0 - 0x14];      // +0x14 .. +0xDF (rig members not modelled here)
    const Parameters* mpParameters;                   // +0xE0  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourHeliCam::SetParameters @0x821F3AA0
//   lwz  r11, 0(r4)         ; lpParameters->meType
//   cmplwi r11, 6           ; == eBehaviourHeliCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->miParamWord1
//   stw  r4,  0xE0(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourHeliCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourHeliCam,
               "lpParameters->GetType() == eBehaviourHeliCam");
    mpParameters = lpParameters;                   // stw r4,  0xE0(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H
