#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters (embedded "Shake Params" sub-block)
#include "GameSource/AttribSys/Generated/classes/aftertouchcam.h" // Attrib::Gen::aftertouchcam (the adopted source shot)

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
// Parameters::Construct: the block's authored defaults, transcribed in full -- see it below.
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
    // are authoritative. The block's WIDTH and the slots the visitors skip come from the second
    // witness, Parameters::Construct below: the parameter bank calls it on this block and then
    // constructs the next block 108 bytes further on, and Construct itself seeds every word in
    // the +0x1C..+0x28 and +0x58..+0x68 runs that no visitor walks.
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
        //   walk. Construct below DOES seed four of the five words, so they are named slots
        //   rather than one reserved span; +0x18 is the only word nothing in this class
        //   writes or reads. FLAG: the four names are ours (no label survives for them);
        //   their offsets and their seeded values are attested.
        u8  maReserved18[4];                // +0x18  (never written, never walked)
        f32 mfField1C;                      // +0x1C
        f32 mfField20;                      // +0x20
        f32 mfField24;                      // +0x24
        f32 mfField28;                      // +0x28

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

        // +0x58 .. +0x6C  the block's tail. Like the +0x1C..+0x28 run above, none of the
        //   three Serialise<S> instances walk these, but Construct seeds every one of them,
        //   so they are named slots at their attested offsets. The block is 108 bytes: the
        //   parameter bank places the next block (an aftertouch-crash one) immediately after
        //   it, which is what fixes the size. FLAG: the five names are ours.
        f32 mfField58;                      // +0x58
        f32 mfField5C;                      // +0x5C
        f32 mfField60;                      // +0x60
        f32 mfField64;                      // +0x64
        f32 mfField68;                      // +0x68

        // ------------------------------------------------------------------
        // Parameters::Construct -- the block's authored defaults, store for store.
        //
        // The parameter bank's own Construct calls this on its FIRST named block; it is a
        // straight-line run of constant stores with no control flow, so the transcription is
        // complete rather than a slice. The four shake words are the shared
        // CameraShake::Parameters seed, spelled as the call the compiler inlined there.
        // Field order below follows the block's offsets, not the emitted store order.
        // ------------------------------------------------------------------
        void Construct()
        {
            meType       = eBehaviourAftertouchCam;   // the tag SetParameters asserts on
            miParamWord1 = 0;

            mShakeParams.Construct();                 // +0x08 .. +0x14

            mfField1C = 1.0f;
            mfField20 = 1.0f;
            mfField24 = 1.0f;
            mfField28 = 0.5f;

            mfSlowDistance                = 4.0f;
            mfSlowHeight                  = 1.75f;
            mfFastDistance                = 8.0f;
            mfFastHeight                  = 2.0f;
            mfPitch                       = 15.0f;
            mfField40                     = 90.0f;
            mfBlendFactorBlendFactor      = 0.01f;
            mfMinimumBlendFactor          = 0.001f;
            mfMaximumBlendFactor          = 0.01f;
            mfHeightDistanceBlendFactor   = 0.1f;
            mfHeightDistanceVelocityRange = 30.0f;

            mfField58 = 1.0f;
            mfField5C = 91.666664f;
            mfField60 = 0.5f;
            mfField64 = 10.0f;
            mfField68 = 1.0f;
        }
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

    // Adopt the authored shot this camera was created from. The behaviour factory builds a
    // generated aftertouchcam instance over the shot's reference spec and assigns it into the
    // behaviour's own instance member at +0x334, immediately after SetParameters; the console
    // reaches that member by displacement, so this setter's NAME is ours and its store is not.
    void SetSourceShot(const Attrib::Gen::aftertouchcam& lrShot)
    {
        mSourceShot = lrShot;
    }

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

    // The authored shot this camera came into existence through. Console +0x334, i.e.
    // immediately after the parameter pointer -- it CANNOT be placed there here, because
    // mpParameters above is a host pointer and so is twice the console's width. Parity is by
    // named member, the same rule the parameter bank's tail blocks follow.
    Attrib::Gen::aftertouchcam mSourceShot;           // console +0x334
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
