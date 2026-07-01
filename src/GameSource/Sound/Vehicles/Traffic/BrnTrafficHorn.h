#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_HORN_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_HORN_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Logic::Traffic::TrafficHorn
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.h (DWARF home) +
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficHorn.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// TrafficHorn is the traffic-horn sound-logic EFFECT OBJECT (sibling of the
// committed ExplosionEffect). Its X360 ctor (@ 0x826CAEC0) installs the SAME
// dual-vptr pair as the committed BrnEffectObject sibling (primary vptr @ this+0,
// IResourceRequester sub-object vptr @ this+4), so TrafficHorn reuses the COMMITTED
// BrnEffectObject dual base BY NAME and embeds a CgsSound::Logic::VoiceWrapper at
// this+0x38.
//
// Matches the committed ExplosionEffect.h / BrnTrafficControl.h MINIMAL-home
// convention: only the base (BY NAME) + the ctor-touched embedded member (mHornVoice
// @ +0x38) are materialised. The rest of the DWARF surface uses UN-HOMED types
// (VoiceWrapper::FunctorPointer<T>, ETrafficSize, TrafficControl) and is
// DECLARATION-ONLY / DEFERRED (see FLAG); emitting them as real members would not
// compile (those types are not homed anywhere in src).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// BrnTrafficHorn.h:48 (DWARF): struct TrafficHorn : public BrnEffectObject.
// Reuses the committed BrnEffectObject dual base BY NAME (CgsSound::Logic::
// EffectObject primary @ this+0, IResourceRequester sub-object @ this+4 -- the two
// leaf vptrs the X360 ctor installs, off_820B3AD0 @ +0 / off_820B3A9C @ +4) and
// embeds a CgsSound::Logic::VoiceWrapper at +0x38.
struct TrafficHorn : public BrnEffectObject
{
    TrafficHorn();
    virtual ~TrafficHorn();

    // +0x38 (X360). Embedded per-horn voice wrapper the ctor constructs last (its
    // tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`; DWARF
    // BrnTrafficHorn.h:133). Reused BY NAME from the minimal VoiceWrapper home.
    CgsSound::Logic::VoiceWrapper mHornVoice;

    // FLAG: the X360 ctor additionally zero-inits a set of LEAF scalar members
    // (offsets +0x08..+0x34: two f32, two s16, several word/byte) and installs a
    // vptr/table ptr @ +0x88. DWARF (BrnTrafficHorn.h) attests the member SET
    // (mHornFunctionPointer: VoiceWrapper::FunctorPointer<TrafficHorn>;
    // mpTrafficControl: TrafficControl*; meTrafficSize: ETrafficSize;
    // mfAemsPatchMode: f32; mbPrevHornState: bool). Those types are UN-HOMED, so the
    // members are DECLARATION-ONLY here and DEFERRED to the full TrafficHorn layout/
    // RTTI recon slice. NOT fabricated / NOT emitted as real members (matching the
    // committed ExplosionEffect.h + BrnTrafficControl.h minimal-home convention).
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_HORN_H
