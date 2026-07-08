#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICEBANKSTATISTICS_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICEBANKSTATISTICS_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h" // SpliceManager::SpliceContainer

// ============================================================================
// CgsSound::Playback::SpliceBankStatistics  (DWARF home CgsSplicerContent.h:45).
//
// Per-splice-bank play-count bookkeeping: on construction (with a splice bank) it
// allocates a per-splice Stats bank through the global SpliceManager, zero-fills it,
// and prepends itself onto an intrusive list (spHead / mpNext). On destruction it
// frees the bank through the SpliceManager heap and unlinks. Additionally it can
// dump its state to the debug TTY (DumpToTty / DumpAllToTty).
//
// Member names/types are DWARF ground truth (CgsSplicerContent.h:133..140); the
// original commit reconstructed the same layout store-for-store from the X360 ctor
// (@0x826D57E8) + dtor (@0x826A1158) but with SPECULATIVE names -- corrected here to
// the DecFIGS declarations (which the X360 ledger attests). The offsets are unchanged;
// only the identifiers/types are refined.
//
// Layout (X360 word offsets; host-width FLAG -- pointers widen, pinned BY NAME):
//   +0x00 mpSpliceContent  +0x04 mpSpliceBank  +0x08 mpaStats  +0x0C muStatCount
//   +0x10 mpNext           +0x14 muIndex
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// Forward decl (full home CgsSplicerContent.h). Only referenced by pointer here; the
// TTY dump reads its GetContentSpec().GetName() in the .cpp where the type is complete.
struct SplicerContent;

struct SpliceBankStatistics
{
    // CgsSplicerContent.h:128 (DWARF). One per splice: its play count. On X360 the
    // Stats array has a 2-byte stride (the dump loads it with `lhzx r,base,i*2`), so
    // muPlayCount is the sole member.
    struct Stats
    {
        Stats();            // :129 (own TU -- declared only)
        u16 muPlayCount;    // :130 (+0x00)
    };

    // :47 default ctor (own TU -- declared only).
    SpliceBankStatistics();

    // :58 -- (spliceBank, spliceContent, index). Allocate + zero the per-splice Stats
    // bank and register onto spHead. Bodied in CgsSpliceBankStatistics.cpp.
    SpliceBankStatistics(const SpliceManager::SpliceContainer* lpSpliceBank,
                         const SplicerContent* lpSpliceContent, u32 luIndex);

    // :81. Free the Stats bank through the SpliceManager heap and unlink from spHead.
    ~SpliceBankStatistics();

    // :112 -- bump the play count for a splice index (own TU -- declared only).
    void DoPlay(u16 lu16SpliceIndex);

    // CgsSplicerContent.cpp:185. Dump this bank's name + per-splice play counts to the
    // debug TTY (gated on the message filter). :122 DWARF -- const.
    void DumpToTty() const;

    // CgsSplicerContent.cpp:203. Walk the spHead list, dumping each bank. :125 DWARF --
    // static (no `this`; reads spHead directly).
    static void DumpAllToTty();

    const SplicerContent*                 mpSpliceContent; // :135 (+0x00)
    const SpliceManager::SpliceContainer* mpSpliceBank;    // :136 (+0x04)
    Stats*                                mpaStats;        // :137 (+0x08) per-splice bank
    u32                                   muStatCount;     // :138 (+0x0C)
    SpliceBankStatistics*                 mpNext;          // :139 (+0x10) intrusive spHead link
    u32                                   muIndex;         // :140 (+0x14)

    // :133. The intrusive registration list head (X360 global). Defined in the .cpp.
    static SpliceBankStatistics* spHead;
};

} // namespace Playback
} // namespace CgsSound

// The global SpliceManager instance the bank routes allocations through (X360
// off_82FFB9F0). Declared here; the SpliceManager type is SpliceManager.h.
extern SpliceManager* gpSpliceManager;

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICEBANKSTATISTICS_H
