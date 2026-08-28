#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h" // AemsRWSampleFactory (the real base)
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"  // Registry + RegistrySpec (the in-place carve)
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"    // Handle<AemsFactory>

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

// CgsAemsFactory.h:212 (DWARF). Update command: 3 words (tag + 2 operands).
// Type tag E_CSIS_COMMAND_UPDATE (== 3). X360 ctor @ 0x82681BD0 copies words
// 0,4,8 (lwz/stw 0/4/8) and asserts count == 3 and tag == 3.
struct CsisUpdateCommand : public CsisCommand
{
    // @ 0x82681BD0. Copy-construct from a source record of luCommandCount words.
    CsisUpdateCommand(u32 luCommandCount, const CsisUpdateCommand& arSource);

    uintptr_t maOperands[2]; // words 1..2 -- carried verbatim through the queue
};

const u32 KU_MAX_PATCH_MONITORS = 16; // CgsAemsFactory.h:373 (DWARF)

// The sizing spec Create/ctor consume (console spec words +0 the retained RWAC
// factory handle / +4 entityCount / +8 dataBytes / +0xC stringBytes -- the
// ref-spec r5 lowering, unlike RWAC's by-value packing).
struct AemsFactorySpec
{
    Factory*  mpRwacFactory;        // +0x00 (Handle<GenericRwacFactory> raw pointer)
    u32       mu32EntityCount;      // +0x04
    u32       mu32DataSize;         // +0x08
    u32       mu32StringTableSize;  // +0x0C
};

// CgsAemsFactory.h:291 (DWARF): AemsFactory : public AemsRWSampleFactory.
// REAL base chain now (AEMS-cascade slice 2 -- the former minimal placeholder
// modelled the base as an untyped span). Console layout (host: name+sequence):
//   AemsRWSampleFactory base            through +0x5B
//   muPatchMonitorCount                 +0x5C  (:= 0 LAST, after the CSIS check)
//   mpRegistry                          +0x60  -> the in-place Registry @ +0x56C
//   mpRwacFactory                       +0x64  (retained raw handle from the spec)
//   mCommandQueue payload               +0x68..+0x463 (255-deep CommandQueue<uintptr_t>;
//                                       ctor-untouched) + the two control words
//                                       +0x464/+0x468 (:= 0)
//   maPatchMonitors[16]                 +0x46C
//   (the in-place Registry follows      +0x56C)
class AemsFactory : public AemsRWSampleFactory
{
public:
    // @ 0x826DAC28 (AEMS-cascade slice 2; full decode progress/scratch_dossiers/
    // aems_factory_cascade_codex.md). The ref-spec create: carve (console
    // 4*(entities+0x162)+data+strings at host widths) through the ENVIRONMENT's
    // allocator tagged "AemsFactory", construct, return the handle with one
    // explicit Acquire -- the console increments overall +0x08 == the
    // Factory-subobject refcount, the load-bearing MI proof.
    static Handle<AemsFactory> Create(Environment& arEnvironment,
                                      const AemsFactorySpec& akrSpec);

    // @ 0x826DAAD0. Base (Name = the "~AemsFactory::SK_NAME~" intern == console
    // dword_83008664, writer sub_82C65788), the member stores in console order,
    // the in-place Registry, the CSIS-inited assert, and the global
    // sample-player-factory install. Body in CgsAemsFactory.cpp.
    AemsFactory(Environment& arEnvironment, const AemsFactorySpec& akrSpec);

    // The nested registry (console +0x60), by name -- the GetAemsFactoryRegistry
    // accessor (CgsSoundPlaybackModule.h:100) reads it off the generic Factory*.
    Registry* GetRegistry() { return mpRegistry; }

protected:
    // CgsAemsFactory.cpp @ 0x8268A018 (DWARF CgsAemsFactory.cpp:397, returns
    // void). Debug-prints lpcText through the engine log front-end when the
    // log-category filter is enabled (X360 prints "<NULLSTRING>" for null).
    // ⭐ STATIC (ABI corrected, slice 2): the ctor @0x826DAAD0 materializes this
    // function's RAW ADDRESS as a one-argument callback (the console callee
    // consumes the text in r3); a hidden-this member ABI cannot match.
    static void CsisPrint(const char* lpcText);

    // CgsAemsFactory.cpp @ 0x82689E98. Linear-search the patch-monitor table
    // for the monitor whose mpName matches lpcName; returns it, or null if none.
    PatchMonitor* FindPatchMonitor(const char* lpcName);

private:
    // --- members (console offsets in comments; host by NAME + SEQUENCE) ---
    u32          muPatchMonitorCount;            // CgsAemsFactory.h:364  (+0x5C)
    Registry*    mpRegistry;                     // CgsAemsFactory.h:366  (+0x60)
    Factory*     mpRwacFactory;                  // CgsAemsFactory.h:367  (+0x64, retained)
    // CgsAemsFactory.h:368 mCommandQueue (CsisCommandQueue: 255-deep
    // CommandQueue<uintptr_t>). Payload ctor-untouched; only the two control
    // words are zeroed (console +0x464/+0x468).
    uintptr_t    maCommandQueuePayload[255];     // (+0x68..+0x463)
    u32          mu32CommandQueueControl0;       // (+0x464)
    u32          mu32CommandQueueControl1;       // (+0x468)
    PatchMonitor maPatchMonitors[16];            // CgsAemsFactory.h:375  (+0x46C)
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
