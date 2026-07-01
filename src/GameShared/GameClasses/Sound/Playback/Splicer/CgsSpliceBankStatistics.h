#ifndef CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICEBANKSTATISTICS_H
#define CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICEBANKSTATISTICS_H

#include "types.hpp"

struct SpliceManager;

// ============================================================================
// CgsSound::Playback::SpliceBankStatistics  (X360 ARTIST home).
//
// Per-source splice bank bookkeeping: on construction (with a source) it allocates a
// per-entry u16 bank through the global SpliceManager, zero-fills it, and prepends
// itself onto an intrusive list (spHead / mpNext). On destruction it frees the bank
// through the SpliceManager heap and unlinks. No DWARF/leak source -- member names are
// speculative, recovered from the X360 ctor (@0x826D57E8) + dtor (@0x826A1158) stores.
//
// Layout (X360 word offsets; host-width FLAG -- pointers widen, pinned BY NAME):
//   +0x00 miTag   +0x04 mpSource   +0x08 mpBuffer   +0x0C muCount
//   +0x10 mpNext  +0x14 mHandle
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// The source descriptor whose per-entry count the bank is sized from. Only its
// muEntryCount @ +0x08 is load-bearing (*(mpSource + 8)); the rest is opaque.
struct SourceDescriptor
{
    u8  mau8Opaque[8];   // +0x00
    u32 muEntryCount;    // +0x08
};

struct SpliceBankStatistics
{
    SpliceBankStatistics(SourceDescriptor* lprSource, int liTag, int liHandle);
    ~SpliceBankStatistics();

    int               miTag;      // +0x00
    SourceDescriptor* mpSource;   // +0x04
    u16*              mpBuffer;   // +0x08  SpliceManager-allocated per-entry bank
    u32               muCount;    // +0x0C
    SpliceBankStatistics* mpNext; // +0x10  intrusive spHead link
    int               mHandle;    // +0x14

    // The intrusive registration list head (X360 global). Defined in the .cpp.
    static SpliceBankStatistics* spHead;
};

} // namespace Playback
} // namespace CgsSound

// The global SpliceManager instance the bank routes allocations through (X360
// off_82FFB9F0). Declared here; the SpliceManager type is SpliceManager.h.
extern SpliceManager* gpSpliceManager;

#endif // CGS_SOUND_PLAYBACK_SPLICER_CGSSPLICEBANKSTATISTICS_H
