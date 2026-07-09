#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h
//
// BrnDirector::Camera::BehaviourAftertouchCam -- the "aftertouch cam" camera behaviour (the
// slow-motion crash-aftertouch follow camera the testbed / behaviour-manager installs). HOME
// for the BehaviourAftertouchCam class slice this TU bodies (SetParameters @0x821F3EA0 and the
// GetCo* sub-object accessor @0x821FB588). The full behaviour (Construct/Prepare/Update and the
// rest of the rig) and its Behaviour base land with their own TUs; this header models only the
// members these two functions touch, BY NAME, at their asm-attested offsets.
//
// ----------------------------------------------------------------------------
// SetParameters @0x821F3EA0: asserts the supplied parameter block is an aftertouch-cam block
//   (its type tag == eBehaviourAftertouchCam == 10), caches the block's first word at +0x10,
//   and stores the pointer at +0x330.
// GetCo* @0x821FB588: returns &this + 0x20 (a pointer to an embedded sub-object at +0x20);
//   a single `addi r3, r3, 0x20; blr` -- no body, just the address of the member.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   aftertouch-cam one. The console value for eBehaviourAftertouchCam is 10 (the asm at
//   0x821F3EC0 compares the block's first word against 0xA). Replace with the real
//   EBehaviourType enum when the Behaviour base TU lands; the enumerator's VALUE (10) is asm.
enum EBehaviourTypeAftertouchCam
{
    eBehaviourAftertouchCam = 10
};

class BehaviourAftertouchCam
{
public:

    // The aftertouch-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (TextFile{Read,Write}Serialiser); the per-instance body is a separate TU.
        // Declared so the serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeAftertouchCam GetType() const
        {
            return static_cast<EBehaviourTypeAftertouchCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

    // FLAG: the +0x20 sub-object the GetCo* accessor exposes. The truncated dossier name
    //   ("GetCo") and the single `addi r3, r3, 0x20; blr` body attest only that it returns the
    //   address of an embedded member at +0x20; the member's concrete type lands with the full
    //   behaviour TU. Modelled as an opaque embedded sub-object so the accessor returns a typed
    //   pointer to it at the asm-attested offset.
    class CoSubObject;

    // Return the address of the embedded sub-object at +0x20. @0x821FB588.
    CoSubObject* GetCo();

    // Adopt an aftertouch-cam parameter block: assert it carries the aftertouch-cam type tag,
    // then cache its first word and store the pointer. @0x821F3EA0.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members these two functions touch are modelled at their asm-attested
    //   offsets; the rest of the aftertouch-cam rig lands with the full behaviour TU. Reserved
    //   byte spans place them exactly. The vtable/base head occupies +0x00; the +0x20
    //   sub-object GetCo* returns; the cached param word at +0x10; the param pointer at +0x330.
    void*             mpVTable;                       // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];      // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10  cached lpParameters->miParamWord1
    u8                maReserved14[0x20 - 0x14];      // +0x14 .. +0x1F (rig members not modelled here)
    u8                maCoSubObject[0x330 - 0x20];    // +0x20  sub-object GetCo* returns (opaque)
    const Parameters* mpParameters;                   // +0x330  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourAftertouchCam::GetCo @0x821FB588
//   addi r3, r3, 0x20        ; &this->maCoSubObject
//   blr
// ----------------------------------------------------------------------------
inline BehaviourAftertouchCam::CoSubObject*
BehaviourAftertouchCam::GetCo()
{
    return reinterpret_cast<CoSubObject*>(maCoSubObject);   // this + 0x20
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourAftertouchCam::SetParameters @0x821F3EA0
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 0xA          ; == eBehaviourAftertouchCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1
//   stw  r4,  0x330(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)       ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourAftertouchCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourAftertouchCam,
               "lpParameters->GetType() == eBehaviourAftertouchCam");
    mpParameters = lpParameters;                   // stw r4,  0x330(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
