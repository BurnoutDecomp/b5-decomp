#ifndef BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H
#define BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H

#include "types.hpp"
#include "GameSource/AttribSys/Generated/classes/propscrashbinlist.h" // Attrib::Gen::propscrashbinlist (the Build List type)
#include "GameSource/AttribSys/Generated/classes/propscrashbin.h"     // Attrib::Gen::propscrashbin (the Build Bin type: ClassKey / KU_LAYOUT_SIZE / material offsets)

// =============================================================================
// BrnSound::Logic::Collision::BinLookupCache
//   GameSource/Sound/Collision/BrnBinLookupCache.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//   BinLookupCache::Build<Attrib::Gen::propscrashbinlist,
//                         Attrib::Gen::propscrashbin>  @ 0x826A8710
//
// A small fixed-capacity lookup cache the collision-audio CollisionStateManager builds
// from a crash-bin list attribute: for each of the list's mNumCrashBins() crash bins it
// resolves the bin's AttribSys collection, then copies two descriptor qwords out of the
// bin's attribute data area into a cache entry. Home is the Sound/Collision/ dir that
// already hosts the sibling collision-audio classes (BrnCollisionStateManager,
// BrnCollisionDataStructures, BrnHingeStateCache). BinLookupCache is DECLARED in
// BrnCollisionStateManager.h:237 in the original (the assert rodata path proves it), but
// it is its own ledger TU, so it is homed in its own files here rather than editing the
// committed CollisionStateManager TU.
//
// LAYOUT (attested by the X360 Build leaf; members pinned BY NAME, host pointer widths
// differ so absolute offsets/size are NOT static_asserted):
//   +0x00  mNumBins   u32   (count stored last: `*result = lList.mNumCrashBins()`)
//   +0x08  maEntries[KU_CACHE_SIZE]   Entry (16 bytes each: two qwords copied from the bin)
// KU_CACHE_SIZE = 0x40 (the assert `(uint32_t)lList.mNumCrashBins() < KU_CACHE_SIZE`
// blt's against 0x40). The X360 stores entries at result+0x08 with a 16-byte stride
// (`std -8(r30)` / `std 0(r30)`, r30 = result+0x10 pre-incremented by 0x10 per bin).
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
    // Capacity gate the Build assert enforces (`... < KU_CACHE_SIZE`, blt cr6, 0x40).
    static const u32 KU_CACHE_SIZE = 0x40u;

    // One cached crash bin: the material pair Build copies out of the resolved bin's
    // layout block. RESOLVED 2026-08-18 (was "FLAG (un-homed propscrashbin schema)"): the
    // propscrashbin schema is now homed (propscrashbin.h, DWARF _LayoutStruct walk pinned
    // by this very leaf's `ld 0x40` / `ld 0x38`): bin[+0x40] is mMaterialA, bin[+0x38] is
    // mMaterialB (UInt64 pair, DWARF propscrashbin.h:163/:170). mMaterialA is stored first
    // (entry+0x00), mMaterialB second (entry+0x08) -- the X360 store order.
    struct Entry
    {
        u64 mMaterialA; // +0x00  <- propscrashbin layout +0x40
        u64 mMaterialB; // +0x08  <- propscrashbin layout +0x38
    };

    // Build the cache from a crash-bin list attribute. STATIC (the X360 leaf takes no
    // `this`: r3 is the NRVO return slot for the by-value BinLookupCache, r4 is the list).
    // Template on <List, Bin>; only <propscrashbinlist, propscrashbin> is instantiated in
    // the X360 build (the mangled name), matching the sibling CrashBinUtils<Bin> shape.
    template< typename List, typename Bin >
    static BinLookupCache Build( const List& lrList );

    u32   mNumBins;                 // +0x00
    u8    mPad0[4];                 // +0x04 (qword-align the entry array)
    Entry maEntries[KU_CACHE_SIZE]; // +0x08
};

} // namespace Collision
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H
