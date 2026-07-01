#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectControl.h"   // committed BrnEffectControl dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceControl
//   GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: AmbienceControl : public
// BrnSound::Logic::BrnEffectControl. The per-car environmental ambience CONTROL.
//
// FLAG (MINIMAL home): CreateObject + deleting-destructor slice. The full member set
// (mRegion RegionDataPoint, mMap2d WorldMap2D, mMap2dResource ResourceHandle,
// mpEnclosureControl, mfTimeSinceUpdate) is DEFERRED. Only the base (BY NAME) is
// materialised; every stored member the dtor touches is owned by the BrnEffectControl
// base.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct AmbienceControl : public BrnSound::Logic::BrnEffectControl
{
    AmbienceControl() {}
    virtual ~AmbienceControl();     // anchor for the vector deleting destructor @ 0x826B9558

    // @ 0x826D0B70 -- RTTI factory hook.
    static CgsSound::Logic::EffectControl* CreateObject( u32 luType );
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_AMBIENCE_CONTROL_H
