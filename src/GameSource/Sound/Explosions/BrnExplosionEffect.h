#ifndef BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_EFFECT_H
#define BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"     // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Logic::Explosion::ExplosionEffect
//   GameSource/Sound/Explosions/BrnExplosionEffect.h (DWARF home inferred) +
//   GameSource/Sound/Explosions/BrnExplosionEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// ExplosionEffect is the sound-logic EFFECT OBJECT for explosions -- the leaf that
// the explosion CreateObject factory placement-constructs. Its X360 constructor
// (@ 0x826D5480) installs the SAME dual-vptr pair as the committed BrnEffectObject
// sibling (a primary vptr at this+0 and the IResourceRequester sub-object vptr at
// this+4), so ExplosionEffect reuses the COMMITTED BrnEffectObject dual base BY NAME
// (CgsSound::Logic::EffectObject primary @ this+0, BrnSound::Logic::IResourceRequester
// sub-object @ this+4) from
// GameSource/Sound/Module/LogicModule/BrnEffectObject.h, and embeds a
// CgsSound::Logic::VoiceWrapper sub-object (constructed at this+0x38 by the ctor's
// tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper`).
//
// This TU's recon'd function set is exactly TWO entries:
//   ExplosionEffect()                       @ 0x826D5480  (the leaf constructor)
//   `scalar deleting destructor'            @ 0x826E8F78
// The leaf NON-virtual destructor ~ExplosionEffect (which the scalar deleting
// destructor forwards to) is a SEPARATE un-homed recon slice and is DEFERRED;
// it is declared here only so the scalar-deleting-destructor thunk shape exists.
//
// FLAG (un-homed leaf members): the X360 constructor inlines the base member
// zero-init AND a set of LEAF scalar zero-inits (two f32 set to 0.0f, two s16 set to
// 0, and several word/byte fields set to 0, at flat offsets +0x08..+0x34) before
// constructing the embedded VoiceWrapper at +0x38. There is NO DWARF for
// ExplosionEffect's own layout and NO Feb-2007 source, so the NAMES/TYPES of those
// leaf members are genuinely un-homed. Per the project anti-fabrication rule they
// are NOT invented as named fields and NOT raw-offset-hacked: the leaf is modelled
// as the committed BrnEffectObject base (which zero-inits the base members through
// its own default ctor) plus the embedded VoiceWrapper member, and the additional
// leaf scalar inits are DECLARATION-ONLY (deferred to the layout/DWARF recon slice).
// The observable, attestable construction effects -- base sub-object construction
// (the two installed vptrs) and the VoiceWrapper sub-object construction -- are
// reproduced faithfully; no fabricated member or value is added.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): the X360 ctor/dtor access members by
// absolute byte offset (the VoiceWrapper at +0x38, the base members at their
// committed offsets). Those offsets assume 4-byte pointers/vptr; on the 64-bit host
// pointer/vptr widths differ, so members are pinned BY NAME and SEQUENCE only and
// absolute offsets are NOT static_asserted.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Explosion
{

// DWARF home inferred. Reuses the committed BrnEffectObject dual base BY NAME and
// embeds a CgsSound::Logic::VoiceWrapper. The two leaf vptrs the X360 ctor installs
// (off_820B4618 @ +0, off_820B45E4 @ +4) are produced structurally by the dual-base
// + virtual-destructor declaration; the embedded VoiceWrapper is the ctor's tail
// sub-object construction.
struct ExplosionEffect : public BrnEffectObject
{
    ExplosionEffect();
    virtual ~ExplosionEffect();

    // +0x38 (X360). The embedded per-effect voice wrapper the ctor constructs last
    // (its tail `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`). Reused
    // BY NAME from the minimal CgsVoiceWrapper home.
    CgsSound::Logic::VoiceWrapper mVoiceWrapper;

    // FLAG: the X360 ctor additionally zero-inits a set of LEAF scalar members
    // (offsets +0x08..+0x34: two f32, two s16, several word/byte fields). Their
    // names/types are un-homed (no DWARF, no Feb-2007 source) -- DECLARATION-ONLY,
    // deferred to the layout recon slice. NOT fabricated as named fields here.
};

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_EXPLOSION_BRN_EXPLOSION_EFFECT_H
