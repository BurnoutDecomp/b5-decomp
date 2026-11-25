#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H

#include "types.hpp"

#include "types.hpp" // std::uintptr_t

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"

namespace rw { namespace audio { namespace core { class PlugIn; class Voice; } } }

// ARTIST's file string for the asserts of this header's inline bodies (0x820AAE38):
// the original declared these records in CgsGenericRwacFactory.h.
#define CGS_RWAC_FACTORY_H_ASSERT_FILE \
    "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Rwac/CgsGenericRwacFactory.h"

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Rwac/CgsGenericRwacCommands.h
//
// Home for the fixed-size generic-RWAC command / parameter records queued and
// dispatched through GenericRwacFactory::HandlePluginEvent. Each record is a run
// of pointer-width words; the leading word holds an ERwacCommandType tag (so
// GetCommandType() == the first word). The per-command copy constructors copy a
// source record word-for-word, then assert (a) the source-supplied word count
// matches this record's own word count (sizeof(*this)/sizeof(uintptr_t) ==
// luCommandCount) and (b) the copied tag matches the expected ERwacCommandType.
//
// These mirror the CSIS command family in CgsAemsFactory.h. The factory class
// (GenericRwacFactory) is its own TU; this header homes ONLY the command/param
// records, declared by name, so the command ctor TUs are bodyable without
// raw-offset access.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (X360 cmpwi/cmplwi values are
// authoritative):
//   RwacCommandPlayerPlayParameters::ctor          @ 0x826AD578  (3 words, tag 6)
//   RwacCommandPlayerIsRequestDoneParameters::ctor @ 0x82681570  (2 words, tag 7)
//   RwacCommandApplyReverbIRFile::ctor             @ 0x826816A0  (2 words, tag 9)
//   RwacCommandGinsuAttachDataParameters::ctor     @ 0x82681738  (2 words, tag 10)
//
// X360 stores prove the word counts: each ctor copies lwz/stw at 0,4[,8]; word 0
// is the type tag. Payload words past the tag are command-specific operands whose
// per-field meaning beyond "carried verbatim through the queue" is not recovered
// from the ctor alone, so they are modelled as named pointer-width operand slots.
//
// luCommandCount is the source-supplied word count the ctor validates against the
// record's own size (X360 passes it in r4, compares as a 32-bit immediate). A
// "word" is a 4-byte uintptr_t; the queue element type is uintptr_t, so the
// operand slots are typed uintptr_t to track the queue's element width by name.
// Absolute X360 offsets are documentation only, not static_asserted.
//
// Cited by X360 address only -- no reference-source provenance.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

struct GenericRwacWaveContent;
class Voice;
struct GenericRwacVoiceConfig;

// CgsGenericRwacFactory.h (DWARF). The generic-RWAC command tags. The values
// exercised by the command ctors in this home are confirmed against the X360 asm
// (the remaining gaps are other RWAC commands not homed here):
//   E_RWAC_COMMAND_VOICE_CREATE_INSTANCE             == 1  (li r11,1; stw 0(this) @0x82681194)
//   E_RWAC_COMMAND_VOICE_RELEASE                     == 2  (cmpwi r11,2 @0x82681378 site)
//   E_RWAC_COMMAND_PLUGIN_EVENT                      == 3  (li r11,3; stw 0(this) @0x826813CC)
//   E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE              == 5  (cmpwi r11,5 @0x82681530 site)
//   E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS            == 6  (cmpwi r11, 6 @0x826AD5EC)
//   E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS == 7  (cmpwi r11, 7 @0x82681570 site)
//   E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS           == 8  (cmpwi r11,8 @0x82681660 site)
//   E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA            == 9  (cmpwi r11, 9 @0x826816A0 site)
//   E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS      == 10 (cmpwi r11, 10 @0x82681738 site)
enum ERwacCommandType
{
    E_RWAC_COMMAND_VOICE_CREATE_INSTANCE             = 1,
    E_RWAC_COMMAND_VOICE_RELEASE                     = 2,
    E_RWAC_COMMAND_PLUGIN_EVENT                      = 3,
    E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE              = 5,
    E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS            = 6,
    E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS = 7,
    E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS           = 8,
    E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA            = 9,
    E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS      = 10,
};

// The shared command base: the leading type-tag word. Matches the CSIS
// CgsCommand shape (GetCommandType reads the leading word, X360 lwz 0(this)).
struct RwacCommand
{
    // CgsGenericRwacFactory.h:125 (DWARF). Every record names its type through this base
    // constructor (ARTIST schedules the tag store with the other members, e.g. before
    // the GetPlugin call in GenericRwacContentSlot::DoStop 0x826C1FF8).
    RwacCommand(ERwacCommandType leCommandType) : muCommandType(leCommandType) {}

    ERwacCommandType GetCommandType() const { return static_cast<ERwacCommandType>(muCommandType); }

    // CgsGenericRwacFactory.h:131 (DWARF). The record as the word run the queue posts.
    const uintptr_t* AsCommand() const { return &muCommandType; }

    uintptr_t muCommandType; // word 0 -- ERwacCommandType tag

protected:
    // CgsGenericRwacFactory.h:134 (DWARF). The copy forms take the type word as read.
    RwacCommand(uint32_t luCommandType) : muCommandType(luCommandType) {}
};

// Player-play parameters: 3 words (tag + 2 operands). Tag == 6.
// DecFIGS names the two operands: an owning GenericRwacWaveContent handle and
// the address receiving SndPlayer1's request handle. X360 ctor @0x826AD578
// copies words 0,1,2 and acquires the wave-content object.
struct RwacCommandPlayerPlayParameters : public RwacCommand
{
    RwacCommandPlayerPlayParameters(GenericRwacWaveContent* apWaveContent,
                                    f32* apRequestHandle);
    // @ 0x826AD578. Copy-construct from a source record of luCommandCount words.
    RwacCommandPlayerPlayParameters(u32 luCommandCount,
                                    const RwacCommandPlayerPlayParameters& arSource);
    ~RwacCommandPlayerPlayParameters();

    GenericRwacWaveContent* mpWaveContent; // word 1 -- owning handle payload
    f32* mpRequestHandle;                    // word 2 -- post-play handle destination
};

// Player-is-request-done parameters: 2 words (tag + 1 operand). Tag == 7.
// X360 ctor @0x82681570: copies words 0,1 (no refcount step).
struct RwacCommandPlayerIsRequestDoneParameters : public RwacCommand
{
    // CgsGenericRwacFactory.h:489 (DWARF). Header-inline, no ARTIST copy:
    // GenericRwacPlayerVoice::DoUpdate inlines it (0x826D9AEC..0x826D9AFC: tag 7, then
    // the address of the voice's IsRequestDoneParams).
    explicit RwacCommandPlayerIsRequestDoneParameters(void* apParams)
        : RwacCommand(E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS)
    {
        mpParams = apParams;
    }
    // @ 0x82681570. Copy-construct from a source record of luCommandCount words.
    RwacCommandPlayerIsRequestDoneParameters(u32 luCommandCount,
                                             const RwacCommandPlayerIsRequestDoneParameters& arSource);

    void* mpParams; // word 1 -- SndPlayer1::IsRequestDoneParams*
};

// Apply-reverb-IR-file command: 2 words (tag + 1 operand). Tag == 9.
// X360 ctor @0x826816A0: copies words 0,1 (no refcount step).
struct RwacCommandApplyReverbIRFile : public RwacCommand
{
    // @ 0x826816A0. Copy-construct from a source record of luCommandCount words.
    RwacCommandApplyReverbIRFile(u32 luCommandCount,
                                 const RwacCommandApplyReverbIRFile& arSource);

    uintptr_t maOperand; // word 1 -- carried verbatim through the queue
};

// Ginsu attach-data parameters: 2 words (tag + 1 operand). Tag == 10.
// X360 ctor @0x82681738: copies words 0,1 (no refcount step).
struct RwacCommandGinsuAttachDataParameters : public RwacCommand
{
    // CgsGinsuSlot.cpp:73 / DecFIGS DoPlay: build the two-word command from
    // GinsuPlayer::PlayParams (whose sole field is the loaded .gin data pointer).
    explicit RwacCommandGinsuAttachDataParameters(void* apRamData);
    // @ 0x82681738. Copy-construct from a source record of luCommandCount words.
    RwacCommandGinsuAttachDataParameters(u32 luCommandCount,
                                         const RwacCommandGinsuAttachDataParameters& arSource);

    uintptr_t maOperand; // word 1 -- carried verbatim through the queue
};

// Voice-create-instance command: 6 words (tag + 5 operands). Tag == 1.
// X360 building ctor @0x82681170: builds the record from individual arguments
// rather than copying a source record. Store order (r31 == this): stw r4,4(this)
// [mpPlaybackVoice]; li r11,1 / stw r11,0(this) [tag]; stw r5,8(this) [mppVoice];
// stw r6,0xC(this) [mpppPlugin]; stw r7,0x10(this) [mpConfig]; stw r8,0x14(this)
// [trailing operand]. It then asserts FOUR pointers non-null (the X360 cmplwi each
// against 0): mpPlaybackVoice (CgsGenericRwacFactory.h:173), mppVoice (:174),
// mpppPlugin (:175), mpConfig (:176). The 6th word (word 5, +0x14) carries no
// assert. Like RwacCommandPluginEvent this carries no luCommandCount/tag self-check.
struct RwacCommandVoiceCreateInstance : public RwacCommand
{
    // @ 0x82681170 (DWARF CgsGenericRwacFactory.h:165). Build from the playback voice,
    // the slots the factory fills (voice, plug-in array), the config and the optional
    // submix output slot.
    RwacCommandVoiceCreateInstance(Voice* lpPlaybackVoice, rw::audio::core::Voice** lppVoice,
                                   rw::audio::core::PlugIn*** lpppPlugin,
                                   GenericRwacVoiceConfig* lpConfig,
                                   rw::audio::core::PlugIn** lppSubmix);

    Voice*                     mpPlaybackVoice; // word 1 -- DWARF h:225, asserted non-null
    rw::audio::core::Voice**   mppVoice;        // word 2 -- h:226, asserted non-null
    rw::audio::core::PlugIn*** mpppPlugin;      // word 3 -- h:227, asserted non-null
    GenericRwacVoiceConfig*    mpConfig;        // word 4 -- h:228, asserted non-null
    rw::audio::core::PlugIn**  mppSubmix;       // word 5 -- h:229, may be null
};

// Voice-release command: 2 words (tag + 1 operand). Tag == 2.
// X360 copy ctor @0x82681320: copies words 0,1 from arSource, then asserts
// (a) luCommandCount == 2 ("sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
//     CgsGenericRwacFactory.h:252; X360 cmplwi cr6,r4,2) and
// (b) the copied tag == 2 ("E_RWAC_COMMAND_VOICE_RELEASE == GetCommandType()",
//     CgsGenericRwacFactory.h:253; X360 cmpwi r11,2 against the just-stored 0(this)).
struct RwacCommandVoiceRelease : public RwacCommand
{
    // CgsGenericRwacFactory.h:239 (DWARF). Header-inline, no ARTIST copy: the
    // GenericRwacVoice destructor inlines it (0x826C1E04..0x826C1E38: tag 2, the voice,
    // then the "mpVoice" assert on line 243).
    RwacCommandVoiceRelease(rw::audio::core::Voice* lpVoice)
        : RwacCommand(E_RWAC_COMMAND_VOICE_RELEASE)
    {
        mpVoice = lpVoice;
        CGS_ASSERT(mpVoice, CGS_ASSERT_MESSAGE_AT("mpVoice",
            CGS_RWAC_FACTORY_H_ASSERT_FILE, 243));
    }

    // @ 0x82681320. Copy-construct from a source record of luCommandCount words.
    RwacCommandVoiceRelease(u32 luCommandCount, const RwacCommandVoiceRelease& arSource);

    rw::audio::core::Voice* mpVoice; // word 1 -- DWARF h:263
};

// Send-connect-parameters command: 2 words (tag + 1 operand). Tag == 8.
// X360 copy ctor @0x82681608: copies words 0,1 from arSource, then asserts
// (a) luCommandCount == 2 ("sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
//     CgsGenericRwacFactory.h:532; X360 cmplwi cr6,r4,2) and
// (b) the copied tag == 8 ("E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS == GetCommandType()",
//     CgsGenericRwacFactory.h:533; X360 cmpwi r11,8 against the just-stored 0(this)).
struct RwacCommandSendConnectParameters : public RwacCommand
{
    // @ 0x82681608. Copy-construct from a source record of luCommandCount words.
    RwacCommandSendConnectParameters(u32 luCommandCount,
                                     const RwacCommandSendConnectParameters& arSource);

    uintptr_t maOperand; // word 1 -- carried verbatim through the queue
};

// Plugin-event command: 4 words (tag + 3 operands). Tag == 3.
// X360 building ctor @0x826813B8: stores tag 3 to word 0, the plugin pointer (arg)
// to word 1, then two further operands to words 2,3. It asserts the plugin pointer
// is non-null ("mpPlugin", CgsGenericRwacFactory.h:289). Unlike the copy ctors this
// one builds the record from individual arguments rather than copying a source
// record, and carries no luCommandCount/tag self-check (only the mpPlugin non-null
// guard), matching the X360 asm (cmplwi cr6,r4,0 -- the plugin pointer, not a count).
struct RwacCommandPluginEvent : public RwacCommand
{
    // CgsGenericRwacFactory.h:274 (DWARF): whether a parameter record follows the event.
    enum EParameters
    {
        E_NO_PARAMETERS  = 0,
        E_HAS_PARAMETERS = 1
    };

    // CgsGenericRwacFactory.h:283 (DWARF), header-inline: ARTIST inlines it into the
    // content slots (GenericRwacContentSlot::DoStop 0x826C1FF8..0x826C204C: tag 3, the
    // plug-in, the event, the parameter flag, then the "mpPlugin" assert on line 289)
    // and keeps an out-of-line copy at 0x826813B8 (called by
    // GenericRwacPlayerVoice::DoUpdate 0x826D9AE8).
    RwacCommandPluginEvent(rw::audio::core::PlugIn* lpPlugin, uintptr_t luEvent,
                           EParameters leParameters)
        : RwacCommand(E_RWAC_COMMAND_PLUGIN_EVENT)
    {
        mpPlugin = lpPlugin;
        muEvent = luEvent;
        muParameters = leParameters;
        CGS_ASSERT(mpPlugin, CGS_ASSERT_MESSAGE_AT("mpPlugin",
            CGS_RWAC_FACTORY_H_ASSERT_FILE, 289));
    }

    rw::audio::core::PlugIn* mpPlugin;     // word 1 -- DWARF h:324
    uintptr_t                muEvent;      // word 2 -- h:325
    uintptr_t                muParameters; // word 3 -- h:326 (an EParameters value)
};

// Plugin-get-attribute command: 4 words (tag + 3 operands). Tag == 5.
// X360 copy ctor @0x826814C8: copies words 0..3 from arSource, then asserts
// (a) luCommandCount == 4 ("sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
//     CgsGenericRwacFactory.h:408; X360 cmplwi cr6,r4,4) and
// (b) the copied tag == 5 ("E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE == GetCommandType()",
//     CgsGenericRwacFactory.h:409; X360 cmpwi r11,5 against the just-stored 0(this)).
struct RwacCommandPluginGetAttribute : public RwacCommand
{
    // CgsGenericRwacFactory.h:392 (DWARF). Build the request: header-inline, no ARTIST
    // copy; GenericRwacVoice::Update<T> inlines it (0x826D8214..0x826D8248: tag 5, the
    // attribute, the address, the plug-in, then the "lpPlugin" assert on line 397).
    RwacCommandPluginGetAttribute(rw::audio::core::PlugIn* lpPlugin, uintptr_t luAttribute,
                                  float* lpfAddress)
        : RwacCommand(E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE)
    {
        mpPlugin = lpPlugin;
        muAttribute = luAttribute;
        mpfAddress = lpfAddress;
        CGS_ASSERT(lpPlugin, CGS_ASSERT_MESSAGE_AT("lpPlugin",
            CGS_RWAC_FACTORY_H_ASSERT_FILE, 397));
    }

    // @ 0x826814C8. Copy-construct from a source record of luCommandCount words.
    RwacCommandPluginGetAttribute(u32 luCommandCount,
                                  const RwacCommandPluginGetAttribute& arSource);

    rw::audio::core::PlugIn* mpPlugin;    // word 1 -- CgsGenericRwacFactory.h:431
    uintptr_t                muAttribute; // word 2 -- :432
    float*                   mpfAddress;  // word 3 -- :433
};

// CgsGenericRwacFactory.h:737 (DWARF region). The RWAC-side command ring: a
// fixed-capacity single-producer/single-consumer ring of E_QUEUE_LENGTH command
// words. X360 GetCommand @0x82681888 proves the layout: the read cursor lives at
// byte +0x4000 and the write cursor at +0x4004 (lwz 0x4000/0x4004(this)), i.e. the
// ring is 4096 (0x1000) uintptr_t words at +0, followed by the two u32 cursors. The
// read cursor advances with wrap mask 0xFFF (clrlwi r11,r11,20 == & 0xFFF), so the
// capacity is 0x1000 words.
struct RwacCommandQueue
{
    static const unsigned int E_QUEUE_LENGTH = 0x1000u; // 4096 command words

    uintptr_t mauCommandQueue[E_QUEUE_LENGTH]; // +0x0000  the command-word ring
    u32       mu32Read;                        // +0x4000  consumer cursor
    u32       mu32Write;                       // +0x4004  producer cursor

    RwacCommandQueue() : mu32Read(0), mu32Write(0) {}

    // CgsGenericRwacFactory.h:657. Pop one command word and advance the read cursor.
    void GetCommand(uintptr_t& arWord);

    bool IsEmpty() const { return mu32Read == mu32Write; }

    // CgsGenericRwacFactory.h:640 (DWARF). Post a record: its word count, then its words.
    // Header-inline: ARTIST keeps a copy at 0x82693F50 (assert "lpuFirstCommand", line
    // 690) and inlines it into GenericRwacVoice::Update<T> (0x826D8270..0x826D82A4).
    void PostCommand(uint32_t luCommandCount, const uintptr_t* lpuFirstCommand)
    {
        CGS_ASSERT(lpuFirstCommand, CGS_ASSERT_MESSAGE_AT("lpuFirstCommand",
            CGS_RWAC_FACTORY_H_ASSERT_FILE, 690));
        PostCommand(luCommandCount);
        for (uint32_t i = 0; i < luCommandCount; ++i)
        {
            PostCommand(lpuFirstCommand[i]);
        }
    }

    // CgsGenericRwacFactory.h:672 (DWARF `operator<< <T>(const T& lCommand)`, one
    // instance per command record). Always inlined on ARTIST.
    template <class T>
    RwacCommandQueue& operator<<(const T& lCommand)
    {
        PostCommand(sizeof(T) / sizeof(uintptr_t), lCommand.AsCommand());
        return *this;
    }

private:
    // CgsGenericRwacFactory.h:653 (DWARF). Store the word, advance the
    // write cursor with wrap, then assert the ring did not fill (streamed message, line
    // 730). ARTIST keeps it out of line (0x826817D0, unnamed) and every poster calls it.
    void PostCommand(uintptr_t luCommand)
    {
        mauCommandQueue[mu32Write] = luCommand;
        mu32Write = (mu32Write + 1) & (E_QUEUE_LENGTH - 1);
        CGS_ASSERT(mu32Write != mu32Read, CGS_ASSERT_STREAM_MESSAGE_AT(
            "Out of command buffer. SKU32_COMMAND_COUNT", CGS_RWAC_FACTORY_H_ASSERT_FILE, 730));
    }


};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H
