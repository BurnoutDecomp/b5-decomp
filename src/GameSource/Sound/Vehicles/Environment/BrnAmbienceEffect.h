#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceEffect
//   GameSource/Sound/Vehicles/Environment/BrnAmbienceEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: AmbienceEffect : public
// BrnEffectObject. The per-car environmental ambience EFFECT OBJECT.
//
// FLAG (MINIMAL home): ctor + CreateObj + dtor slice. Beyond the BrnEffectObject dual
// base the ctor ALSO installs a THIRD polymorphic sub-object base @ +0x38 (un-recovered
// class -- no DWARF, no Feb-2007 source) and zero-inits un-homed leaf scalars; those are
// DECLARATION-DEFERRED (not fabricated). Only the base (BY NAME) is materialised, so this
// leaf is layout-incomplete vs the X360 (same precedent as the committed
// PlayerVehicleStateManager +0x98 FLAG).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct AmbienceEffect : public BrnSound::Logic::BrnEffectObject
{
    AmbienceEffect();               // @ 0x826B9600
    virtual ~AmbienceEffect();      // leaf vtable emission point (off_820B1F80)

    // @ 0x826D0CC0 -- effect-object factory hook. Returns the +4 IResourceRequester view.
    static BrnSound::Logic::IResourceRequester* CreateObj( u32 luFlavour );
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_EFFECT_H
