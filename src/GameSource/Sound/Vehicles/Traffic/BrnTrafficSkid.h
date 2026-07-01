#ifndef BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_SKID_H
#define BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_SKID_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Logic::Traffic::TrafficSkid
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficSkid.h (DWARF home inferred) +
//   GameSource/Sound/Vehicles/Traffic/BrnTrafficSkid.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// TrafficSkid is the traffic-skid sound-logic EFFECT OBJECT (sibling of the committed
// ExplosionEffect). Its X360 ctor (@ 0x826CAFE0) and scalar deleting destructor
// (@ 0x826E31F0) install the SAME dual-vptr pair as the committed BrnEffectObject
// sibling (primary vptr @ this+0, IResourceRequester sub-object vptr @ this+4), so
// TrafficSkid reuses the COMMITTED BrnEffectObject dual base BY NAME and embeds a
// CgsSound::Logic::VoiceWrapper at this+0x38 (ctor tail `bl VoiceWrapper::VoiceWrapper
// (this+0x38)`; dtor leading `bl VoiceWrapper::~VoiceWrapper(this+0x38)`).
//
// This TU's recon'd function set is exactly two entries:
//   TrafficSkid()                   @ 0x826CAFE0  (the leaf constructor)
//   `scalar deleting destructor'    @ 0x826E31F0  (compiler-synthesised; forwards to
//        the ~TrafficSkid anchor -- no separate hand-written body)
//
// FLAG (un-homed leaf members): the ctor additionally zero-inits leaf scalars at
// +0x08..+0x34 (two f32 @ +0x1C/+0x20, two s16 @ +0x10/+0x12, several word/byte
// fields) and installs off_820AC1B0 @ +0x88 (likely an RTTI/ClassTypeInfo-style
// pointer). Names/types un-homed (no DWARF, no Feb-2007 source) -- DECLARATION-ONLY,
// deferred to the layout recon slice, NOT fabricated here.
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

// DWARF home inferred. Reuses the committed BrnEffectObject dual base BY NAME and
// embeds a CgsSound::Logic::VoiceWrapper. The two leaf vptrs are produced structurally
// by the dual-base + virtual-destructor declaration; the embedded VoiceWrapper is the
// ctor's tail / dtor's leading sub-object (con/de)struction.
struct TrafficSkid : public BrnEffectObject
{
    TrafficSkid();                 // @ 0x826CAFE0
    virtual ~TrafficSkid();        // out-of-line anchor (empty); scalar deleting
                                   // destructor @ 0x826E31F0 forwards to it

    // +0x38 (X360). Embedded per-effect voice wrapper: ctor tail `bl VoiceWrapper::
    // VoiceWrapper(this+0x38)`; dtor leading `bl VoiceWrapper::~VoiceWrapper(this+0x38)`.
    // Reused BY NAME from the minimal CgsVoiceWrapper home.
    CgsSound::Logic::VoiceWrapper mVoiceWrapper;

    // FLAG: the ctor additionally zero-inits leaf scalars at +0x08..+0x34 and installs
    // off_820AC1B0 @ +0x88. Names/types un-homed (no DWARF, no Feb-2007 source) --
    // DECLARATION-ONLY, deferred to the layout recon slice. NOT fabricated here.
};

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_TRAFFIC_BRN_TRAFFIC_SKID_H
