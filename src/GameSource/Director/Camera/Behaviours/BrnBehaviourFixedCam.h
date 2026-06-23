#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FIXED_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FIXED_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h
//
// BrnDirector::Camera::BehaviourFixedCam -- the "fixed cam" camera behaviour: it holds the
// camera at a fixed authored world position/orientation (used by scripted moments such as the
// static-cam impact and the arbitrator testbed). HOME for the BehaviourFixedCam class slice
// this TU bodies (the SetParameters inline). The full behaviour (Construct/Prepare/Update and
// the rest of the rig) and its Behaviour base land with their own TUs; this header models only
// the slice SetParameters needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is SetParameters @0x821F4518: it asserts the supplied
// parameter block is a fixed-cam block (its type tag == eBehaviourFixedCam == 15) and stores
// the pointer in mpParameters. Everything else below is the minimal named scaffolding that
// inline needs to compile.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id
//   in the leading word of their Parameters block; SetParameters asserts the block's id is
//   the fixed-cam one. The console value for eBehaviourFixedCam is 15 (the asm at 0x821F4538
//   compares the block's first word against 0xF). Replace with the real EBehaviourType enum
//   when the Behaviour base TU lands; the fixed-cam enumerator's VALUE (15) is pinned from asm.
enum EBehaviourTypeFixedCam
{
    eBehaviourFixedCam = 15
};

// FLAG: minimal slice of the camera-behaviour base. The full Behaviour base (vtable + shared
//   flag/state block) lands with its own TU; SetParameters only needs the typed parameter
//   pointer member to exist on the derived class, so the base is modelled as an opaque head
//   here. The real base layout/method set replaces this when the Behaviour TU lands.
class BehaviourFixedCam
{
public:

    // The fixed-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        EBehaviourTypeFixedCam GetType() const
        {
            return static_cast<EBehaviourTypeFixedCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

    // Adopt a fixed-cam parameter block: assert it carries the fixed-cam type tag, then cache
    // its first word and store the pointer. @0x821F4518.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the fixed-cam rig lands with the full behaviour TU. The vtable/base head
    //   occupies +0x00; the cached param word is at +0x10 (stw r11, 0x10(this)) and the param
    //   pointer is at +0x2A0 (stw r31, 0x2A0(this)). Reserved byte spans place them exactly.
    void*             mpVTable;                       // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];      // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10  cached lpParameters->miParamWord1
    u8                maReserved14[0x2A0 - 0x14];     // +0x14 .. +0x29F (rig members not modelled here)
    const Parameters* mpParameters;                   // +0x2A0  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourFixedCam::SetParameters @0x821F4518
//   lwz  r11, 0(r4)         ; lpParameters->meType
//   cmplwi r11, 0xF         ; == eBehaviourFixedCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->miParamWord1
//   stw  r4,  0x2A0(r3)     ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourFixedCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourFixedCam,
               "lpParameters->GetType() == eBehaviourFixedCam");
    mpParameters = lpParameters;                  // stw r4,  0x2A0(this)
    mParamWord1  = lpParameters->miParamWord1;     // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FIXED_CAM_H
