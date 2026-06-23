#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H

#include "types.hpp"

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

// CgsGenericRwacFactory.h (DWARF). The generic-RWAC command tags. Only the four
// values exercised by the command ctors in this home are confirmed against the
// X360 asm (the gaps 0..5/8 are other RWAC commands not homed here):
//   E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS            == 6  (cmpwi r11, 6 @0x826AD5EC)
//   E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS == 7  (cmpwi r11, 7 @0x82681570 site)
//   E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA            == 9  (cmpwi r11, 9 @0x826816A0 site)
//   E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS      == 10 (cmpwi r11, 10 @0x82681738 site)
enum ERwacCommandType
{
    E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS            = 6,
    E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS = 7,
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

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_COMMANDS_H
