#pragma once

// =============================================================================
// BrnTrafficCarStreamer.h  --  BrnTraffic::TrafficCarStreamer
//
// The traffic system's streamed vehicle-GRAPHICS table: a BrnWorld::BaseStreamer
// over the 64-entry traffic vehicle-asset catalogue. It owns, per asset, the
// CgsID the resource system streams by, the load flags (requested / bonus), the
// load state, a 64-frame rendering history, and the loaded
// CgsResource::ResourcePtr<BrnTraffic::GraphicsStub> the renderer draws from.
//
// The canonical path is baked into this class's own X360 assert strings
// (BURNOUT_X360_ARTIST.XEX):
//   "...code\\gamesource\\world\\entitymodules\\trafficentitymodule\\BrnTrafficCarStreamer.h"
//   "...code\\gamesource\\unity\\../World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.cpp"
//
// SHAPE SOURCES (the asm arbitrates):
//
// 1. The leaked Feb-2007 BrnTrafficCarStreamer.h/.cpp carry this class in full: member list,
//    member order, the two enums, both constants, and every body.
// 2. The X360 ARTIST asm confirms that member order byte for byte. progress/identity.json holds
//    fourteen TrafficCarStreamer symbols, and every one addresses the members at fixed
//    displacements off `this` that tile over the leak's declaration order with no slack:
//
//      console  size   member                         attested by
//      -------  -----  -----------------------------  -------------------------------
//        +9080  512    mauRenderingHistory[64] (u64)  Update @0x8274F740:
//                                                       v8 = this+9080; *v8 = *v8 >> 1;
//                                                       v8 += 1 per asset  (8-byte stride)
//        +9592   64    maxLoadFlags[64]        (u8)   AddVehiclesToTargetList @0x8274F6A0
//                                                       *(this+id+9592) |= 1
//                                                     GetBonusAssets @0x8274F8C0
//                                                       (*(this+i+9592) & 3) == 2
//        +9656   64    mauLoadStates[64]       (u8)   IsTrafficAssetLoaded @0x82706160
//                                                       return *(this+id+9656) == 2
//        +9720  512    maAssetIds[64]        (CgsID)  Update @0x8274F740:
//                                                       v14 = this+9720; AddEntry(*(v14+4)…);
//                                                       v14 += 8            (8-byte stride)
//       +10232 2048    maGraphicsStubs[64]            OnLoadBegin @0x82757E40 etc:
//                        (ResourcePtr<GraphicsStub>)    32*asset + this + 10232
//                                                       (32 == console sizeof BaseResourcePtr)
//       +12280    4    muNumAssets             (u32)  every body: `v4[3070]` / *(this+12280)
//
//    9080+512 == 9592, +64 == 9656, +64 == 9720, +512 == 10232, +2048 == 12280. The
//    base BrnWorld::BaseStreamer<64> therefore occupies [0, 9080) on the console.
// 3. The DecFIGS DWARF entry at references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
//    TrafficEntityModule/BrnTrafficCarStreamer.h declares the whole class as
//    `struct BrnTraffic::TrafficCarStreamer : public BrnWorld::BaseStreamer<64>` and
//    corroborates the base class and its template argument, all six members in this order,
//    both private constants, the six protected virtuals in the order used below, and the
//    trailing `const` on the four query methods. QueryLoad / QueryUnload are declared there
//    but absent from the X360 ledger, which is why overriding them is recovery, not invention.
//
//    THREE DWARF-DECLARED METHODS ARE DELIBERATELY NOT DECLARED HERE:
//      ClearRenderingHistory()   (DWARF .h:216)
//      GetAssetID( u32 )         (DWARF .h:346)
//    Neither appears among the fourteen TrafficCarStreamer symbols in progress/identity.json,
//    so under "DWARF supplies names/types; the X360 ledger decides what exists" they are
//    PS3-side-only as far as we can attest. Named here so a later reader knows they were
//    considered and gated out. Adding one, if it turns up attested, is a pure addition.
//
// HOST LAYOUT: the console displacements above are provenance, not targets. On x64 the base is
// wider (8-byte pointers in InternalBaseStreamer and its queues) and each
// ResourcePtr<GraphicsStub> is 40 bytes rather than 32, so the absolute offsets move.
// Everything is accessed by name, and _AssertLayout() pins only the pointer-invariant facts:
// the three pointer-free runs and their order.
//
// SHIP-vs-LEAK DIVERGENCES (the asm wins; each is marked at its member or method):
//   * Update gained two parameters, `( const u8* lpauOverrideBonusAssets, u32
//     luNumBonusAssets )`. The leak's Update() takes none and derives the bonus set from the
//     rendering history.
//   * GetBonusAssets @0x8274F8C0 is ship-only, and is the producer that feeds that override
//     list back in through the new parameters.
//   * The constant is KU_MAX_BONUS_STREAMED_ASSETS in ship (the assert strings say so); the
//     leak calls it KU_MAX_BONUS_ASSETS.
//   * IsTrafficAssetLoaded gained a leading `luAssetId < KU_MAX_VEHICLE_ASSETS` assert.
//   * The base Construct gained the `lbSlotPoolSystem` parameter (see Construct).
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                              // CgsID
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"       // CgsResource::ResourcePtr<T>, NULLResourcePtr
#include "GameSource/World/BrnBaseStreamer.h"                            // BrnWorld::BaseStreamer<N>, StreamerTargetEntry
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"                    // BrnResource::EAssetSet, MakeTrafficVehicleId
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"              // GameDataAssetEvent, GetTrafficVehicleGraphicsResponse, E_POOL_TRAFFIC
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"             // BrnTraffic::KU_MAX_VEHICLE_ASSETS
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"                // BrnTraffic::VehicleAsset
#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"                // BrnTraffic::GraphicsStub + the two GraphicsSpec homes

#include <cstddef>   // offsetof (used only by the never-called _AssertLayout)

namespace BrnTraffic
{

class TrafficCarStreamer : public BrnWorld::BaseStreamer< KU_MAX_VEHICLE_ASSETS >
{
    typedef BrnWorld::BaseStreamer< KU_MAX_VEHICLE_ASSETS > BaseClass;

public:
    // ---- lifecycle ---------------------------------------------------------

    // @0x827539A0 -- EXPORT HOLE (no .ida-exports JSON for this address). Body
    // from the leak, with the base call adapted to the ship's 4-parameter
    // BaseStreamer<N>::Construct. See the .cpp for the full FLAG.
    void Construct();

    // @0x8274F690 -- `*(this + 12280) = 0`, i.e. muNumAssets = 0. Nothing else.
    void Destruct();

    // ---- the asset catalogue ----------------------------------------------

    // @0x82753A38. Publish TrafficData's vehicle-asset list into the streamer. Nothing
    // requests a traffic bundle until this has run.
    void SetAssetList( u32 luNumAssets, const VehicleAsset* lpaAssets );

    // Leaked inline (BrnTrafficCarStreamer.h:187). Drop every "should be loaded"
    // flag, keeping the load STATES untouched -- the next Update() then unloads
    // whatever nothing re-requested. Not separately exported (inlined into its
    // callers on the X360).
    inline void ClearAssetList();

    // @0x8274F6A0. Mark the assets a hull needs as requested this frame:
    // `maxLoadFlags[lpauVehicleAssetIds[v]] |= E_LOADFLAG_REQUESTED`.
    void AddVehiclesToTargetList( u32 luNumVehicles, const u8* lpauVehicleAssetIds );

    // @0x8274F740. Rebuild the base streamer's target list from the flags and pump the base.
    // Ship signature, from the asm (r4 = list pointer, r5 = count): the sole call site,
    // TrafficEntityModule::UpdateStreaming @0x82748848, passes `Update( v4, v12, v6 )` with v12
    // either 0 or a byte list and v6 its length. The leak's Update() has no parameters.
    void Update( const u8* lpauOverrideBonusAssets = 0, u32 luNumBonusAssets = 0 );

    // @0x8274F8C0, ship-only. Collect the indices of the assets currently flagged BONUS into
    // lpauOutBonusAssets as bytes and write the count to *lpuOutNumBonusAssets. The out array
    // must hold at least KU_MAX_BONUS_STREAMED_ASSETS entries.
    void GetBonusAssets( u8* lpauOutBonusAssets, u32* lpuOutNumBonusAssets ) const;

    // ---- queries (leaked inlines; the X360 emitted these three out-of-line) --

    // @0x82706160.
    inline bool IsTrafficAssetLoaded( u32 luAssetId ) const;

    // @0x82706288.
    inline bool AreAllAssetsLoaded() const;

    // DWARF .h:275. No standalone X360 symbol, because the compiler inlined its only call site:
    // TrafficEntityModule::UpdateStreaming @0x82748848's meEmptyTrafficPoolState == 1 arm
    // carries the loop at 0x827488D0..0x827488FC (walk mauLoadStates bounded by muNumAssets,
    // bail false on the first non-zero, return true on an empty catalogue). The DecFIGS scope
    // tree for that same function (_compile/BrnTrafficUnity.cpp:15427) names it in the call list.
    // Note the console's asymmetry with AreAllAssetsLoaded: this one tests the state bytes for
    // every known asset with no maxLoadFlags filter, so it means "nothing is resident", not
    // "nothing that was wanted is resident".
    inline bool AreAllAssetsUnloaded() const;

    // @0x8271D440 -- returns `*ResourcePtr<GraphicsStub>::operator*()` slot 0.
    inline const BrnVehicle::GraphicsSpec* GetGraphicsSpec( u32 luAssetId ) const;

    // @0x8271D678 -- the same for slot 1.
    inline const BrnWheel::GraphicsSpec* GetWheelGraphicsSpec( u32 luAssetId ) const;

    // @0x82706300 -- EXPORT HOLE. Body from the leak; corroborated by Update's
    // own `history >>= 1` ageing of the same word (see the .cpp).
    inline void NotifyAssetRenderedThisFrame( u32 luAssetId );

    // Never called. Pins ONLY what is true on both platforms: the three
    // pointer-free member runs and the whole member ORDER. Deliberately does NOT
    // pin any absolute console offset -- the base and the ResourcePtr array are
    // both wider on x64, so the console displacements in the banner cannot and
    // must not hold here.
    static void _AssertLayout();

protected:
    // The six BrnWorld::InternalBaseStreamer pure virtuals, in the DWARF's order
    // (dwarfdump BrnTrafficCarStreamer.h, protected block). NOTE: QueryLoad /
    // QueryUnload are NOT in the X360 ledger for this class -- both leaked bodies
    // are a bare `return 0`, which the console's ICF folds onto a shared thunk, so
    // no per-class symbol survives to export. The DecFIGS DWARF declares both as
    // members of this class anyway, which is what makes overriding them a RECOVERY
    // rather than an invention. They must be overridden here regardless: without
    // them TrafficCarStreamer stays abstract, and TrafficEntityModule embeds one
    // BY VALUE (BrnTrafficEntityModule.h `TrafficCarStreamer mStreamer`).
    virtual s32  QueryLoad( const BrnWorld::StreamerTargetEntry* lpPotentialList,
                            s32 liPotentialListLength );
    virtual s32  QueryUnload( const BrnWorld::StreamerTargetEntry* lpPotentialList,
                              s32 liPotentialListLength );
    virtual void OnLoadBegin( s32 liListIndex );        // @0x82757E40
    virtual void OnUnloadBegin( s32 liListIndex );      // @0x82758008
    virtual void OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent,
                                 s32 liListIndex );     // @0x82758270
    virtual void OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent,
                                   s32 liListIndex );   // @0x82753F08

private:
    // Leak BrnTrafficCarStreamer.h:141. The low two bits of maxLoadFlags -- the
    // console only ever tests them as `& 3`, `& 1` and `| 2`, which is exactly
    // this pair (AddVehiclesToTargetList ors 1; Update ors 2; AreAllAssetsLoaded,
    // GetBonusAssets and OnUnloadBegin test the pair).
    enum ELoadFlags
    {
        E_LOADFLAG_NONE             = 0,
        E_LOADFLAG_REQUESTED        = 0x01,
        E_LOADFLAG_BONUS            = 0x02,
        E_LOADFLAG_SHOULD_BE_LOADED = E_LOADFLAG_REQUESTED | E_LOADFLAG_BONUS,
    };

    // Leak :150. The four values the console's mauLoadStates byte takes, each
    // pinned by an asm literal: OnLoadBegin asserts ==0 then stores 1;
    // OnLoadComplete asserts ==1 then stores 2; IsTrafficAssetLoaded returns
    // (==2); OnUnloadBegin asserts ==2 then stores 3; OnUnloadComplete asserts
    // ==3 then stores 0.
    enum ELoadState
    {
        E_LOADSTATE_NOT_LOADED     = 0,
        E_LOADSTATE_LOAD_STARTED   = 1,
        E_LOADSTATE_LOADED         = 2,
        E_LOADSTATE_UNLOAD_STARTED = 3,

        E_LOADSTATE_COUNT          = 4,
    };

    // Leak :160. The top bit of the 64-bit per-asset rendering history; Update
    // shifts the whole word right by one each frame, so the word holds "was this
    // asset drawn in each of the last 64 frames".
    static const u64 KU_ASSET_RENDERED_FLAG = 1ull << 63;

    // Ship spelling, baked verbatim into two X360 assert strings:
    //   "luNumBonusAssets <= KU_MAX_BONUS_STREAMED_ASSETS"  (Update, cpp:195)
    //   "luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS"  (GetBonusAssets, cpp:277)
    // Both compare against the literal 2 (`cmplwi r30,2` @0x8274F958).
    static const u32 KU_MAX_BONUS_STREAMED_ASSETS = 2;

    typedef CgsResource::ResourcePtr< GraphicsStub > GraphicsStubPtr;

    // ---- members, in the console's proven order (see the banner) ------------
    u64             mauRenderingHistory[KU_MAX_VEHICLE_ASSETS];   // console +9080
    u8              maxLoadFlags[KU_MAX_VEHICLE_ASSETS];          // console +9592
    u8              mauLoadStates[KU_MAX_VEHICLE_ASSETS];         // console +9656
    CgsID           maAssetIds[KU_MAX_VEHICLE_ASSETS];            // console +9720
    GraphicsStubPtr maGraphicsStubs[KU_MAX_VEHICLE_ASSETS];       // console +10232
    u32             muNumAssets;                                  // console +12280
};


// -----------------------------------------------------------------------------
// Leaked inline (BrnTrafficCarStreamer.h:187). Clears both "should be loaded"
// bits for every KNOWN asset (bounded by muNumAssets, not by the capacity --
// the console's loop bound everywhere in this class).
// -----------------------------------------------------------------------------
inline void
TrafficCarStreamer::ClearAssetList()
{
    for( u32 luAsset = 0; luAsset < muNumAssets; luAsset++ )
    {
        maxLoadFlags[luAsset] &= static_cast< u8 >( ~E_LOADFLAG_SHOULD_BE_LOADED );
    }
}


// -----------------------------------------------------------------------------
// @0x82706160. An asset is loaded exactly when its state byte is E_LOADSTATE_LOADED.
// The first assert is ship-only (`cmplwi r28,0x40` @0x82706178, baked line 244); the
// leak has only the second (baked line 245), which streamed the id and muNumAssets
// into the message buffer where the house CGS_ASSERT carries static text.
// -----------------------------------------------------------------------------
inline bool
TrafficCarStreamer::IsTrafficAssetLoaded( u32 luAssetId ) const
{
    CGS_ASSERT( luAssetId < KU_MAX_VEHICLE_ASSETS, "luAssetId < KU_MAX_VEHICLE_ASSETS" );
    CGS_ASSERT( luAssetId < muNumAssets,
                "Attempted to use a traffic car asset that is not known about" );

    return ( mauLoadStates[luAssetId] == E_LOADSTATE_LOADED );
}


// -----------------------------------------------------------------------------
// Inlined into TrafficEntityModule::UpdateStreaming @0x82748848 (0x827488D0..0x827488FC);
// DWARF .h:275. "Nothing is resident": every known asset's state byte is
// E_LOADSTATE_NOT_LOADED. Unlike AreAllAssetsLoaded below there is no maxLoadFlags
// filter; the console's inlined loop indexes mauLoadStates directly. An empty
// catalogue is trivially all-unloaded, the console's own `beq` past the loop.
// -----------------------------------------------------------------------------
inline bool
TrafficCarStreamer::AreAllAssetsUnloaded() const
{
    for( u32 luAsset = 0; luAsset < muNumAssets; luAsset++ )
    {
        if( mauLoadStates[luAsset] != E_LOADSTATE_NOT_LOADED )
        {
            return false;
        }
    }

    return true;
}


// -----------------------------------------------------------------------------
// @0x82706288. Every asset that should be loaded is loaded. Slots with no request
// bit set are skipped (the console's `(flags & 3) == 0` continue).
// -----------------------------------------------------------------------------
inline bool
TrafficCarStreamer::AreAllAssetsLoaded() const
{
    for( u32 luAsset = 0; luAsset < muNumAssets; luAsset++ )
    {
        if( maxLoadFlags[luAsset] & E_LOADFLAG_SHOULD_BE_LOADED )
        {
            if( !IsTrafficAssetLoaded( luAsset ) )
            {
                return false;
            }
        }
    }

    return true;
}


// -----------------------------------------------------------------------------
// @0x8271D440. Three asserts (baked lines 309/310/311), then the stub's first import
// slot: the X360 tail is `*GraphicsStub::operator*( 32*asset + this + 10232 )` then
// slot 0, i.e. maGraphicsStubs[luAssetId]->mpVehicleGraphics by name.
// -----------------------------------------------------------------------------
inline const BrnVehicle::GraphicsSpec*
TrafficCarStreamer::GetGraphicsSpec( u32 luAssetId ) const
{
    CGS_ASSERT( luAssetId < muNumAssets,
                "Attempted to use a traffic car asset that is not known about" );
    CGS_ASSERT( IsTrafficAssetLoaded( luAssetId ),
                "Attempted to get main graphics for a traffic asset which isn't loaded" );
    CGS_ASSERT( !CgsResource::NULLResourcePtr.IsEqual( &maGraphicsStubs[luAssetId] ),
                "Resource ptr for a loaded traffic asset is NULL" );

    return maGraphicsStubs[luAssetId]->mpVehicleGraphics;
}


// -----------------------------------------------------------------------------
// @0x8271D678. The wheel twin of GetGraphicsSpec (baked lines 328/329/330).
// -----------------------------------------------------------------------------
inline const BrnWheel::GraphicsSpec*
TrafficCarStreamer::GetWheelGraphicsSpec( u32 luAssetId ) const
{
    CGS_ASSERT( luAssetId < muNumAssets,
                "Attempted to use a traffic car asset that is not known about" );
    CGS_ASSERT( IsTrafficAssetLoaded( luAssetId ),
                "Attempted to get wheel graphics for a traffic asset which isn't loaded" );
    CGS_ASSERT( !CgsResource::NULLResourcePtr.IsEqual( &maGraphicsStubs[luAssetId] ),
                "Resource ptr for a loaded traffic asset is NULL" );

    return maGraphicsStubs[luAssetId]->mpWheelGraphics;
}


// -----------------------------------------------------------------------------
// @0x82706300 -- export hole (no .ida-exports JSON). Body from the leak
// (BrnTrafficCarStreamer.h:284), corroborated by the exported consumer: Update
// @0x8274F740 reads the same 64-bit word at this+9080+8*asset, shifts it right by one,
// and treats a non-zero result as "rendered recently, keep it resident as a bonus".
// A producer setting the top bit is what makes that ageing shift meaningful.
//
// The [T1-stream] latch lives in the .cpp beside the four load hooks; this inline stays
// free of it because RenderTrafficCar calls it once per drawn car per frame.
// -----------------------------------------------------------------------------
inline void
TrafficCarStreamer::NotifyAssetRenderedThisFrame( u32 luAssetId )
{
    CGS_ASSERT( luAssetId < muNumAssets,
                "Attempted to use a traffic car asset that is not known about" );

    mauRenderingHistory[luAssetId] |= KU_ASSET_RENDERED_FLAG;
}

}   // namespace BrnTraffic
