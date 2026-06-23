#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H

#include "types.hpp"

// =============================================================================
// CgsSound::Playback::AemsFactory  (+ supporting CSIS command types)
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsFactory.h (DWARF home) +
//   GameShared/GameClasses/Sound/Playback/aems/CgsAemsFactory.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This home models the slice of
// AemsFactory exercised by the two boot-trace functions in this TU:
//   AemsFactory::CsisPrint(const char*)      @ 0x8268A018
//   AemsFactory::FindPatchMonitor(const char*) @ 0x82689E98
//
// FLAG (MINIMAL home of a deep un-homed base): the real AemsFactory derives from
// AemsRWSampleFactory (-> ... -> Factory), a large engine factory hierarchy that is
// NOT yet reconstructed. The X360 functions reach this->muPatchMonitorCount at
// +0x5C (=92) and this->maPatchMonitors at +0x46C (=1132); between them sit the
// registry pointer, the RWAC factory handle and a 255-deep CsisCommandQueue. None
// of that base hierarchy is homed here. To keep the two leaf functions bodyable BY
// NAME (the project rule forbids raw-offset member access), this home models the
// AemsFactory members the functions touch (muPatchMonitorCount, maPatchMonitors)
// plus typed-by-name PLACEHOLDERS for the intervening members, on a plain
// (non-engine-base) struct. Because the base chain is not modelled, ABSOLUTE X360
// offsets (+92 / +1132) are intentionally NOT static_asserted -- only the member
// names and access logic are load-bearing. The full base hierarchy + the factory's
// virtual surface (DoCreateVoice/DoCreateContent/DoUpdate/ctors/Create) is DEFERRED
// to a dedicated AemsFactory keystone TU.
// =============================================================================

namespace CgsSound
{
namespace Playback
{

// CgsAemsFactory.h:59 (DWARF).
enum ECsisCommandType
{
    E_CSIS_COMMAND_SET_CLASS_HANDLE = 0,
    E_CSIS_COMMAND_CREATE           = 1,
    E_CSIS_COMMAND_RELEASE          = 2,
    E_CSIS_COMMAND_UPDATE           = 3,
};

// CgsAemsFactory.h:68 (DWARF). One registered AEMS patch monitor.
struct PatchMonitor
{
    const char* mpName;       // CgsAemsFactory.h:70
    void*       mpClientFunc; // CgsAemsFactory.h:71
    void*       mpClientData; // CgsAemsFactory.h:72
    s32         miPerfmon;    // CgsAemsFactory.h:73
};

// =============================================================================
// CSIS command structures (CgsAemsFactory.h). These are the fixed-size command
// records queued through the AEMS CsisCommandQueue (a CommandQueue<uintptr_t>). Each
// record is a run of pointer-width words; the leading word holds the ECsisCommandType
// tag (so GetCommandType() == the first word). The per-command constructors copy a
// source record word-for-word, then assert (a) the source's command-count matches
// this record's word count (sizeof(*this)/sizeof(uintptr_t) == luCommandCount) and
// (b) the copied type tag matches the expected ECsisCommandType for the class.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CsisSetClassHandleCommand::ctor @ 0x826819E0  (5 words, type == 0)
//   CsisCreateCommand::ctor         @ 0x82681A90  (4 words, type == 1)
//   CsisReleaseCommand::ctor        @ 0x82681B38  (2 words, type == 2)
//
// X360 stores prove the word counts: the ctors copy lwz/stw at 0,4,8,0xC[,0x10]
// (Create/SetClassHandle copy 4/5 words; Release copies 2). Word 0 is the type tag.
// The payload words past the tag are command-specific operands; their per-field
// meaning beyond "carried verbatim through the queue" is not recovered from the ctor
// alone, so they are modelled as named pointer-width operand slots. Member access is
// BY NAME; no raw-offset cast.
//
// luCommandCount is the source-supplied word count the ctor validates against the
// record's own size. It is modelled as u32 (the X360 passes it in r4 and compares as
// a 32-bit immediate). On the X360 a "word" is a 4-byte uintptr_t; the queue element
// type is uintptr_t, so the operand slots are typed uintptr_t to track the queue's
// element width by name (absolute X360 offsets are documentation only, not asserted).
// =============================================================================

// CgsAemsFactory.h:80 (DWARF). The shared command base: the leading type-tag word.
struct CsisCommand
{
    // GetCommandType() reads the leading tag word (X360 lwz 0(this)).
    ECsisCommandType GetCommandType() const { return static_cast<ECsisCommandType>(muCommandType); }

    uintptr_t muCommandType; // word 0 -- ECsisCommandType tag
};

// CgsAemsFactory.h:130 (DWARF). Set-class-handle command: 5 words (tag + 4 operands).
// Type tag E_CSIS_COMMAND_SET_CLASS_HANDLE (== 0).
struct CsisSetClassHandleCommand : public CsisCommand
{
    // @ 0x826819E0. Copy-construct from a source record of luCommandCount words.
    CsisSetClassHandleCommand(u32 luCommandCount, const CsisSetClassHandleCommand& arSource);

    uintptr_t maOperands[4]; // words 1..4 -- carried verbatim through the queue
};

// CgsAemsFactory.h:160 (DWARF). Create command: 4 words (tag + 3 operands).
// Type tag E_CSIS_COMMAND_CREATE (== 1).
struct CsisCreateCommand : public CsisCommand
{
    // @ 0x82681A90. Copy-construct from a source record of luCommandCount words.
    CsisCreateCommand(u32 luCommandCount, const CsisCreateCommand& arSource);

    uintptr_t maOperands[3]; // words 1..3 -- carried verbatim through the queue
};

// CgsAemsFactory.h:186 (DWARF). Release command: 2 words (tag + 1 operand).
// Type tag E_CSIS_COMMAND_RELEASE (== 2).
struct CsisReleaseCommand : public CsisCommand
{
    // @ 0x82681B38. Copy-construct from a source record of luCommandCount words.
    CsisReleaseCommand(u32 luCommandCount, const CsisReleaseCommand& arSource);

    uintptr_t maOperand; // word 1 -- carried verbatim through the queue
};

const u32 KU_MAX_PATCH_MONITORS = 16; // CgsAemsFactory.h:373 (DWARF)

// CgsAemsFactory.h:291 (DWARF): AemsFactory : public AemsRWSampleFactory.
// MINIMAL home -- see file banner FLAG. Only the patch-monitor table and the two
// leaf functions are modelled; the engine factory base is a typed-by-name
// placeholder.
class AemsFactory
{
public:
    // CgsAemsFactory.cpp @ 0x8268A018. Debug-prints lpcText through the engine
    // log front-end when the log-category filter is enabled; returns lpcText so it
    // can chain. (X360 returns "<NULLSTRING>" when handed a null pointer.)
    const char* CsisPrint(const char* lpcText);

protected:
    // CgsAemsFactory.cpp @ 0x82689E98. Linear-search the patch-monitor table
    // for the monitor whose mpName matches lpcName; returns it, or null if none.
    PatchMonitor* FindPatchMonitor(const char* lpcName);

private:
    // --- members (DWARF order; X360 offsets in comments, NOT asserted) ---
    // FLAG: placeholder for the un-homed AemsRWSampleFactory base sub-object that
    // precedes muPatchMonitorCount in the X360 layout. Not modelled by field.
    u32         muPatchMonitorCount;       // CgsAemsFactory.h:364  (+0x5C X360)
    void*       mpRegistry;                // CgsAemsFactory.h:366  (Registry*, opaque)
    void*       mhRwacFactory;             // CgsAemsFactory.h:367  (GenericRwacFactoryHandle, opaque)
    // CgsAemsFactory.h:368 mCommandQueue (CsisCommandQueue: 255-deep CommandQueue<uintptr_t>).
    // Modelled only by size so maPatchMonitors keeps its position relative to the
    // count; contents are not used by this TU's functions.
    uintptr_t   maCommandQueuePlaceholder[256]; // count word + 255 slots
    PatchMonitor maPatchMonitors[16];      // CgsAemsFactory.h:375  (+0x46C X360)
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
