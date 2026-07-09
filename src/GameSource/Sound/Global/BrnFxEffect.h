#ifndef BRN_SOUND_LOGIC_BRN_FX_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_FX_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"     // CgsSound::Logic::VoiceWrapper element (BY NAME)

// =============================================================================
// BrnSound::Logic::FxEffect
//   GameSource/Sound/Global/BrnFxEffect.h (DWARF home inferred) +
//   GameSource/Sound/Global/BrnFxEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// FxEffect is a sound-logic EFFECT OBJECT (a leaf like the committed sibling
// ExplosionEffect). The X360 mangling is BrnSound::Logic::FxEffect::FxEffect --
// the class lives DIRECTLY in namespace BrnSound::Logic; there is NO `Fx`
// sub-namespace (the `Global/` path is filesystem-only). Its ctor (@ 0x826C9288)
// installs the SAME dual-vptr pair as the committed BrnEffectObject sibling
// (primary vptr @ this+0, IResourceRequester sub-object vptr @ this+4), so FxEffect
// reuses the COMMITTED BrnEffectObject dual base BY NAME and embeds an ARRAY of 4
// CgsSound::Logic::VoiceWrapper sub-objects (the ctor's tail do/while loop, stride
// 0x50, base +0x38: +0x38/+0x88/+0xD8/+0x128).
//
// This TU's recon'd function set is exactly two entries:
//   FxEffect()                                @ 0x826C9288 (leaf constructor)
//   `vector deleting destructor'`adjustor{4}' @ 0x826C9328 (compiler-synthesised
//        `this - 4` MI thunk forwarding to FxEffect::`scalar deleting destructor';
//        no hand-written body -- see .cpp).
//
// FLAG (un-homed leaf members): the X360 ctor additionally zero-inits a set of LEAF
// scalar members (two f32 @ +0x1C/+0x20, two s16 @ +0x10/+0x12, word/byte fields @
// +0x08/+0x0C/+0x30/+0x34, and the trailing +0x18C word / +0x190 byte / +0x191 byte
// past the VoiceWrapper array). Their names/types are un-homed (no DWARF, no
// Feb-2007 source) -- DECLARATION-ONLY, deferred to the layout recon slice. NOT
// fabricated as named fields here. The committed VoiceWrapper minimal home is a
// size-1 shell, so the [4] array does NOT yet reproduce the 0x50-byte per-element
// stride; that is DEFERRED with the rest of the VoiceWrapper layout. Element COUNT
// (4) and construction COUNT/ORDER are attested here.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// =============================================================================
// BrnSound::Logic::FxMessage family (DWARF home BrnFxEffect.h:32/33) -- the small
// "FX" command messages the sound logic module posts onto the command queue as
// CgsSound::Io::Message<FxMessage_*>. Distinct from the FxEffect runtime object
// below; homed here alongside it because BrnFxEffect.h is their DWARF home.
//
// The base FxMessage holds a single 4-byte FxType discriminant; each concrete
// FxMessage_* is an EMPTY subclass whose only job is to fix meType to its FxType
// (the DWARF declares each derived default ctor and adds no members), so
// sizeof(FxMessage_*) == sizeof(FxMessage) == 4. X360-attested: every
// CgsModule::VariableEventQueue<*,16>::AddEvent<CgsSound::Io::Message<FxMessage_*>>
// bakes sizeof(Message<...>) == 0x14 == 16 (MessageHeader) + 4 (payload) as its
// record-size argument.
// =============================================================================
struct FxMessage
{
    // BrnFxEffect.h:33 (DWARF). The kind of FX event this message carries.
    enum FxType
    {
        E_WINDOW_SMASH       = 0,
        E_CAMERA_CUT         = 1,
        E_STUNT_SMASH        = 2,
        E_STUNT_STUNT        = 3,
        E_STUNT_JUMP         = 4,
        E_RESET_ON_TRACK     = 5,
        E_CAMERA_PHOTO       = 6,
        E_QUIT_EVENT         = 7,
        E_CRASH_IN_WATER     = 8,
        E_ONLINE_RIVAL_SWEEP = 9,
    };

    // BrnFxEffect.h:46 (DWARF). The only data member (4 bytes).
    FxType meType;

    // DWARF declares FxMessage() and FxMessage(FxType). The default ctor body is not
    // recovered, so meType is left default-initialised (no fabricated value); the
    // FxType-taking ctor is what the derived types below use to fix the discriminant.
    FxMessage() {}
    explicit FxMessage(FxType leType) : meType(leType) {}
};

// Empty FxMessage subclasses that only fix the discriminant (DWARF BrnFxEffect.h:65..).
struct FxMessage_CameraCut  : public FxMessage { FxMessage_CameraCut()  : FxMessage(E_CAMERA_CUT)  {} };
struct FxMessage_StuntSmash : public FxMessage { FxMessage_StuntSmash() : FxMessage(E_STUNT_SMASH) {} };
struct FxMessage_StuntStunt : public FxMessage { FxMessage_StuntStunt() : FxMessage(E_STUNT_STUNT) {} };
struct FxMessage_StruntJump : public FxMessage { FxMessage_StruntJump() : FxMessage(E_STUNT_JUMP)  {} };
struct FxMessage_QuitEvent  : public FxMessage { FxMessage_QuitEvent()  : FxMessage(E_QUIT_EVENT)  {} };

// DWARF home inferred: struct BrnSound::Logic::FxEffect : public BrnEffectObject.
// Reuses the committed BrnEffectObject dual base BY NAME and embeds a 4-element
// CgsSound::Logic::VoiceWrapper array. The two leaf vptrs the X360 ctor installs
// (off_820B3800 @ +0, off_820B37CC @ +4) are produced structurally by the dual-base
// + virtual-destructor declaration; the VoiceWrapper array is the ctor's tail
// do/while sub-object construction (4x, stride 0x50, base +0x38).
struct FxEffect : public BrnEffectObject
{
    FxEffect();
    virtual ~FxEffect();

    // +0x38 (X360), stride 0x50 per element. The do/while loop (r29 = 3 -> 0)
    // constructs 4 sub-objects at +0x38/+0x88/+0xD8/+0x128. Reused BY NAME from the
    // minimal CgsVoiceWrapper home (same wrapper the committed ExplosionEffect embeds
    // a single instance of).
    CgsSound::Logic::VoiceWrapper mVoiceWrappers[4];

    // FLAG: the ctor additionally zero-inits un-homed LEAF scalar members at
    // +0x08/+0x0C/+0x10/+0x12/+0x1C/+0x20/+0x30(byte)/+0x34 (pre-array) and
    // +0x18C(word)/+0x190(byte)/+0x191(byte) (post-array). Names/types un-homed
    // (no DWARF, no Feb-2007 source) -- DECLARATION-ONLY, deferred to the layout
    // recon slice. NOT fabricated as named fields here.
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_FX_EFFECT_H
