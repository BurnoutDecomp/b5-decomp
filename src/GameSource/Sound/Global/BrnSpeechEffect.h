#ifndef BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (BY NAME)
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // BrnSound::Logic::Streaming::IStreamUser (third base)

// =============================================================================
// BrnSound::Logic::SpeechEffect
//   GameSource/Sound/Global/BrnSpeechEffect.h (DWARF home) +
//   GameSource/Sound/Global/BrnSpeechEffect.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// SpeechEffect is the sound-logic EFFECT OBJECT that drives spoken content. It is a
// multiple-inheritance effect-object leaf: the X360 ctor (@ 0x826BB4E8) settles THREE
// vptr slots (@0 primary off_820B23B0, @4 off_820B237C, @0x38 off_820B2370) and the
// class has a `vector deleting destructor' adjustor{4} @ 0x826BB5D0 (a `this - 4`
// thunk), which only exists for a base sub-object at +4. So SpeechEffect derives from
// the committed BrnEffectObject dual base (EffectObject primary @ +0, IResourceRequester
// sub-object @ +4) AND the streaming IStreamUser interface (third sub-object @ +0x38) --
// mirroring StreamingEffect/PresentationEffect. (DWARF renders only the single
// IStreamUser base, the documented dwarfdump-drops-a-base false-negative; the ctor's
// 3-vptr layout + the adjustor{4} thunk are asm-authoritative and pin the MI shape.)
//
// This TU's recon'd function set:
//   SpeechEffect()                            @ 0x826BB4E8  (ctor)
//   `vector deleting destructor'              @ 0x826BB668  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}' @ 0x826BB5D0  (compiler-synthesised)
//   CompassDirectionToString(int)             @ 0x82687000  (leaf switch->string)
//   GameModeToString(int)                     @ 0x82687148  (leaf switch->string)
//   GetCr()                                   @ 0x82687F78  (take-address accessor)
//
// FLAG (shape vs full surface): the full DWARF member set (mePlayState, mParams
// (VoiceWrapper::CreateParams), mbFlagSpeechIsStillPlaying, mbFirstTimeTipPlaying,
// mSpeechData (Attrib::Gen::speechdata @ +0x74), meLanguage, muNextRoadRageIntroIndex,
// muNextStuntRunIntroIndex, muQueuedContentSpec, mbQueuedSpeechIsFirstTimeTip) uses
// un-homed types and is DEFERRED / declaration-only -- NOT fabricated as concrete
// members here (matching the ExplosionEffect/StreamingEffect minimal-home convention).
// Only the +0x08 sub-object address GetCr() returns is modelled (as the opaque Cr).
//
// LAYOUT NOTE (X360 32-bit vs host 64-bit): members are pinned BY NAME + SEQUENCE;
// absolute offsets are NOT static_asserted across pointer members on the 64-bit host.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// SpeechEffect : BrnEffectObject (dual base @ +0/+4) + Streaming::IStreamUser (@ +0x38).
struct SpeechEffect : public BrnEffectObject,
                      public BrnSound::Logic::Streaming::IStreamUser
{
    // Opaque sub-object whose address GetCr() returns at +0x08. Real type UNVERIFIED
    // (the asm merely takes the address); size/layout DEFERRED.
    struct Cr;

    SpeechEffect();                // @ 0x826BB4E8
    virtual ~SpeechEffect();       // out-of-line anchor; @ 0x826BB668 vector deleting
                                   // destructor + @ 0x826BB5D0 adjustor{4} forward to it

    // @ 0x82687F78. Return the address of the sub-object @ +0x08.
    Cr* GetCr();

    // @ 0x82687000 / @ 0x82687148. Leaf switch-to-string helpers (the `this` r3 arg is
    // never dereferenced; kept as members to match the X360 mangled names
    // BrnSound::Logic::SpeechEffect::CompassDirectionToString / ::GameModeToString).
    const char* CompassDirectionToString( int liDirection );
    const char* GameModeToString( int a1, int a2 );

    // The Cr sub-object GetCr() returns the address of (X360 +0x08, past the base
    // vptr region). Modelled as a named opaque member AFTER the BrnEffectObject base
    // (whose leading vptr/data region the ctor zeroes); GetCr returns &maCr[0]. Element
    // type DEFERRED (the asm merely takes its address). The remainder of SpeechEffect
    // (through the ctor's +0x74 speechdata sub-object) is DEFERRED (un-homed member
    // types) and NOT fabricated here.
    u8 maCr[1];
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H
