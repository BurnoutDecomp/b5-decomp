#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_REVERB_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_REVERB_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"            // CgsSound::Utils::InterpolateLine / DataPoint (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::ReverbEffect
//   GameSource/Sound/Vehicles/Environment/BrnReverbEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF (BrnReverbEffect.h): ReverbEffect :
// public BrnEffectObject. The environmental reverb EFFECT OBJECT; interpolates a reverb
// preset over time.
//
// FLAG (DWARF-exact ctor): only the two embedded members mInterpolateReverb
// (InterpolateLine) + mReverbType (DataPoint<eReverbTypes>) are default-CONSTRUCTED by
// the X360 ctor; the four scalar floats + meReverbState + the two control back-pointers
// are LEFT UNINITIALIZED (populated on Attach). mReverbType's enum is the un-homed
// AttribSys::Enums::eReverbTypes; modelled as DataPoint<s32> (matches the two-word
// zero-init). Absolute offsets are NOT static_asserted across the 32/64 boundary.
// =============================================================================

// Pointer-only control back-references -> forward declarations.
namespace BrnSound { namespace Vehicles { namespace Environment { struct EnclosureControl; } } }
namespace BrnSound { namespace Vehicles { namespace Engines { struct PhysicsControl; } } }

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct ReverbEffect : public BrnSound::Logic::BrnEffectObject
{
    // DWARF BrnReverbEffect.h:10.
    enum eReverbState
    {
        E_REVERB_STATE_NONE          = 0,
        E_REVERB_STATE_INTERPOLATING = 1,
    };

    ReverbEffect();                 // @ 0x826BA458
    virtual ~ReverbEffect();        // anchor for the vector deleting destructor @ 0x826BA4E8

    // @ 0x826D1438 -- RTTI factory hook.
    static CgsSound::Logic::EffectObject* CreateObject( u32 luType );

    // ---- members in DWARF order (offsets are X360 facts, not asserted on host) ----
    f32          mfTime;         // +0x38 (X360 ctor leaves UNINITIALIZED)
    f32          mfSpaceSize;    // +0x3C (UNINITIALIZED)
    f32          mfBrightness;   // +0x40 (UNINITIALIZED)
    f32          mfGain;         // +0x44 (UNINITIALIZED)
    CgsSound::Utils::InterpolateLine mInterpolateReverb; // +0x48 (default-constructed)
    eReverbState meReverbState;  // +0x64 (UNINITIALIZED)
    CgsSound::Utils::DataPoint<s32>  mReverbType;         // +0x68 (default-constructed; eReverbTypes==s32)
    const EnclosureControl*          mpEnclosureControl; // +0x70 (UNINITIALIZED)
    const BrnSound::Vehicles::Engines::PhysicsControl* mpPhysicsControl; // +0x74 (UNINITIALIZED)
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_REVERB_EFFECT_H
