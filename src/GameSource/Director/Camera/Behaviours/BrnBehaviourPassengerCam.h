#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_PASSENGER_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_PASSENGER_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.h
//
// BrnDirector::Camera::BehaviourPassengerCam -- the "passenger cam" camera behaviour: an
// in-car camera mounted at the passenger seat that looks where a passenger would (used by the
// "passenger sees action" scripted moment and the arbitrator testbed). HOME for the
// BehaviourPassengerCam class slice this TU bodies (the SetParameters inline). The full
// behaviour (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land
// with their own TUs; this header models only the slice SetParameters needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is SetParameters @0x821F3A30: it asserts the supplied parameter
// block is a passenger-cam block (its type tag == eBehaviourPassengerCam == 7) then caches the
// block's +0x04 word at +0x10 and stores the block pointer at +0x14. The members below are the
// minimal named scaffolding that inline needs to compile.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   passenger-cam one. The console value for eBehaviourPassengerCam is 7 (the asm at 0x821F3A50
//   compares the block's first word against 7). Replace with the real EBehaviourType enum when
//   the Behaviour base TU lands; the passenger-cam enumerator's VALUE (7) is pinned from asm.
enum EBehaviourTypePassengerCam
{
    eBehaviourPassengerCam = 7
};

class BehaviourPassengerCam
{
public:

    // The passenger-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (TextFile{Read,Write}Serialiser); the per-instance body is a separate TU.
        // Declared so the serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypePassengerCam GetType() const
        {
            return static_cast<EBehaviourTypePassengerCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

    // Adopt a passenger-cam parameter block: assert it carries the passenger-cam type tag, then
    // cache its first word and store the pointer. @0x821F3A30.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the passenger-cam rig lands with the full behaviour TU. The vtable/base head
    //   occupies +0x00; the cached param word is at +0x10 (stw r11, 0x10(this)) and the param
    //   pointer is at +0x14 (stw r31, 0x14(this)). Reserved byte span places the cached word
    //   exactly.
    void*             mpVTable;                       // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];      // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10  cached lpParameters->miParamWord1
    const Parameters* mpParameters;                   // +0x14  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourPassengerCam::SetParameters @0x821F3A30
//   lwz  r11, 0(r4)         ; lpParameters->meType
//   cmplwi r11, 7           ; == eBehaviourPassengerCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->miParamWord1
//   stw  r4,  0x14(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourPassengerCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourPassengerCam,
               "lpParameters->GetType() == eBehaviourPassengerCam");
    mpParameters = lpParameters;                   // stw r4,  0x14(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_PASSENGER_CAM_H
