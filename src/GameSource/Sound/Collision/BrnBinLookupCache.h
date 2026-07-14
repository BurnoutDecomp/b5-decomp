#ifndef BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H
#define BRN_SOUND_LOGIC_COLLISION_BIN_LOOKUP_CACHE_H

#include "types.hpp"
#include "GameSource/AttribSys/Generated/classes/propscrashbinlist.h" // Attrib::Gen::propscrashbinlist (the Build List type)

// AttribSys-generated crash-bin container (Build's Bin template argument). The Build leaf
// only needs the class-key COMPILE-TIME CONSTANT baked from this type (hardcoded, attested
// in the asm) and never forms a propscrashbin object, so a forward declaration suffices --
// and no propscrashbin ctor is fabricated (none is attested in the X360 ledger; only
// CrashBinUtils<propscrashbin>::GetSampleIds @0x8268FE90 is).
namespace Attrib { namespace Gen { class propscrashbin; } }

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

    // One cached crash bin: the two qwords Build copies out of the resolved bin's
    // attribute data area.
    // FLAG (un-homed propscrashbin schema): the field MEANINGS of the bin's data area at
    // +0x38 / +0x40 are not recoverable -- the propscrashbin generated schema is not homed
    // (no propscrashbin.h; its accessors are inlined away), and there is no DWARF for
    // BinLookupCache. Named by their attested source byte offset rather than fabricating a
    // meaning. mBinField40 gets bin[+0x40] (stored first, at entry+0x00); mBinField38 gets
    // bin[+0x38] (stored second, at entry+0x08), matching the X360 store order.
    struct Entry
    {
        u64 mBinField40; // +0x00  <- (bin data area)[+0x40]
        u64 mBinField38; // +0x08  <- (bin data area)[+0x38]
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
