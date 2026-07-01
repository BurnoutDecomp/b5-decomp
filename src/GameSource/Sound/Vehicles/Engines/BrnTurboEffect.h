#ifndef BRN_SOUND_VEHICLES_ENGINES_TURBO_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_TURBO_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::TurboEffect
//   GameSource/Sound/Vehicles/Engines/BrnTurboEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Turbo spool/blowoff engine sound
// EFFECT OBJECT. Reuses the committed BrnEffectObject dual base BY NAME.
//
// FLAG (MINIMAL home): deleting-destructor-only slice. The full DWARF surface
// (mTurboVoice VoiceWrapper, mTurboState DataPoint<eTurboState>, the mfTurbo* floats,
// mu8TurboBlowoff, mpEngineControl/mpHybridControl, RTTI/Attach/UpdateParams) is
// DEFERRED. Only the base (BY NAME) is materialised.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct TurboEffect : public BrnSound::Logic::BrnEffectObject
{
    TurboEffect() {}
    virtual ~TurboEffect();     // anchor for the scalar deleting destructor @ 0x826E4618
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_TURBO_EFFECT_H
