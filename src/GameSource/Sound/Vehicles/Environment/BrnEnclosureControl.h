#ifndef BRN_SOUND_VEHICLES_ENVIRONMENT_ENCLOSURE_CONTROL_H
#define BRN_SOUND_VEHICLES_ENVIRONMENT_ENCLOSURE_CONTROL_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Environment::EnclosureControl
//   GameSource/Sound/Vehicles/Environment/BrnEnclosureControl.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF: EnclosureControl : public
// BrnEffectObject. Maps a trigger region type to an enclosure-index and owns the
// enclosure (tunnel/underpass) reverb transition.
//
// FLAG (MINIMAL home): ConvertRegionTypeToIndex (pure mapping, `this` unused) +
// Create + deleting-destructor slice. The full member set is DEFERRED; only the base
// (BY NAME) is materialised.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

struct EnclosureControl : public BrnSound::Logic::BrnEffectObject
{
    EnclosureControl() {}
    virtual ~EnclosureControl();    // anchor for the vector deleting destructor @ 0x826B94A8

    // @ 0x82685FA0 -- map a region type (19..31) to an enclosure index (pure; `this` unused).
    int ConvertRegionTypeToIndex( int liRegionType ) const;
    // @ 0x826D0A30 -- allocate + construct factory. Returns the EffectObject* base view.
    static CgsSound::Logic::EffectObject* Create( bool lbFlavour );
};

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENVIRONMENT_ENCLOSURE_CONTROL_H
