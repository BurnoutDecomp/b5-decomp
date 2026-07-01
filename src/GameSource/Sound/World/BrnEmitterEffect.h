#ifndef BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_EFFECT_H
#define BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"     // CgsSound::Logic::VoiceWrapper member (BY NAME)

// =============================================================================
// BrnSound::Logic::World::EmitterEffect
//   GameSource/Sound/World/BrnEmitterEffect.h (DWARF home) +
//   GameSource/Sound/World/BrnEmitterEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// EmitterEffect is the 3D-positional world-emitter sound-logic EFFECT OBJECT (a leaf
// like the committed sibling ExplosionEffect). DWARF (BrnEmitterEffect.h:74):
//   struct EmitterEffect : public BrnSound::Logic::BrnEffectObject
// Its X360 ctor (@ 0x826D1C00) installs the SAME dual-vptr pair as the committed
// BrnEffectObject sibling (primary vptr @ this+0, IResourceRequester sub-object vptr
// @ this+4), so EmitterEffect reuses the COMMITTED BrnEffectObject dual base BY NAME
// and embeds a CgsSound::Logic::VoiceWrapper at this+0x38.
//
// This home materialises the two ledger functions that fit the minimal-leaf shape:
//   EmitterEffect()                @ 0x826D1C00  (the leaf constructor)
//   `scalar deleting destructor'   @ 0x826E6B40  (compiler-synthesised; forwards to
//        the ~EmitterEffect anchor -- no separate hand-written body)
// GetSoundEntity() @ 0x82686708 is a SEPARATE ledger function that needs the owned
// EmitterState surface (its +0x0C mpState + EmitterState::IsAttached/GetSoundEntity +
// StaticSoundEntity by value) which is not homed in this slice; it is DEFERRED to its
// own recon slice and is NOT modelled here (blocking it avoids growing the committed
// BrnSound::Logic::World::EmitterState layout / forcing a StaticSoundEntity cascade).
//
// FLAG (un-homed leaf members): the X360 ctor additionally zero-inits leaf scalars at
// +0x08..+0x34 (two f32, two s16, word/byte fields). Their names/types are un-homed
// (no Feb-2007 source for this TU) -- DECLARATION-ONLY, deferred to the layout recon
// slice, NOT fabricated as named fields here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// DWARF (BrnEmitterEffect.h:74): EmitterEffect : public BrnEffectObject. Reuses the
// committed BrnEffectObject dual base BY NAME and embeds a CgsSound::Logic::VoiceWrapper.
// The two leaf vptrs the X360 ctor installs (off_820B4010 @ +0, off_820B3FDC @ +4)
// are produced structurally by the dual-base + virtual-destructor declaration; the
// embedded VoiceWrapper is the ctor's tail sub-object construction.
struct EmitterEffect : public BrnEffectObject
{
    EmitterEffect();               // @ 0x826D1C00
    virtual ~EmitterEffect();      // out-of-line anchor (empty); scalar deleting
                                   // destructor @ 0x826E6B40 forwards to it

    // +0x38 (X360). Embedded per-effect voice wrapper the ctor constructs by the tail
    // `bl CgsSound::Logic::VoiceWrapper::VoiceWrapper(this+0x38)`. Reused BY NAME from
    // the minimal CgsVoiceWrapper home.
    CgsSound::Logic::VoiceWrapper mVoiceWrapper;

    // FLAG: leaf scalar members at un-homed offsets +0x08..+0x34 (two f32, two s16,
    // word/byte fields) zero-inited by the ctor -- DECLARATION-ONLY, deferred, NOT
    // fabricated. The DWARF also attests an owned EmitterState* (@ +0x0C, read by the
    // DEFERRED GetSoundEntity accessor), mPos Vector3, mp3dControl Emitter3dControl*,
    // mi16PitchOutput -- all DEFERRED to their own recon slices (not in this slice's
    // function set), NOT fabricated here.
};

} // namespace World
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_WORLD_BRN_EMITTER_EFFECT_H
