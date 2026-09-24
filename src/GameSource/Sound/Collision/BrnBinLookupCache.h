#ifndef BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H
#define BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT (GetEntry's bound)
#include "GameSource/AttribSys/Generated/classes/crashbinlist.h"      // Attrib::Gen::crashbinlist      (Build List, pipeline 0)
#include "GameSource/AttribSys/Generated/classes/crashbin.h"          // Attrib::Gen::crashbin          (Build Bin,  pipeline 0)
#include "GameSource/AttribSys/Generated/classes/propscrashbinlist.h" // Attrib::Gen::propscrashbinlist (Build List, pipeline 1)
#include "GameSource/AttribSys/Generated/classes/propscrashbin.h"     // Attrib::Gen::propscrashbin     (Build Bin,  pipeline 1)

// =============================================================================
// BrnSound::Logic::Collision::BinLookupCache
//   GameSource/Sound/Collision/BrnBinLookupCache.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//   BinLookupCache::Build<Attrib::Gen::crashbinlist, Attrib::Gen::crashbin>           @ 0x826A85F8
//   BinLookupCache::Build<Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin> @ 0x826A8710
//
// The material pre-filter of the collision-audio bin selection. CollisionStateManager owns
// one per pipeline (DWARF BrnCollisionStateManager.h:787 `BinLookupCache[2] maBinLoopupCache`,
// the console's own spelling): ResourcesAreReady @0x826D3788 builds [0] from the crash-bin list
// and [1] from the props crash-bin list, and SelectBin<List, Bin> (@0x826A97E8 / @0x826A8828)
// walks the entries to find the next bin whose material pair matches a collision before it
// resolves that bin's attributes. The DWARF declares the type in BrnCollisionStateManager.h
// (:232..:276 -- the asserts' file string is that header); it is its own ledger TU here, so it
// keeps its own files.
//
// SHAPE (DWARF BrnCollisionStateManager.h:232..:276, gated on the X360 bodies):
//   KU_CACHE_SIZE = 64          (:234; Build's assert `blt` against 0x40)
//   CacheEntry { mx64MaterialA; mx64MaterialB; }   (:239..:241; 16-byte stride, `std -8(r30)` /
//                                                   `std 0(r30)` with r30 = cache+0x10 per entry)
//   BinLookupCache()            (:236; the owner's ctor stores 0 to +0x98 / +0x4A0 = muEntryCount)
//   Build<List, Bin>(const List&)  a PUBLIC NON-STATIC member returning void: the mangled
//                               `...@BinLookupCache@Collision@Logic@BrnSound@@QAAXABV...`, r3 = the
//                               cache (ResourcesAreReady passes &maBinLoopupCache[i]), r4 = the list
//   GetEntry(u32) const         (:262; its bound assert "lu32Index < muEntryCount" fires from
//                               BrnCollisionStateManager.h line 252 inside SelectBin)
//   GetEntryCount() const       (:269)
//   muEntryCount (+0x00), maCacheEntry[64] (+0x08)  (:275 / :276)
// Members are pinned BY NAME; the X360 size 0x408 (the SelectBin `mulli 0x408` stride) is
// documentation, not a host static_assert.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

class BinLookupCache
{
public:
    // DWARF :234. Build's assert `(uint32_t)lList.mNumCrashBins() < KU_CACHE_SIZE` (blt 0x40).
    static const u32 KU_CACHE_SIZE = 64u;

    // DWARF :239..:241 -- the material pair a crash bin matches on, copied out of the bin's
    // layout block (+0x40 -> A, +0x38 -> B).
    struct CacheEntry
    {
        u64 mx64MaterialA;   // +0x00
        u64 mx64MaterialB;   // +0x08
    };

    // DWARF :236. The owner's ctor @0x826FFB00 / 0x826FFB04 stores 0 into both caches' count;
    // the entries are left as they are.
    BinLookupCache() : muEntryCount(0) {}

    // Fill the cache from a crash-bin list: entry i is the material pair of the list's i-th bin,
    // and the count is written last. Bodies and the two instantiations in BrnBinLookupCache.cpp.
    template< typename List, typename Bin >
    void Build( const List& lrList );

    // DWARF :262. SelectBin inlines it with the bound assert at BrnCollisionStateManager.h:252
    // (0x826A9A6C..0x826A9A8C, li r5,0xFC) -- non-gating, the read follows either way.
    const CacheEntry& GetEntry( u32 lu32Index ) const
    {
        CGS_ASSERT( lu32Index < muEntryCount, "lu32Index < muEntryCount" );
        return maCacheEntry[lu32Index];
    }

    // DWARF :269.
    u32 GetEntryCount() const { return muEntryCount; }

private:
    u32        muEntryCount;                 // +0x00  DWARF :275
    CacheEntry maCacheEntry[KU_CACHE_SIZE];  // +0x08  DWARF :276
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H
