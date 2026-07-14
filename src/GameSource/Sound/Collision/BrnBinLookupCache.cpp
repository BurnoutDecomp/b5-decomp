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
// FLAG (Bin=propscrashbin compile-time constants baked, not derived): the X360 template
// body derived the resolved-collection class key and the bin data-area size from the Bin
// type at compile time. Bin (propscrashbin) has no attested ctor / homed schema, so those
// two constants are reproduced here as the values the asm literally stages -- the class
// key 0x4154BD6DE9FF326C (low word 0xE9FF326C surfaced to FindCollection(int), matching the
// b4blurasset / languagestreamconfiguration key-staging convention) and the 0x190-byte data
// area (== crashbin's DefaultDataArea size) -- rather than fabricating a propscrashbin ctor.
//
// FLAG (FindCollection owner arg): the X360 forwards *(RefSpec + 8) (the RefSpec's 64-bit
// collection key) as FindCollection's second (owner) argument. The committed FindCollection
// takes `void* lpOwner`; the qword is reinterpret_cast to that pointer parameter to carry
// the same value the asm passes in r4. Its exact resolve semantics live in the AttribSys
// runtime (not homed); the observable effect -- a resolved propscrashbin Collection* handed
// to the Instance handle -- matches.
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
    // Bin is a compile-time type tag only (its class key / data-area size are the baked
    // constants below); reference it so the parameter is not flagged unused.
    (void)sizeof( Bin* );

    // Bin=propscrashbin compile-time constants the X360 baked into this instantiation.
    static const int KI_PROPSCRASHBIN_CLASS  = static_cast<int>( 0xE9FF326Cu ); // low word of 0x4154BD6DE9FF326C
    static const u32 KU_PROPSCRASHBIN_DATA   = 0x190u;

    BinLookupCache lCache; // entries left indeterminate past mNumBins, as in the asm

    CGS_ASSERT( lrList.mNumCrashBins() < KU_CACHE_SIZE,
                "(uint32_t)lList.mNumCrashBins() < KU_CACHE_SIZE" );

    const u32 luNumBins = lrList.mNumCrashBins();
    for ( u32 i = 0; i < luNumBins; ++i )
    {
        // i-th crash-bin RefSpec (OOB -> shared null-RefSpec sentinel).
        const void* lpRef = lrList.GetCrashBinRefData( i );

        // RefSpec collection key (qword @ +8) -> FindCollection owner argument.
        const u64 luOwnerKey =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpRef ) + 8 );

        // Resolve the bin's propscrashbin collection.
        Attrib::Collection* lpCollection =
            Attrib::FindCollection( KI_PROPSCRASHBIN_CLASS,
                                    reinterpret_cast<void*>( static_cast<u64>( luOwnerKey ) ) );

        // Handle onto the resolved collection; fall back to a default data area if the
        // handle resolved without one (the X360 `if (!v11) v11 = DefaultDataArea(0x190)`).
        Attrib::Instance lBin( lpCollection, 0 );
        const void* lpBinData = lBin.GetLayoutPointer();
        if ( !lpBinData )
            lpBinData = Attrib::DefaultDataArea( KU_PROPSCRASHBIN_DATA );

        // Copy the two attested descriptor qwords into the cache entry (store order:
        // +0x40 into entry+0x00, then +0x38 into entry+0x08).
        lCache.maEntries[i].mBinField40 =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpBinData ) + 0x40 );
        lCache.maEntries[i].mBinField38 =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpBinData ) + 0x38 );
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
