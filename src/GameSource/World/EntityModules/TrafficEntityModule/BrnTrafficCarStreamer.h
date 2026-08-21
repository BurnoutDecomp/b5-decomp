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
// ⭐⭐ 2026-08-21 (traffic wave T1, cluster C5) -- THIS HEADER WAS REPLACED, AND
// THE THING IT REPLACED WAS THE REASON NO TRAFFIC CAR EVER LOADED.
//
// The previous version was an honest but blind OFFSET-FIRST slice: a standalone
// (non-derived) class with `u8 mPad_0019[0x2578 - 0x19]`, four query methods, and
// no SetAssetList / Update / On*Complete at all. Because SetAssetList was not even
// DECLARED, TrafficEntityModule::LoadData's stage-1 leg could not be written, so
// `muNumAssets` stayed 0, so `Update()` added no entries, so **no VEH_T*_GR bundle
// was ever requested at any point in the boot** -- which is exactly what the boot
// log reported by name:
//   [Q7-traffic-leg] ... TrafficCarStreamer::SetAssetList(...) -- no declaration/
//   body in tree [FLAG PC partial gate]
// The padding was also a live instance of the tree's #1 recurring bug: `mPad_0019`
// reserved a CONSOLE byte span inside a HOST object whose base class is wider,
// so the "proven offsets" it was protecting could not hold on x64 anyway.
//
// ⭐ WHERE THE REAL SHAPE COMES FROM (three sources; the ASM arbitrates)
//
// 1. The leaked Feb-2007 BrnTrafficCarStreamer.h/.cpp contain this class in full --
//    member list, member ORDER, the two enums, both constants, and every body.
// 2. The X360 ARTIST asm confirms that member order byte for byte. progress/identity.json
//    holds FOURTEEN BrnTraffic::TrafficCarStreamer symbols (the thirteen bodies listed in
//    the .cpp banner, plus the compiler-generated constructor @0x827E3E98), and every one
//    of them addresses the members at fixed displacements off `this` -- displacements that
//    tile EXACTLY over the leak's declaration order with no slack anywhere:
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
// 3. ⚠ CORRECTED 2026-08-21 (C5 fix round). An earlier revision of this banner said
//    "there is NO DecFIGS DWARF entry for this class" and told the next reader not to
//    go looking. THAT WAS FALSE, and it was the worst kind of error to leave in a
//    banner: it instructed the next agent to skip rung 2 of the ladder. The entry is
//    at references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
//    TrafficEntityModule/BrnTrafficCarStreamer.h and it declares the whole class:
//
//      struct BrnTraffic::TrafficCarStreamer : public BrnWorld::BaseStreamer<64>
//
//    It independently corroborates -- it does not merely echo the leak -- every shape
//    decision this header makes:
//      * the BASE CLASS and its 64 template argument;
//      * all six members, with these names/types, IN THIS ORDER: mauRenderingHistory
//        (u64[64]), maxLoadFlags (u8[64]), mauLoadStates (u8[64]), maAssetIds
//        (CgsID[64]), maGraphicsStubs (ResourcePtr<BrnTraffic::GraphicsStub>[64]),
//        muNumAssets (u32) -- i.e. exactly the order the asm displacements tile over;
//      * both private constants (KU_ASSET_RENDERED_FLAG, KU_MAX_BONUS_ASSETS = 2);
//      * the six PROTECTED virtuals in the order used below, including QueryLoad /
//        QueryUnload, which the X360 ledger does not attest (see the .cpp banner) --
//        so the DWARF is the reason we know they are real members and not an
//        invention needed to de-abstract the class;
//      * GetGraphicsSpec / GetWheelGraphicsSpec returning `const GraphicsSpec*` and
//        being trailing-`const`; IsTrafficAssetLoaded / AreAllAssetsLoaded `const`.
//    So rung 2 agrees with rung 1 everywhere here. Where the DWARF and the ship asm
//    DO differ, the asm still wins and is flagged at the member/method (the
//    KU_MAX_BONUS_ASSETS -> KU_MAX_BONUS_STREAMED_ASSETS rename, and Update's two new
//    parameters, are both such cases -- see the divergence list below).
//
//    ⭐ THREE DWARF-DECLARED METHODS ARE DELIBERATELY NOT DECLARED HERE, and the
//    omission is stated rather than implied:
//      * AreAllAssetsUnloaded() const   (DWARF .h:275)  ⭐ RETIRED 2026-08-21 (wave T1
//        round 3): it IS attested after all -- see its declaration below. The other two
//        stand.
//      * ClearRenderingHistory()        (DWARF .h:216)
//      * GetAssetID( u32 )              (DWARF .h:346)
//    None of the three appears among the fourteen TrafficCarStreamer symbols in
//    progress/identity.json, so under AGENTS.md's "DWARF supplies names/types; the
//    X360 ledger decides what EXISTS" rule they are PS3-side-only as far as we can
//    attest and must stay out. They are named here so a later agent who finds them in
//    the dwarfdump knows they were considered and gated out, not missed. If a future
//    X360 export hole is filled and one of them turns up attested, adding it is a
//    pure addition -- none of them is load-bearing for any body below.
//
// ⭐ HOST LAYOUT: the console displacements above are PROVENANCE, not targets. On
// x64 the base is wider (8-byte pointers in InternalBaseStreamer + its queues) and
// each ResourcePtr<GraphicsStub> is 40 bytes rather than 32, so the absolute
// offsets legitimately move. Everything is accessed BY NAME. `_AssertLayout()`
// pins only the POINTER-INVARIANT facts -- the three pointer-free runs and their
// order -- exactly as AGENTS.md requires.
//
// ⭐ SHIP-vs-LEAK DIVERGENCES (the asm wins; each is marked at its member/method):
//   * `Update` gained TWO PARAMETERS: `( const u8* lpauOverrideBonusAssets, u32
//     luNumBonusAssets )`. The leak's Update() takes none and always derives the
//     bonus set from the rendering history. See Update's banner.
//   * `GetBonusAssets` @0x8274F8C0 is NEW in ship (absent from the leak) -- it is
//     the producer that feeds the override list back in through the new parameters.
//   * The constant is spelled KU_MAX_BONUS_STREAMED_ASSETS in ship (the assert
//     strings say so verbatim); the leak calls it KU_MAX_BONUS_ASSETS.
//   * `IsTrafficAssetLoaded` gained a leading `luAssetId < KU_MAX_VEHICLE_ASSETS`
//     assert the leak does not have.
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
#include "SharedClasses/Traffic/BrnTrafficGraphicsStub.h"                // BrnTraffic::GraphicsStub + the two GraphicsSpec homes (cluster C0)

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

    // @0x82753A38. THE function whose absence kept every traffic bundle
    // unrequested: publish TrafficData's vehicle-asset list into the streamer.
    void SetAssetList( u32 luNumAssets, const VehicleAsset* lpaAssets );

    // Leaked inline (BrnTrafficCarStreamer.h:187). Drop every "should be loaded"
    // flag, keeping the load STATES untouched -- the next Update() then unloads
    // whatever nothing re-requested. Not separately exported (inlined into its
    // callers on the X360).
    inline void ClearAssetList();

    // @0x8274F6A0. Mark the assets a hull needs as requested this frame:
    // `maxLoadFlags[lpauVehicleAssetIds[v]] |= E_LOADFLAG_REQUESTED`.
    void AddVehiclesToTargetList( u32 luNumVehicles, const u8* lpauVehicleAssetIds );

    // @0x8274F740. Rebuild the base streamer's target list from the flags and
    // pump the base. ⭐ SHIP SIGNATURE (asm: r4 = list pointer, r5 = count; the
    // sole call site, TrafficEntityModule::UpdateStreaming @0x82748848, passes
    // `TrafficCarStreamer::Update( v4, v12, v6 )` with v12 either 0 or a byte
    // list and v6 its length). The leak's Update() has no parameters.
    void Update( const u8* lpauOverrideBonusAssets = 0, u32 luNumBonusAssets = 0 );

    // @0x8274F8C0. SHIP-ONLY (not in the leak). Collect the indices of the assets
    // currently flagged BONUS into lpauOutBonusAssets (as bytes) and write the
    // count to *lpuOutNumBonusAssets. The out array must hold at least
    // KU_MAX_BONUS_STREAMED_ASSETS entries.
    void GetBonusAssets( u8* lpauOutBonusAssets, u32* lpuOutNumBonusAssets ) const;

    // ---- queries (leaked inlines; the X360 emitted these three out-of-line) --

    // @0x82706160.
    inline bool IsTrafficAssetLoaded( u32 luAssetId ) const;

    // @0x82706288.
    inline bool AreAllAssetsLoaded() const;

    // ⭐ DECLARED 2026-08-21 (wave T1 round 3, closure item 1). This is the DWARF .h:275
    // method the banner above listed as "deliberately NOT declared here ... PS3-side-only as
    // far as we can attest", with the standing instruction: "If a future X360 export hole is
    // filled and one of them turns up attested, adding it is a pure addition." IT IS NOW
    // ATTESTED, from the caller rather than from a symbol of its own:
    //   * TrafficEntityModule::UpdateStreaming @0x82748848, arm meEmptyTrafficPoolState == 1,
    //     inlines the loop at 0x827488D0..0x827488FC -- walk mauLoadStates (streamer +9656)
    //     bounded by muNumAssets, bail with FALSE on the first non-zero, return TRUE on an
    //     empty catalogue (`beq` straight to `li r11,1`);
    //   * the DecFIGS scope tree for that same function (_compile/BrnTrafficUnity.cpp:15427)
    //     names `TrafficCarStreamer::AreAllAssetsUnloaded` in its call list.
    // Two independent sources, same function, same loop. It has no standalone X360 symbol
    // because the compiler inlined its only call site -- which is precisely the case
    // AGENTS.md's "inlined / ICF-folded bodies" note covers.
    //
    // NOTE the asymmetry with AreAllAssetsLoaded above, and it is the CONSOLE's: this one
    // tests the STATE bytes for every KNOWN asset, with no maxLoadFlags filter at all. It
    // means "nothing is resident", not "nothing that was wanted is resident".
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
// @0x82706160. An asset is "loaded" exactly when its state byte is
// E_LOADSTATE_LOADED. The FIRST assert is ship-only (the leak has only the
// second): `cmplwi r28,0x40 @0x82706178` against KU_MAX_VEHICLE_ASSETS, baked
// file BrnTrafficCarStreamer.h line 244. The second (line 245) streamed the id
// and muNumAssets into the message buffer; the house CGS_ASSERT carries the
// static text instead, per project convention.
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
// @0x82706288. Every asset that SHOULD be loaded IS loaded. Slots with no
// request bit set are skipped (the console's `(flags & 3) == 0` continue).
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Inlined into TrafficEntityModule::UpdateStreaming @0x82748848 (0x827488D0..0x827488FC);
// DWARF .h:275. "Nothing is resident" -- every KNOWN asset's state byte is
// E_LOADSTATE_NOT_LOADED. Unlike AreAllAssetsLoaded below there is NO maxLoadFlags
// filter: the console's inlined loop indexes mauLoadStates directly. An empty
// catalogue is trivially "all unloaded", which is the console's own `beq` past the
// loop to `v9 = 1`.
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
// @0x8271D440. Three asserts (baked BrnTrafficCarStreamer.h lines 309/310/311),
// then the stub's first import slot. The X360's last two instructions are
//   result = *BrnTraffic::GraphicsStub_::operat( 32*asset + this + 10232 );
// i.e. ResourcePtr<GraphicsStub>::operator*() then slot 0 -- which is
// `maGraphicsStubs[luAssetId]->mpVehicleGraphics` written by name.
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
// @0x82706300 -- EXPORT HOLE (no .ida-exports JSON). Body from the leak
// (BrnTrafficCarStreamer.h:284), and it is corroborated by the CONSUMER that IS
// exported: Update @0x8274F740 reads the same 64-bit word at this+9080+8*asset,
// shifts it right by one, and treats a non-zero result as "this asset was
// rendered recently, keep it resident as a bonus". A producer that sets the TOP
// bit is the only thing that makes that ageing shift meaningful.
//
// [T1-stream] note: the per-asset diagnostic latch lives in the .cpp beside the
// four load hooks; this inline stays free of it because RenderTrafficCar calls it
// once per drawn car per frame.
// -----------------------------------------------------------------------------
inline void
TrafficCarStreamer::NotifyAssetRenderedThisFrame( u32 luAssetId )
{
    CGS_ASSERT( luAssetId < muNumAssets,
                "Attempted to use a traffic car asset that is not known about" );

    mauRenderingHistory[luAssetId] |= KU_ASSET_RENDERED_FLAG;
}

}   // namespace BrnTraffic
