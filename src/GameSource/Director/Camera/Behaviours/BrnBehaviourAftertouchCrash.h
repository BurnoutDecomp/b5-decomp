#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CRASH_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h
//
// BrnDirector::Camera::BehaviourAftertouchCrash -- the "aftertouch crash" camera behaviour
// (the crash-mode / takedown aftertouch camera the crash/takedown arbitrator states and the
// testbed install). HOME for the BehaviourAftertouchCrash class slice this TU bodies
// (SetParameters @0x821F4378 and the gated Get* sub-object accessor @0x821FA8F8). The full
// behaviour (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with
// their own TUs; this header models only the members these two functions touch, BY NAME, at
// their asm-attested offsets.
//
// ----------------------------------------------------------------------------
// SetParameters @0x821F4378: asserts the supplied parameter block is an aftertouch-crash block
//   (its type tag == eBehaviourAftertouchCrash == 13), caches the block's first word at +0x10,
//   and stores the pointer at +0x3D8.
// Get* @0x821FA8F8: returns &this + 0x60 (a pointer to an embedded sub-object at +0x60) UNLESS
//   the byte flag at +0x3C2 is set, in which case it returns null.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// The director camera the behaviour produces each frame (its base Behaviour's output camera at
// +0x10). Forward-declared so the Get/Set accessors below can name it; consumers that copy the
// produced camera #include Camera.h themselves.
struct Camera;

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   aftertouch-crash one. The console value for eBehaviourAftertouchCrash is 13 (the asm at
//   0x821F4398 compares the block's first word against 0xD). Replace with the real
//   EBehaviourType enum when the Behaviour base TU lands; the enumerator's VALUE (13) is asm.
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
        EBehaviourTypeAftertouchCrash GetType() const
        {
            return static_cast<EBehaviourTypeAftertouchCrash>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word
    };

    // FLAG: the +0x60 sub-object the Get* accessor exposes. The truncated dossier name ("Get")
    //   and the `lbz +0x3C2 / addi +0x60` body attest only that it returns the address of an
    //   embedded member at +0x60 (or null when the +0x3C2 flag is set); the member's concrete
    //   type lands with the full behaviour TU. Modelled as an opaque embedded sub-object so the
    //   accessor returns a typed pointer to it at the asm-attested offset.
    class GettableSubObject;

    // Return the address of the embedded sub-object at +0x60, or null when the gating flag at
    // +0x3C2 is set. @0x821FA8F8.
    GettableSubObject* Get();

    // Adopt an aftertouch-crash parameter block: assert it carries the aftertouch-crash type
    // tag, then cache its first word and store the pointer. @0x821F4378.
    void SetParameters(const Parameters* lpParameters);

    // ---- per-frame outputs the crash-mode arbitrator state drives -----------------------
    // These three are the named operations BrnArbStateCrashMode::Update / ::DoCloseup invoke on
    // the live behaviour each frame (DWARF SetRollAngleRads :108, SetCloseupAmount0To1 :113;
    // GetCamera is the base Behaviour's produced-camera accessor). DECLARATION-ONLY here -- the
    // bodies land with the full BehaviourAftertouchCrash TU; the precise produced-camera /
    // mfRollAngleRads (+0x3D0) / mfCloseupAmount0To1 (+0x3D4) offsets are this type's own concern.
    // The crash-mode state copies the produced camera and pokes only these two scalars by name,
    // never by offset.

    // The camera this behaviour produced this frame (the X360 reads it at behaviour +0x10 via the
    // handle helper @0x821FDE68); the crash-mode state copies it into its own mCamera.
    const Camera& GetCamera() const;

    // Set the camera roll angle (radians) the crash-mode tilt oscillation drives. Stores
    // mfRollAngleRads (+0x3D0). @ (with the full behaviour TU).
    void SetRollAngleRads(f32 lfRollAngleRads);

    // Set the slow-mo close-up blend [0..1] the crash-mode close-up ramps. Stores
    // mfCloseupAmount0To1 (+0x3D4). @ (with the full behaviour TU).
    void SetCloseupAmount0To1(f32 lfCloseupAmount0To1);

private:

    // FLAG: only the members these two functions touch are modelled at their asm-attested
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
    u8                maReserved3C3[0x3D8 - 0x3C3];   // +0x3C3 .. +0x3D7 (rig members not modelled here)
    const Parameters* mpParameters;                   // +0x3D8  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourAftertouchCrash::Get @0x821FA8F8
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
// BrnDirector::Camera::BehaviourAftertouchCrash::SetParameters @0x821F4378
//   lwz  r11, 0(r4)          ; lpParameters->meType
//   cmplwi r11, 0xD          ; == eBehaviourAftertouchCrash
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)          ; lpParameters->miParamWord1
//   stw  r4,  0x3D8(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)       ; mParamWord1  = lpParameters->miParamWord1
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
