#ifndef BRN_SOUND_VEHICLES_WHEELS_AI_SKID_EFFECT_H
#define BRN_SOUND_VEHICLES_WHEELS_AI_SKID_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Wheels::AISkidEffect
//   GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// AISkidEffect is the AI-car skid sound EFFECT OBJECT. DWARF (BrnAISkidEffect.h:41):
// AISkidEffect : public BrnEffectObject -- it reuses the committed BrnEffectObject
// dual base BY NAME (primary vptr @ this+0, IResourceRequester sub-object @ this+4)
// and embeds a CgsSound::Logic::VoiceWrapper (mSkidsVoice).
//
// FLAG (MINIMAL home): this slice bodies only the vector deleting destructor
// (@ 0x826E5E88), which adds no teardown of its own beyond the inherited
// BrnEffectObject settle. AISkidEffect's DWARF leaf members (mpPhysicsControl,
// mDriftInterp PathLine<2>, mfDriftFactor) use UN-HOMED / not-needed-here types and
// are DECLARATION-DEFERRED (not touched by the destructor); only the base (BY NAME)
// is materialised. Matches the committed CollisionEffect / TrafficHorn minimal-home
// convention.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// BrnAISkidEffect.h:41 (DWARF). Reuses the committed BrnEffectObject dual base BY NAME.
struct AISkidEffect : public BrnSound::Logic::BrnEffectObject
{
    AISkidEffect() {}

    // @ 0x826E5E88 -- anchor for the X360 `vector deleting destructor'. Empty leaf
    // teardown (the inherited BrnEffectObject settle is compiler-synthesised).
    virtual ~AISkidEffect();
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_AI_SKID_EFFECT_H
