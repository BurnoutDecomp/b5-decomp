#include "GameSource/Sound/Collision/BrnBinLookupCache.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"            // Attrib::FindCollection
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h" // Attrib::Instance / Collection / DefaultDataArea
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Collision::BinLookupCache::Build<List, Bin>
//   <Attrib::Gen::crashbinlist,      Attrib::Gen::crashbin>       @ 0x826A85F8 (export hole: ppcdis)
//   <Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin>  @ 0x826A8710
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The two
// instantiations are the same 72 instructions; they differ ONLY in the class key staged for
// Attrib::FindCollection (0x3DFA53FA_FE5BD9D7 crashbin @0x826A867C..0x826A8690,
// 0x4154BD6D_E9FF326C propscrashbin @0x826A8794..0x826A87A8), i.e. Bin::ClassKey().
//
//   0x826A8604  mr r27, r4 ; mr r26, r3                  ; the list, THIS cache (a member: r3)
//   0x826A8614  cmplwi mNumCrashBins, 0x40 ; blt          ; assert (h:237, li r5,0xED)
//   loop i < lList.mNumCrashBins()  (re-read every pass, 0x826A86DC..0x826A86EC):
//     ref = i < Private::GetLength(list layout) ? list layout + 8 + 24*i
//                                                : DefaultDataArea(0x18)   ; the null RefSpec
//     Attrib::Instance lBin( Attrib::FindCollection(Bin::ClassKey(), ref+8), 0 )
//     if (!lBin layout) layout = DefaultDataArea(0x190)                   ; Bin::KU_LAYOUT_SIZE
//     maCacheEntry[i] = { layout[+0x40], layout[+0x38] }                  ; std -8(r30) / 0(r30)
//   muEntryCount = lList.mNumCrashBins()                  ; 0x826A8700, stored LAST
//
// The resolve is the plain Attrib::FindCollection (0x82808378), NOT FindCollectionWithDefault:
// a key with no collection yields a zeroed default area, whose zero material pair never
// matches a collision. The generated Bin's own (Collection*) ctor is not what the console
// calls here (no class check between FindCollection and Instance::Instance), so the Instance
// is formed directly, as the asm does. The two qword reads use the Bin's named layout offsets
// (DATA-format -- they do not widen on the host).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

template< typename List, typename Bin >
void BinLookupCache::Build( const List& lrList )
{
    CGS_ASSERT( lrList.mNumCrashBins() < KU_CACHE_SIZE,
                "(uint32_t)lList.mNumCrashBins() < KU_CACHE_SIZE" );

    for ( u32 i = 0; i < lrList.mNumCrashBins(); ++i )
    {
        // The i-th crash-bin RefSpec (out of range -> the shared null RefSpec); its collection
        // key is the qword at +8.
        const void* lpRef = lrList.GetCrashBinRefData( i );
        const u64 luCollectionKey =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpRef ) + 8 );

        Attrib::Instance lBin( Attrib::FindCollection( Bin::ClassKey(), luCollectionKey ), 0 );
        const void* lpBinData = lBin.GetLayoutPointer();
        if ( !lpBinData )
            lpBinData = Attrib::DefaultDataArea( Bin::KU_LAYOUT_SIZE );

        maCacheEntry[i].mx64MaterialA =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpBinData ) + Bin::KU_OFFSET_MATERIAL_A );
        maCacheEntry[i].mx64MaterialB =
            *reinterpret_cast<const u64*>( reinterpret_cast<const u8*>( lpBinData ) + Bin::KU_OFFSET_MATERIAL_B );
    }

    muEntryCount = lrList.mNumCrashBins();
}

// The two specialisations the X360 build emits -- one per collision pipeline
// (InputCollision::E_REGULAR / E_PROP), both built by CollisionStateManager::ResourcesAreReady.
template void BinLookupCache::Build< Attrib::Gen::crashbinlist, Attrib::Gen::crashbin >(
    const Attrib::Gen::crashbinlist& );                                              // @ 0x826A85F8
template void BinLookupCache::Build< Attrib::Gen::propscrashbinlist, Attrib::Gen::propscrashbin >(
    const Attrib::Gen::propscrashbinlist& );                                         // @ 0x826A8710

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
