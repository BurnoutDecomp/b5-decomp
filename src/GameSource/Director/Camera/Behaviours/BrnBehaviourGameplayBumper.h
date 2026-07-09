#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_BUMPER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_BUMPER_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (type assert + Parameters::Set sanity)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h
//
// BrnDirector::Camera::BehaviourGameplayBumper -- the in-car "bumper cam" gameplay behaviour:
// the low, fast camera mounted at the front of the player car, with its own FOV / boost-FOV
// and shake tunables. HOME for the two BehaviourGameplayBumper class slices this header's
// TUs body:
//   - BehaviourGameplayBumper::SetParameters @0x821F39C0   (adopt a bumper-cam param block)
//   - BehaviourGameplayBumper::Parameters::Set @0x821F94C8 (seed a param block from an
//     attribute-system source block, with default tunables)
// The full behaviour (Construct/Prepare/Update and the rest of the rig) lands with its own
// TU; this header models only the slices these two functions need, BY NAME.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The console value for
//   eBehaviourGameplayBumper is 1 (the asm at 0x821F39DC compares the param block's first
//   word against 1; Parameters::Set stores 1 into that word). Replace with the real
//   EBehaviourType enum when the Behaviour base TU lands; the enumerator VALUE (1) is pinned.
enum EBehaviourTypeGameplayBumper
{
    eBehaviourGameplayBumper = 1
};

class BehaviourGameplayBumper
{
public:

    // ------------------------------------------------------------------------
    // The bumper-cam parameter block. Layout pinned store-for-store from the asm of
    // Parameters::Set @0x821F94C8 (offsets are the stfs/stw/stb displacements off r31):
    //   +0x00 meType (s32, =eBehaviourGameplayBumper)   +0x04 mpcName (const char*)
    //   +0x08..+0x30 the f32 tunables copied from the attribute-system source block
    //   +0x24 mfFOV (asserted > 0)   +0x30 mfBoostFOV (asserted > 0)
    //   +0x34 mbFlag (u8, =1)        +0x38..+0x44 the default shake/blend tunables
    // The X360 build seeds the source-copied tunables from a block reached via
    // *(a2+4) (the attribute-system parameter source); the field names below are the
    // observable roles (FOV / boost-FOV asserted) with the remainder named by their slot
    // so every store has a named home. Re-name precisely when the bumper-cam attribute
    // schema TU lands; the OFFSETS are authoritative.
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (TextFile{Read,Write}Serialiser); the per-instance body is a separate TU.
        // Declared so the serialiser's Serialise<Parameters> can drive it by name.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeGameplayBumper GetType() const
        {
            return static_cast<EBehaviourTypeGameplayBumper>(meType);
        }

        // The source block this seeds from: a small object whose +0x04 holds a pointer to
        // the f32 source array Parameters::Set copies out of (lwz r11,4(r4); lfs ...(r11)).
        struct Source
        {
            const f32* mpfValues;   // +0x04 of the source object: the f32 source array
        };

        // Seed this block from an attribute-system source object, applying the default
        // bumper-cam tunables. @0x821F94C8.
        void Set(const Source* lpSource);

        s32         meType;       // +0x00  type tag (= eBehaviourGameplayBumper)
        const char* mpcName;      // +0x04  debug name ("Bumper Cam")
        f32         mfField08;    // +0x08  <- source[0x04]
        f32         mfField0C;    // +0x0C  <- source[0x00]
        f32         mfField10;    // +0x10  <- source[0x28]
        f32         mfField14;    // +0x14  <- source[0x24]
        f32         mfField18;    // +0x18  <- source[0x10]
        f32         mfField1C;    // +0x1C  <- source[0x08]
        f32         mfField20;    // +0x20  <- source[0x0C]
        f32         mfFOV;        // +0x24  <- source[0x14]  (asserted > 0)
        f32         mfField28;    // +0x28  <- source[0x1C]
        f32         mfField2C;    // +0x2C  <- source[0x20]
        f32         mfBoostFOV;   // +0x30  <- source[0x18]  (asserted > 0)
        u8          mbField34;    // +0x34  default flag (= 1)
        u8          maReserved35[0x38 - 0x35]; // +0x35..+0x37 padding to the next f32 slot
        f32         mfField38;    // +0x38  default tunable (= 0.0f)
        f32         mfField3C;    // +0x3C  default tunable (= 0.0f)
        f32         mfField40;    // +0x40  default tunable (= 3.0f)
        f32         mfField44;    // +0x44  default tunable (= 1.0f)
    };

    // Adopt a bumper-cam parameter block: assert it carries the bumper-cam type tag, then
    // cache its first word and store the pointer. @0x821F39C0.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the bumper-cam rig lands with the full behaviour TU. The vtable/base head
    //   occupies +0x00; the cached param word is at +0x10 (stw r11, 0x10(this)) and the param
    //   pointer is at +0x828 (stw r31, 0x828(this)). Reserved byte spans place them exactly.
    void*             mpVTable;                        // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];       // +0x04 .. +0x0F (rig members not modelled here)
    const char*       mpcCachedName;                   // +0x10  cached lpParameters->mpcName
    u8                maReserved14[0x828 - 0x14];      // +0x14 .. +0x827 (rig members not modelled here)
    const Parameters* mpParameters;                    // +0x828  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayBumper::SetParameters @0x821F39C0
//   lwz  r11, 0(r4)         ; lpParameters->meType
//   cmplwi r11, 1           ; == eBehaviourGameplayBumper
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->miParamWord1 (its +0x04 word)
//   stw  r4,  0x828(r3)     ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; mParamWord1  = lpParameters's +0x04 word
// ----------------------------------------------------------------------------
inline void
BehaviourGameplayBumper::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourGameplayBumper,
               "lpParameters->GetType() == eBehaviourGameplayBumper");
    mpParameters  = lpParameters;             // stw r4,  0x828(this)
    // The asm caches the param block's +0x04 word into the rig's +0x10 word (lwz r11,4(r4);
    // stw r11, 0x10(this)). The block's +0x04 slot is its mpcName pointer, so the cached word
    // is that name pointer -- read BY NAME here.
    mpcCachedName = lpParameters->mpcName;     // lwz r11,4(lp); stw r11, 0x10(this)
}

// Layout note: the offsets in the field comments are the X360 store displacements (4-byte
//   console pointers). This PC reconstruction is a 64-bit build, so the single pointer member
//   (mpcName) is 8 bytes here and the absolute host offsets of the trailing f32 fields differ
//   from the console -- that is expected and matches the project's host-native-pointer-width
//   convention. The store ROLES (which source value lands in mfFOV / mfBoostFOV, which default
//   in each field) are preserved by name; the FOV/boost-FOV asserts below are pointer-width
//   independent and lock the two semantically load-bearing fields.

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_BUMPER_H
