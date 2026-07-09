#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_EXTERNAL_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_EXTERNAL_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the Parameters::Set FOV sanity asserts)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h
//
// BrnDirector::Camera::BehaviourGameplayExternal -- the default external ("chase") gameplay
// camera behaviour: the third-person camera that trails the player car, with its own FOV /
// boost-FOV, follow height/distance/pitch, lag and shake tunables. HOME for the
// BehaviourGameplayExternal::Parameters slice this TU bodies:
//   - BehaviourGameplayExternal::Parameters::Set @0x821F9228 (seed a param block from an
//     attribute-system source block, applying the default external-cam tunables)
// The full behaviour (Construct/Prepare/Update and the rest of the rig) and its Behaviour base
// land with their own TUs; this header models only the slice Parameters::Set needs, BY NAME.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block. Parameters::Set stamps the external-cam tag
//   into that word; the console value for eBehaviourGameplayExternal is 0 (the asm at
//   0x821F9270 stores r9, which was loaded li r9,0, into +0x00). Replace with the real
//   EBehaviourType enum when the Behaviour base TU lands; the enumerator VALUE (0) is pinned
//   from the asm (and is consistent with the sibling tags eBehaviourGameplayBumper == 1,
//   eBehaviourRig == 2, eBehaviourHeliCam == 6, eBehaviourPassengerCam == 7).
enum EBehaviourTypeGameplayExternal
{
    eBehaviourGameplayExternal = 0
};

class BehaviourGameplayExternal
{
public:

    // ------------------------------------------------------------------------
    // The external-cam parameter block. Layout pinned store-for-store from the asm of
    // Parameters::Set @0x821F9228 (offsets are the stfs/stw/stb displacements off r31). The
    // block spans +0x00 .. +0xAC; every 4-byte slot is written. The X360 build seeds the
    // per-vehicle tunables from a source object reached through lpSource->mpfValues (i.e.
    // *(a2+4), the attribute-system parameter source array), and stamps the remaining slots
    // with fixed default tunables. The two FOV tunables are asserted positive:
    //   +0x6C mrFOV       (asserted > 0) <- source[+0x30]
    //   +0x78 mfBoostFOV  (asserted > 0) <- source[+0x40]
    // Field names below are the observable roles (the two asserted FOVs) plus slot-named homes
    // for the remainder so every store has a named destination; re-name precisely when the
    // external-cam attribute schema TU lands. The OFFSETS are authoritative.
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (TextFile{Read,Write}Serialiser); the per-instance body is a separate TU.
        // Declared so the serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeGameplayExternal GetType() const
        {
            return static_cast<EBehaviourTypeGameplayExternal>(meType);
        }

        // The source block this seeds from: a small object whose +0x04 slot (mpfValues) holds
        // a pointer to the f32 source array Parameters::Set copies out of (the asm re-loads
        // lwz r11,4(r4) before each lfs ...(r11)).
        struct Source
        {
            const f32* mpfValues;   // +0x04 of the source object: the f32 source array
        };

        // Seed this block from an attribute-system source object, applying the default
        // external-cam tunables, then assert the two FOV tunables are positive. @0x821F9228.
        void Set(const Source* lpSource);

        s32         meType;        // +0x00  type tag (= eBehaviourGameplayExternal, 0)
        const char* mpcName;       // +0x04  debug name ("Gameplay")
        s32         miField08;     // +0x08  default integer slot (= 3)
        f32         mfField0C;     // +0x0C  default tunable (= 0.06f)
        f32         mfField10;     // +0x10  default tunable (= 0.0f)
        f32         mfField14;     // +0x14  default tunable (= 1.15f)
        f32         mfField18;     // +0x18  default tunable (= 0.11f)
        f32         mfField1C;     // +0x1C  default tunable (= 0.0f)
        f32         mfField20;     // +0x20  default tunable (= 0.0f)
        f32         mfField24;     // +0x24  default tunable (= 3.0f)
        f32         mfField28;     // +0x28  default tunable (= 1.0f)
        f32         mfField2C;     // +0x2C  default tunable (= 8.0f)
        f32         mfField30;     // +0x30  default tunable (= 8.0f)
        f32         mfField34;     // +0x34  default tunable (= 0.75f)
        f32         mfField38;     // +0x38  default tunable (= 0.0f)
        f32         mfField3C;     // +0x3C  <- source[+0x2C]
        f32         mfField40;     // +0x40  <- source[+0x08]
        f32         mfField44;     // +0x44  default tunable (= -0.5f)
        f32         mfField48;     // +0x48  default tunable (= 0.015f)
        f32         mfField4C;     // +0x4C  <- source[+0x28]
        f32         mfField50;     // +0x50  <- source[+0x24]
        f32         mfField54;     // +0x54  <- source[+0x20]
        f32         mfField58;     // +0x58  <- source[+0x1C]
        f32         mfField5C;     // +0x5C  <- source[+0x18]
        f32         mfField60;     // +0x60  default tunable (= 17.0f)
        f32         mfField64;     // +0x64  default tunable (= 0.25f)
        f32         mfField68;     // +0x68  <- source[+0x14]
        f32         mrFOV;         // +0x6C  <- source[+0x30]  (asserted > 0)
        f32         mfField70;     // +0x70  default tunable (= 60.0f)
        f32         mfField74;     // +0x74  default tunable (= 0.0f)
        f32         mfBoostFOV;    // +0x78  <- source[+0x40]  (asserted > 0)
        f32         mfField7C;     // +0x7C  default tunable (= 0.01f)
        f32         mfField80;     // +0x80  default tunable (= 0.1f)
        f32         mfField84;     // +0x84  default tunable (= 0.7f)
        f32         mfField88;     // +0x88  <- source[+0x00]
        f32         mfField8C;     // +0x8C  <- source[+0x34]
        f32         mfField90;     // +0x90  <- source[+0x3C]
        f32         mfField94;     // +0x94  <- source[+0x38]
        f32         mfField98;     // +0x98  default tunable (= 0.0f)
        f32         mfField9C;     // +0x9C  <- source[+0x04]
        f32         mfFieldA0;     // +0xA0  <- source[+0x0C], then OVERRIDDEN to -1.0f
        f32         mfFieldA4;     // +0xA4  <- source[+0x10]
        f32         mfFieldA8;     // +0xA8  default tunable (= 0.5f)
        u8          mbFieldAC;     // +0xAC  default flag (= 1)
    };

    // Adopt an external-cam parameter block: assert it carries the external-cam type tag,
    // then cache its name word and store the pointer. INLINED on the X360 (no standalone
    // ledger symbol -- the SharedCameraContainer::Prepare @0x82263D50 asm carries the whole
    // body: cmplwi bank-block+0x00 vs 0 + the "lpParameters->GetType() ==
    // eBehaviourGameplayExternal" assert citing BehaviourGameplayExternal.cpp:138, then
    // stw params, 0xB00(this) / stw params->mpcName, 0x10(this)); extracted back out here
    // per the inlining-reversal rule, mirroring the bumper's committed SetParameters
    // @0x821F39C0.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the external-cam rig lands with the full behaviour TU. The vtable/base
    //   head occupies +0x00; the cached param word is at +0x10 (stw r11, 0x10(this)) and the
    //   param pointer is at +0xB00 (stw r29, 0xB00(this)). Reserved byte spans place them.
    void*             mpVTable;                        // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];       // +0x04 .. +0x0F (rig members not modelled here)
    const char*       mpcCachedName;                   // +0x10  cached lpParameters->mpcName
    u8                maReserved14[0xB00 - 0x14];      // +0x14 .. +0xAFF (rig members not modelled here)
    const Parameters* mpParameters;                    // +0xB00  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayExternal::SetParameters (X360-inlined; asm from
// SharedCameraContainer::Prepare @0x82263D50):
//   lwz  r11, 0(r29)        ; lpParameters->meType
//   cmplwi r11, 0           ; == eBehaviourGameplayExternal
//   ... assert on mismatch ("lpParameters->GetType() == eBehaviourGameplayExternal",
//                           BehaviourGameplayExternal.cpp:138) ...
//   lwz  r11, 4(r29)        ; lpParameters->mpcName (its +0x04 word)
//   stw  r29, 0xB00(r28)    ; mpParameters  = lpParameters
//   stw  r11, 0x10(r28)     ; mpcCachedName = lpParameters->mpcName
// ----------------------------------------------------------------------------
inline void
BehaviourGameplayExternal::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourGameplayExternal,
               "lpParameters->GetType() == eBehaviourGameplayExternal");
    mpParameters  = lpParameters;              // stw r29, 0xB00(this)
    mpcCachedName = lpParameters->mpcName;     // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_EXTERNAL_H
