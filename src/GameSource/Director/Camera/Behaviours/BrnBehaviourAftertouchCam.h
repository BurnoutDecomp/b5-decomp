#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters (embedded "Shake Params" sub-block)

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
    //
    // Layout pinned from the Parameters::Serialise<S> field-walk asm (the three visitors at
    // 0x8224C530 / 0x8224E458 / 0x822321B0): each Process<float>/fscanf/fprintf displacement off
    // the block pointer names an f32 slot; the leading `CameraShake::Parameters::Serialise(a1+8, a2)`
    // recursion names an embedded CameraShake::Parameters at +0x08 (the "Shake Params" sub-section).
    // meType(+0x00)/miParamWord1(+0x04) are the pre-existing behaviour header words SetParameters
    // reads. All three visitors walk the SAME field sequence in the SAME order, so the offsets below
    // are authoritative; the +0x18..+0x2C span holds aftertouch-cam members none of the three
    // visitors serialise (reserved to place the walked floats at their attested offsets).
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (DebugMenu / TextFile{Read,Write}Serialiser); the per-instance body lives in
        // BrnBehaviourAftertouchCamParameters.cpp. Declared so the serialiser's Serialise<Parameters>
        // can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeAftertouchCam GetType() const
        {
            return static_cast<EBehaviourTypeAftertouchCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word

        // +0x08  embedded shake post-process tunings; walked first as the "Shake Params"
        //   sub-section (CameraShake::Parameters::Serialise(a1+8, a2) in every visitor).
        Utils::CameraShake::Parameters mShakeParams;   // +0x08 .. +0x18 (four f32)

        // +0x18 .. +0x2C  aftertouch-cam members that none of the three Serialise<S> instances
        //   walk; reserved so the serialised floats below sit at their asm-attested offsets.
        u8  maReserved18[0x2C - 0x18];

        f32 mfSlowDistance;                 // +0x2C  "Slow Distance"
        f32 mfSlowHeight;                   // +0x30  "Slow Height"
        f32 mfFastDistance;                 // +0x34  "Fast Distance"
        f32 mfFastHeight;                   // +0x38  "Fast Height"
        f32 mfPitch;                        // +0x3C  "Pitch"
        f32 mfField40;                      // +0x40  <unk_820051C0> (field label unrecovered; see cpp)
        f32 mfBlendFactorBlendFactor;       // +0x44  "Blend Factor Blend Factor"
        f32 mfMinimumBlendFactor;           // +0x48  "Minimum Blend Factor"
        f32 mfMaximumBlendFactor;           // +0x4C  "Maximum Blend Factor"
        f32 mfHeightDistanceBlendFactor;    // +0x50  "Height Distance Blend Factor"
        f32 mfHeightDistanceVelocityRange;  // +0x54  "Height Distance Velocity Range"
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
