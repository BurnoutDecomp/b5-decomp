#ifndef BRN_SOUND_VEHICLES_ENGINES_BOOST_EFFECT_H
#define BRN_SOUND_VEHICLES_ENGINES_BOOST_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Engines::BoostEffect
//   GameSource/Sound/Vehicles/Engines/BrnBoostEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. BoostEffect is the boost/turbo engine
// sound EFFECT OBJECT. Its X360 deleting destructor installs/tears down the same
// BrnEffectObject dual-vptr pair as the committed siblings (LoopModelEffect /
// CollisionEffect), so it reuses the committed BrnEffectObject base BY NAME.
//
// FLAG (MINIMAL home): destructor-only slice. DWARF's true parent IStreamUser and the
// full member/method surface (mBoostVoice, mParams, the mfParam_AEMS_* floats, timing
// DataPoints, mpPhysicsControl/mpSpeedStreamControl, RTTI hooks) are DEFERRED. Only
// the base (BY NAME) is materialised.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

struct BoostEffect : public BrnSound::Logic::BrnEffectObject
{
    BoostEffect() {}
    virtual ~BoostEffect();     // anchor for the vector deleting destructor @ 0x826E43A0
};

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_ENGINES_BOOST_EFFECT_H
