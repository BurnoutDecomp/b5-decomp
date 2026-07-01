#ifndef BRN_SOUND_VEHICLES_WHEELS_SKID_EFFECT_H
#define BRN_SOUND_VEHICLES_WHEELS_SKID_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Vehicles::Wheels::SkidEffect
//   GameSource/Sound/Vehicles/Wheels/BrnSkidEffect.{h,cpp}  (DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// SkidEffect is the player-car skid sound EFFECT OBJECT. DWARF (BrnSkidEffect.h:41):
// SkidEffect : public BrnEffectObject -- it reuses the committed BrnEffectObject dual
// base BY NAME (primary vptr @ this+0, IResourceRequester sub-object @ this+4) and
// embeds a CgsSound::Logic::VoiceWrapper (mSkidsVoice @ +0x54).
//
// FLAG (MINIMAL home): this slice bodies only the leaf constructor (@ 0x826C8C78).
// The DWARF NAMES SkidEffect's remaining leaf members (mDataHandle: ResourceHandle;
// mpWheelControl: WheelControl*; mpPhysicsControl: PhysicsControl*; mfOverallMax /
// mfSkidAzimuth: f32; mbSkidsLatched: DataPoint<bool>; mSkidFunctorPointer:
// VoiceWrapper::FunctorPointer<SkidEffect> @ +0xA8), but their TYPES are UN-HOMED, so
// they are DECLARATION-ONLY / DEFERRED -- NOT emitted as real members. Only the base
// (BY NAME) + the embedded VoiceWrapper (mSkidsVoice) are materialised, matching the
// committed ExplosionEffect / TrafficSkid / TrafficHorn minimal-home convention.
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

// BrnSkidEffect.h:41 (DWARF). Reuses the committed BrnEffectObject dual base BY NAME
// and embeds mSkidsVoice (the ctor's tail `bl VoiceWrapper::VoiceWrapper(this+0x54)`).
struct SkidEffect : public BrnSound::Logic::BrnEffectObject
{
    SkidEffect();               // @ 0x826C8C78
    virtual ~SkidEffect();      // DWARF BrnSkidEffect.cpp:60 (anchor; empty leaf)

    // +0x54 (X360). Embedded per-skid voice wrapper the ctor constructs. Reused BY
    // NAME from the minimal VoiceWrapper home.
    CgsSound::Logic::VoiceWrapper mSkidsVoice;
};

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

#endif // BRN_SOUND_VEHICLES_WHEELS_SKID_EFFECT_H
