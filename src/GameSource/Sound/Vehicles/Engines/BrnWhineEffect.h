#ifndef BRN_SOUND_VEHICLES_ENGINES_WHINE_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_WHINE_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::WhineEffect
//   GameSource/Sound/Vehicles/Engines/BrnWhineEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Engine-whine sound EFFECT OBJECT.
// Reuses the committed BrnEffectObject dual base BY NAME.
//
// FLAG (MINIMAL home): deleting-destructor-only slice. The full DWARF surface
// (mWhineVoice VoiceWrapper, mpPhysicsControl, mfWhineVolume, RTTI/Attach/UpdateParams)
// is DEFERRED. Only the base (BY NAME) is materialised.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct WhineEffect : public BrnSound::Logic::BrnEffectObject
{
    WhineEffect() {}
    virtual ~WhineEffect();     // anchor for the scalar deleting destructor @ 0x826E49A8
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_WHINE_EFFECT_H
