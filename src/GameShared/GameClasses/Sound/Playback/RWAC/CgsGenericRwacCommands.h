#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H

#include "types.hpp"

#include <cstdint> // std::uintptr_t

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
// Cited by X360 address only -- no leaked-source provenance.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

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
    ERwacCommandType GetCommandType() const { return static_cast<ERwacCommandType>(muCommandType); }

    uintptr_t muCommandType; // word 0 -- ERwacCommandType tag
};

// Reference-counted referent of a PlayerPlay command's first operand. The X360
// ctor (@0x826AD578) treats word 1 as a pointer to an object whose dword at +4 is
// a reference count: when the pointer is non-null it does lwz r10,4(r11) / addi
// r10,r10,1 / stw r10,4(r11) -- i.e. ++referent->mu32RefCount. The referent's
// other members are opaque to the ctor; only the +4 refcount word is touched, so
// it is modelled by name with a single leading opaque word ahead of the count to
// place the count at +4. (Documentation offset; not static_asserted.)
struct RwacPlayRequest
{
    uintptr_t muReserved0;  // +0 -- opaque (not touched by the ctor)
    u32       mu32RefCount; // +4 -- incremented when a PlayerPlay command copies it
};

// Player-play parameters: 3 words (tag + 2 operands). Tag == 6.
// X360 ctor @0x826AD578: copies words 0,1,2; when operand 0 (the play-request
// pointer) is non-null, increments its +4 refcount.
struct RwacCommandPlayerPlayParameters : public RwacCommand
{
    // @ 0x826AD578. Copy-construct from a source record of luCommandCount words.
    RwacCommandPlayerPlayParameters(u32 luCommandCount,
                                    const RwacCommandPlayerPlayParameters& arSource);

    RwacPlayRequest* mpPlayRequest; // word 1 -- ref-counted; AddRef'd on copy
    uintptr_t        maOperand;     // word 2 -- carried verbatim through the queue
};

// Player-is-request-done parameters: 2 words (tag + 1 operand). Tag == 7.
// X360 ctor @0x82681570: copies words 0,1 (no refcount step).
struct RwacCommandPlayerIsRequestDoneParameters : public RwacCommand
{
    // @ 0x82681570. Copy-construct from a source record of luCommandCount words.
    RwacCommandPlayerIsRequestDoneParameters(u32 luCommandCount,
                                             const RwacCommandPlayerIsRequestDoneParameters& arSource);

    uintptr_t maOperand; // word 1 -- carried verbatim through the queue
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
    // @ 0x82681170. Build from a playback voice, a voice pointer-slot, a plugin
    // pointer-slot, a config pointer and one trailing operand.
    RwacCommandVoiceCreateInstance(uintptr_t apPlaybackVoice, uintptr_t appVoice,
                                   uintptr_t apppPlugin, uintptr_t apConfig,
                                   uintptr_t auOperand);

    uintptr_t mpPlaybackVoice; // word 1 -- asserted non-null ("mpPlaybackVoice")
    uintptr_t mppVoice;        // word 2 -- asserted non-null ("mppVoice")
    uintptr_t mpppPlugin;      // word 3 -- asserted non-null ("mpppPlugin")
    uintptr_t mpConfig;        // word 4 -- asserted non-null ("mpConfig")
    uintptr_t maOperand;       // word 5 -- carried verbatim through the queue
};

// Voice-release command: 2 words (tag + 1 operand). Tag == 2.
// X360 copy ctor @0x82681320: copies words 0,1 from arSource, then asserts
// (a) luCommandCount == 2 ("sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
//     CgsGenericRwacFactory.h:252; X360 cmplwi cr6,r4,2) and
// (b) the copied tag == 2 ("E_RWAC_COMMAND_VOICE_RELEASE == GetCommandType()",
//     CgsGenericRwacFactory.h:253; X360 cmpwi r11,2 against the just-stored 0(this)).
struct RwacCommandVoiceRelease : public RwacCommand
{
    // @ 0x82681320. Copy-construct from a source record of luCommandCount words.
    RwacCommandVoiceRelease(u32 luCommandCount, const RwacCommandVoiceRelease& arSource);

    uintptr_t maOperand; // word 1 -- carried verbatim through the queue
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
    // @ 0x826813B8. Build from a (non-null) plugin pointer and two operands.
    RwacCommandPluginEvent(uintptr_t apPlugin, uintptr_t auOperand1, uintptr_t auOperand2);

    uintptr_t mpPlugin;   // word 1 -- the target plugin (asserted non-null)
    uintptr_t maOperand1; // word 2 -- carried verbatim through the queue
    uintptr_t maOperand2; // word 3 -- carried verbatim through the queue
};

// Plugin-get-attribute command: 4 words (tag + 3 operands). Tag == 5.
// X360 copy ctor @0x826814C8: copies words 0..3 from arSource, then asserts
// (a) luCommandCount == 4 ("sizeof(*this) / sizeof(uintptr_t) == luCommandCount",
//     CgsGenericRwacFactory.h:408; X360 cmplwi cr6,r4,4) and
// (b) the copied tag == 5 ("E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE == GetCommandType()",
//     CgsGenericRwacFactory.h:409; X360 cmpwi r11,5 against the just-stored 0(this)).
struct RwacCommandPluginGetAttribute : public RwacCommand
{
    // @ 0x826814C8. Copy-construct from a source record of luCommandCount words.
    RwacCommandPluginGetAttribute(u32 luCommandCount,
                                  const RwacCommandPluginGetAttribute& arSource);

    uintptr_t maOperand1; // word 1 -- carried verbatim through the queue
    uintptr_t maOperand2; // word 2 -- carried verbatim through the queue
    uintptr_t maOperand3; // word 3 -- carried verbatim through the queue
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

    // @ 0x82681888. Pop one command word into *apOutWord and advance the read
    // cursor (with wrap). Asserts the ring is non-empty before reading. Returns
    // `this` (the X360 leaves the queue pointer in r3 on the normal path).
    RwacCommandQueue* GetCommand(uintptr_t* apOutWord);
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H
