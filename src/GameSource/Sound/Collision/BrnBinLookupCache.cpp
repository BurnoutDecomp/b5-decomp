#include "GameSource/Sound/Collision/BrnBinLookupCache.h"
#include "GameSource/AttribSys/Generated/classes/propscrashbinlist.h"        // Attrib::Gen::propscrashbinlist
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"            // Attrib::FindCollection
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h" // Attrib::Instance / Collection / DefaultDataArea
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Collision::BinLookupCache::Build<List, Bin>  @ 0x826A8710
//   (instantiated <Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//
// X360 leaf (de-optimised to human C++ below):
//   result->mNumBins slot is filled last; the body walks the list's crash bins:
//     CGS_ASSERT( lList.mNumCrashBins() < KU_CACHE_SIZE );        ; blt cr6, 0x40
//     for ( i = 0; i < lList.mNumCrashBins(); ++i ) {
//         ref  = (i < Private::GetLength(dataArea)) ? &refArray[i]
//                                                   : DefaultDataArea(0x18);  ; null-RefSpec
//         key  = *(u64*)(ref + 8);                                ; RefSpec collection key
//         coll = Attrib::FindCollection( 0x4154BD6D_E9FF326C, key );          ; propscrashbin class
//         Attrib::Instance bin( coll, 0 );
//         binData = bin.mpAttributeData;
//         if ( !binData ) binData = DefaultDataArea(0x190);        ; propscrashbin data-area size
//         entry[i] = { binData[+0x40], binData[+0x38] };
//     }
//     result->mNumBins = lList.mNumCrashBins();
//
// The list-side reads (mNumCrashBins, the RefSpec array walk + OOB sentinel) are the
// inlined propscrashbinlist accessors (mNumCrashBins / GetCrashBinRefData, grown onto
// propscrashbinlist.h). The bin-side is the raw AttribSys resolve+read the X360 baked from
// Bin=propscrashbin at compile time.
//
// RESOLVED 2026-08-18 (was "FLAG (Bin=propscrashbin compile-time constants baked, not
// derived)"): the X360 template body derived the resolved-collection class key and the bin
// layout-block size from the Bin type at compile time. Bin (propscrashbin) is now homed in
// propscrashbin.h, so the two constants come from Bin::ClassKey() (0x4154BD6DE9FF326C, the
// doubleword this leaf stages @0x826A8794-A8) and Bin::KU_LAYOUT_SIZE (0x190, `li r3,0x190`
// @0x826A87CC), and the two qword reads use Bin's named material offsets. What is STILL not
// reproduced: the X360 forms a Bin over the collection (its (const Collection*, uint32_t)
// ctor); that ctor has no IDA export, so an Attrib::Instance stands in for the handle.
//
// RESOLVED 2026-07-31 (was: "FLAG (FindCollection owner arg)"). The X360 forwards
// *(RefSpec + 8) -- the RefSpec's 64-bit COLLECTION key -- in r4, and r4 is exactly what
// Attrib::FindCollection @0x82808378 uses as the collection key. This site had it right all
// along and was only obscured by the old `FindCollection(int, void*)` declaration, which
// forced the qword through a pointer parameter. With the corrected two-key signature the
// cast is gone and both keys are passed at their true widths (the class key as the whole
// doubleword 0x4154BD6D_E9FF326C, not just its low word).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

template< typename List, typename Bin >
BinLookupCache BinLookupCache::Build( const List& lrList )
{
    // Bin=propscrashbin compile-time constants the X360 baked into this instantiation,
    // now derived from the homed Bin type (propscrashbin.h).
    const u64 KU_PROPSCRASHBIN_CLASS_KEY = Bin::ClassKey();
    const u32 KU_PROPSCRASHBIN_DATA      = Bin::KU_LAYOUT_SIZE;

    BinLookupCache lCache; // entries left indeterminate past mNumBins, as in the asm

    CGS_ASSERT( lrList.mNumCrashBins() < KU_CACHE_SIZE,
                "(uint32_t)lList.mNumCrashBins() < KU_CACHE_SIZE" );

    const u32 luNumBins = lrList.mNumCrashBins();
    for ( u32 i = 0; i < luNumBins; ++i )
    {
        // i-th crash-bin RefSpec (OOB -> shared null-RefSpec sentinel).
        const void* lpRef = lrList.GetCrashBinRefData( i );

        // RefSpec collection key (qword @ +8) -> FindCollection's collection-key argument.
        const u64 luCollectionKey =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpRef ) + 8 );

        // Resolve the bin's propscrashbin collection.
        Attrib::Collection* lpCollection =
            Attrib::FindCollection( KU_PROPSCRASHBIN_CLASS_KEY, luCollectionKey );

        // Handle onto the resolved collection; fall back to a default data area if the
        // handle resolved without one (the X360 `if (!v11) v11 = DefaultDataArea(0x190)`).
        Attrib::Instance lBin( lpCollection, 0 );
        const void* lpBinData = lBin.GetLayoutPointer();
        if ( !lpBinData )
            lpBinData = Attrib::DefaultDataArea( KU_PROPSCRASHBIN_DATA );

        // Copy the bin's material pair into the cache entry (store order: +0x40 mMaterialA
        // into entry+0x00, then +0x38 mMaterialB into entry+0x08). DATA-format offsets
        // (they do not widen on the host).
        lCache.maEntries[i].mMaterialA =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpBinData ) + Bin::KU_OFFSET_MATERIAL_A );
        lCache.maEntries[i].mMaterialB =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpBinData ) + Bin::KU_OFFSET_MATERIAL_B );
    }

    lCache.mNumBins = lrList.mNumCrashBins();
    return lCache;
}

// Explicit instantiation -- the single crash-bin-cache specialisation the X360 build emits
// (@ 0x826A8710). Matches the mangled name
// BinLookupCache::Build<Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin>.
template BinLookupCache
BinLookupCache::Build< Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin >(
    const Attrib::Gen::propscrashbinlist& );

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
