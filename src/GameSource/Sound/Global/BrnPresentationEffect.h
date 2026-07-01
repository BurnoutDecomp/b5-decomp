#ifndef BRN_SOUND_LOGIC_BRN_PRESENTATION_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_PRESENTATION_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (@+0/+4) BY NAME
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // BrnSound::Logic::Streaming::IStreamUser (third base @+0x38)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper (embedded in AgingVoice)

// =============================================================================
// BrnSound::Logic::PresentationEffect
//   GameSource/Sound/Global/BrnPresentationEffect.h (DWARF home) +
//   GameSource/Sound/Global/BrnPresentationEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// PresentationEffect is the sound-logic EFFECT OBJECT that drives HUD/presentation
// voices. Its X360 ctor (@ 0x826E7628) settles THREE vptr slots (@0 primary off_820B5F44,
// @4 off_820B5F10, @0x38 off_820B5F04) and the class has a `vector deleting destructor'
// adjustor{4} @ 0x826E7728 (a `this - 4` thunk), which only exists for a base sub-object
// at +4. So PresentationEffect multiply-inherits the committed BrnEffectObject dual base
// (EffectObject primary @ +0, IResourceRequester sub-object @ +4) AND the streaming
// IStreamUser interface (third sub-object @ +0x38) -- mirroring SpeechEffect/
// StreamingEffect. (DWARF line 104 renders only the single IStreamUser base, the
// documented dwarfdump-drops-a-base false-negative; the +0/+4/+0x38 triple + adjustor{4}
// are asm-authoritative and pin the MI shape.)
//
// This TU's SHIPPED function set:
//   PresentationEffect()                      @ 0x826E7628  (ctor)
//   `vector deleting destructor'              @ 0x826E78A8  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}' @ 0x826E7728  (compiler-synthesised)
// FindFree() @ 0x82687D68 and FindOrStealAVoice() @ 0x826D2AD8 are BLOCKED (not shipped):
// they require raw-offset access into un-homed AgingVoice/PresentationEntry compare
// fields (the tag word @ AgingVoice+0x4C, mDataEntry @ +0x68..) plus
// CgsSound::Logic::VoiceWrapper::Release, which cannot be expressed without offset-hacks
// / fabrication -- DEFERRED to their own recon slices.
//
// FLAG (shape vs full surface): the full DWARF member set (mau8DataOffsets[14],
// mau8DataEnds[14], mStreamParams (VoiceWrapper::CreateParams), mActions
// (Attrib::Gen::presentationactionlist @ +0x28C), mMode (DataPoint<eMode>),
// mu8StreamOutput) uses un-homed types and is DEFERRED / declaration-only -- NOT
// fabricated here. Only the attested maVoices[4] AgingVoice array (each slot: mu16Age +
// embedded VoiceWrapper) is materialised BY NAME.
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// PresentationEffect : BrnEffectObject (dual base @ +0/+4) + Streaming::IStreamUser (@ +0x38).
struct PresentationEffect : public BrnEffectObject,
                            public BrnSound::Logic::Streaming::IStreamUser
{
    // DWARF BrnPresentationEffect.h:208. One aging voice slot (X360 stride 0x80). MINIMAL:
    // only the two per-slot ctor effects the asm attests (mu16Age=0 @slot+0x00; embedded
    // VoiceWrapper ctor @slot+0x04) are materialised BY NAME. The DWARF-listed mDataEntry
    // (PresentationEntry) + mfTimeSinceLastTick are DEFERRED (un-homed types); the 0x80
    // stride is an X360 fact NOT reproduced as padding here (offsets not static_asserted).
    struct AgingVoice
    {
        AgingVoice() : mu16Age(0), mVoice() {}   // asm: sth 0,slot+0 ; bl VoiceWrapper(slot+4)
        u16                           mu16Age;    // slot+0x00 (attested)
        CgsSound::Logic::VoiceWrapper mVoice;     // slot+0x04 (attested; ctor tail-called)
        // FLAG: PresentationEntry mDataEntry + f32 mfTimeSinceLastTick DEFERRED (un-homed).
    };

    PresentationEffect();          // @ 0x826E7628
    virtual ~PresentationEffect(); // out-of-line anchor; @ 0x826E78A8 vector deleting
                                   // destructor + @ 0x826E7728 adjustor{4} forward to it

    // DWARF :286; X360 this+0x40, stride 0x80 (attested). The ctor constructs all 4.
    AgingVoice maVoices[4];

    // FLAG: mau8DataOffsets[14] (:287), mau8DataEnds[14] (:288), mStreamParams
    // (VoiceWrapper::CreateParams, :289), mActions (Attrib::Gen::presentationactionlist,
    // :290; X360 this+0x28C), mMode (DataPoint<eMode>, :292), mu8StreamOutput (:293) are
    // DWARF-ordered but their concrete types have NO committed home -> DECLARATION-ONLY /
    // DEFERRED (NOT emitted as concrete members; same treatment as
    // StreamingEffect::streamsettings).
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_PRESENTATION_EFFECT_H
