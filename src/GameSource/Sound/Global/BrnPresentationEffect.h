#ifndef BRN_SOUND_LOGIC_BRN_PRESENTATION_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_PRESENTATION_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"   // committed BrnEffectObject dual base (@+0/+4) BY NAME
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"             // BrnSound::Logic::Streaming::IStreamUser (third base @+0x38)
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"    // CgsSound::Logic::VoiceWrapper (embedded in AgingVoice)
#include "GameSource/AttribSys/Generated/classes/presentationactionlist.h"

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
//   PresentationEffect()                        @ 0x826E7628  (ctor)
//   FindFreeVoice()                             @ 0x82687D68  (DWARF cpp:554)
//   FindOrStealAVoice(const PresentationEntry&) @ 0x826D2AD8  (DWARF cpp:485)
//   `vector deleting destructor'                @ 0x826E78A8  (compiler-synthesised)
//   `vector deleting destructor'`adjustor{4}'   @ 0x826E7728  (compiler-synthesised)
// The two Find* helpers read the per-slot state word (console slot+0x4C == the
// wrapper's named miState) and the stored PresentationEntry (slot+0x58 ==
// AgingVoice::mDataEntry) BY NAME (2026-08-25 wave 4; the u8*-cursor walk is
// retired) and call CgsSound::Logic::VoiceWrapper::Release.
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
    // DWARF BrnPresentationEffect.h:132 (struct PresentationEntry). The per-voice
    // resolved presentation request; the parameter type of FindOrStealAVoice. Field
    // NAMES + order are DWARF-authoritative; the Find* compare loop reaches them at the
    // attested byte offsets: +0x10 mContentSpec (leading word) ; +0x14 mu16Splice ;
    // +0x16 mu8ChokeGroup ; +0x17 mu8Valid ; +0x18 mu8Behaviour (== the stream gate) ;
    // +0x19 mu8MixerOutput.
    struct PresentationEntry
    {
        static const u16 KU16_SPECIAL_SPLICE_STREAM = 65534;   // DWARF :133
        static const u16 KU16_SPECIAL_SPLICE_WAVE   = 65535;   // DWARF :134

        u64 mu64StringId;    // +0x00 (:187)
        u64 mu64ScreenId;    // +0x08 (:188)
        // FLAG: mContentSpec is Command::QueueElement (:190) -- un-homed 4-byte type at
        // +0x10; the Find* loop compares only its leading word. Kept as a correctly-placed
        // opaque word here (NOT fabricated field-by-field).
        u32 mu32ContentSpec; // +0x10 (leading word of mContentSpec)
        u16 mu16Splice;      // +0x14 (:191)
        u8  mu8ChokeGroup;   // +0x16 (:192)
        u8  mu8Valid;        // +0x17 (:193)
        u8  mu8Behaviour;    // +0x18 (:194)
        u8  mu8MixerOutput;  // +0x19 (:195)
    };

    // DWARF BrnPresentationEffect.h:208. One aging voice slot (X360 stride 0x80 ==
    // mu16Age @+0x00 + VoiceWrapper @+0x04 (console 0x50, ends +0x54) + the stored
    // PresentationEntry @+0x58 (u64-aligned) + mfTimeSinceLastTick -- COMPLETED
    // 2026-08-25 wave 4 (the two formerly-DEFERRED members are placeable now that
    // VoiceWrapper's real span is settled; the Find* bodies read them BY NAME).
    struct AgingVoice
    {
        AgingVoice() : mu16Age(0), mVoice(), mDataEntry(), mfTimeSinceLastTick(0.0f) {}
                                // asm: sth 0,slot+0 ; bl VoiceWrapper(slot+4); the entry/
                                // timer zero-seed is the enclosing ctor's leaf zero region.
        u16                           mu16Age;             // slot+0x00 (attested)
        CgsSound::Logic::VoiceWrapper mVoice;              // slot+0x04 (attested; ctor tail-called)
        PresentationEntry             mDataEntry;          // slot+0x58 (DWARF :208 member; the
                                                           //  Find* stored-entry compare target)
        f32                           mfTimeSinceLastTick; // (DWARF :208 member)
    };

    PresentationEffect();          // @ 0x826E7628
    virtual ~PresentationEffect(); // out-of-line anchor; @ 0x826E78A8 vector deleting
                                   // destructor + @ 0x826E7728 adjustor{4} forward to it

    virtual CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    virtual const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);

    virtual bool Attach() override;
    virtual void UpdateParams(f32 afDeltaTime) override;
    virtual void ProcessUpdate() override;
    virtual void Notify(const CgsSound::Io::MessageHeader* apMessage) override;

    virtual const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    virtual void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                   f32 afGain, f32 afElapsedTime) override;
    virtual void StreamStopped() override;

    // DWARF :286; X360 this+0x40, stride 0x80 (attested). The ctor constructs all 4.
    AgingVoice maVoices[4];

    u8 mau8DataOffsets[14];
    u8 mau8DataEnds[14];
    CgsSound::Logic::VoiceWrapper::CreateParams mStreamParams;
    Attrib::Gen::presentationactionlist mActions;
    u8 mu8StreamOutput;

    // FLAG: mau8DataOffsets[14] (:287), mau8DataEnds[14] (:288), mStreamParams
    // (VoiceWrapper::CreateParams, :289), mActions (Attrib::Gen::presentationactionlist,
    // :290; X360 this+0x28C), mMode (DataPoint<eMode>, :292), mu8StreamOutput (:293) are
    // DWARF-ordered but their concrete types have NO committed home -> DECLARATION-ONLY /
    // DEFERRED (NOT emitted as concrete members; same treatment as
    // StreamingEffect::streamsettings).

private:
    bool Resolve(s32 aiAction, u64 auStringId, u64 auScreenId,
                 PresentationEntry& arEntry) const;
    void Play(const PresentationEntry& arEntry);
    // @ 0x82687D68 (DWARF BrnPresentationEffect.cpp:554). First maVoices[] slot whose
    // per-VoiceWrapper state word (slot+0x4C) is 0 (unused) or 7 (idle), else null.
    AgingVoice* FindFreeVoice();

    // @ 0x826D2AD8 (DWARF BrnPresentationEffect.cpp:485). Returns 0 if a busy STREAM
    // already content-matches rEntry; else a free slot, else the best steal candidate
    // (choke-group / oldest age), Releasing the stolen voice first.
    AgingVoice* FindOrStealAVoice(const PresentationEntry& rEntry);
};

} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_BRN_PRESENTATION_EFFECT_H
