// =============================================================================
// BrnTrafficCarStreamer.cpp
//
// BrnTraffic::TrafficCarStreamer -- the traffic system's streamed vehicle-GRAPHICS
// table. See BrnTrafficCarStreamer.h for the member map, the three sources it was
// recovered from, and the ship-vs-leak divergence list.
//
// Bodies recovered here (X360 ARTIST addresses; ✱ = EXPORT HOLE, see each banner):
//   Construct()                                   ✱ 0x827539A0
//   Destruct()                                      0x8274F690
//   SetAssetList( u32, const VehicleAsset* )        0x82753A38
//   AddVehiclesToTargetList( u32, const u8* )       0x8274F6A0
//   Update( const u8*, u32 )                        0x8274F740
//   GetBonusAssets( u8*, u32* )                     0x8274F8C0
//   QueryLoad / QueryUnload                         (ICF-folded `return 0`; leak)
//   OnLoadBegin( s32 )                              0x82757E40
//   OnUnloadBegin( s32 )                            0x82758008
//   OnLoadComplete( const GameDataAssetEvent*, s32 )0x82758270
//   OnUnloadComplete( const UnloadGameDataResponse*, s32 ) 0x82753F08
// plus the five header inlines (IsTrafficAssetLoaded @0x82706160,
// AreAllAssetsLoaded @0x82706288, GetGraphicsSpec @0x8271D440,
// GetWheelGraphicsSpec @0x8271D678, NotifyAssetRenderedThisFrame ✱0x82706300)
// and ClearAssetList (inlined into its callers on the console).
//
// The class's own constructor, BrnTraffic::TrafficCarStreamer::TrafficCarStreamer
// @0x827E3E98, is deliberately NOT written by hand: reading it instruction by
// instruction shows it is the COMPILER-GENERATED default constructor, not source.
// It installs the vtable at +0, stores 0 at +0x18 (the leading `bool
// mbIsConstructed` of the base's embedded VariableEventQueue), and then runs 64
// iterations of a 32-byte-stride loop that writes 0 to +0/+4/+8, `&record` to
// +0xC/+0x10/+0x14 and 0 to +0x18 of each record starting at +10232. That is
// exactly CgsResource::BaseResourcePtr::BaseResourcePtr() @0x82204E20 (see its
// banner in CgsBaseResourcePtr.cpp: "mpResourceMemory + the two mHandle pointers
// are zeroed, the list is self-circular, mpThis = this, muThreadId = 0") applied
// to maGraphicsStubs[64]. Declaring an explicit constructor here would ADD a
// symbol the original does not have; the implicit one emits the same stores.
//
// ⚠ The X360 assert messages that streamed an index/name into the message buffer
// are reproduced with the house CGS_ASSERT carrying the static text, per project
// convention (the same treatment BrnRaceCarComponentStreamers.cpp uses).
// =============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.h"

#include <cstring>   // strstr, strlen
#include <cstdlib>   // getenv  ([T1-stream] diagnostic only)

#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDUnCompress, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint ([T1-stream] only)

namespace BrnTraffic
{

// -----------------------------------------------------------------------------
// [T1-stream] BRING-UP PROBE -- NOT IN THE X360 BINARY. DELETE WHEN STABLE.
//
// Latched per-asset load-state transition trace. It exists because the whole
// point of this cluster is "does a VEH_T*_GR bundle ever get requested, and does
// it come back?", and the only honest answer is a dump rather than an argument.
// Opt-in behind BRN_TRAFFIC_DIAG; VALUE-LATCHED per asset, so a slot that keeps
// re-reporting the same state prints once, and a full load cycle prints exactly
// four lines (0 -> 1 -> 2, then 3 -> 0 on unload).
//
// The latch array is a FILE STATIC on purpose: putting it in the class would
// change the object's layout, which is the one thing this wave must not do.
// -----------------------------------------------------------------------------
namespace
{
    void TrafficStreamDiag_NoteLoadState( u64 luAsset, u32 luNewState, const char* lpcHook )
    {
        static const bool sbTrafficDiag = ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        if( !sbTrafficDiag || CgsDev::Log::gpDebugPrint == 0 )
        {
            return;
        }

        static u8 sauLastLogged[KU_MAX_VEHICLE_ASSETS] = { 0 };
        static bool sbSeeded = false;
        if( !sbSeeded )
        {
            sbSeeded = true;
            for( u32 luSlot = 0; luSlot < KU_MAX_VEHICLE_ASSETS; luSlot++ )
            {
                sauLastLogged[luSlot] = 0xFFu;   // "nothing logged yet" sentinel
            }
        }

        if( luAsset >= KU_MAX_VEHICLE_ASSETS )
        {
            return;
        }
        if( sauLastLogged[luAsset] == static_cast< u8 >( luNewState ) )
        {
            return;
        }
        sauLastLogged[luAsset] = static_cast< u8 >( luNewState );

        *CgsDev::Log::gpDebugPrint
            << "[T1-stream] asset " << static_cast< s32 >( luAsset )
            << " -> loadstate " << static_cast< s32 >( luNewState )
            << " (" << lpcHook << ") [DELETE-WHEN-STABLE]\n";
    }
}


// -----------------------------------------------------------------------------
// ✱ @0x827539A0 -- EXPORT HOLE. There is NO .ida-exports JSON for this address,
// so neither its pseudocode nor its assembly can be read; the body below is the
// leaked Feb-2007 TrafficCarStreamer::Construct with ONE adaptation, flagged.
//
// WHAT IS SOLID:
//   * The call itself is attested from the CALLER: TrafficEntityModule::Construct
//     @0x82740220 calls `TrafficCarStreamer::Construct( this + 469848 )`, and
//     TrafficEntityModule::UpdateStreaming @0x82748848 addresses the same member
//     as `v4 = a1 + 469848`. So the function exists, takes only `this`, and is
//     called exactly once from the module's own Construct.
//   * Every store in the leak's body is corroborated by a READER in the exported
//     siblings: mauLoadStates must start at 0 (OnLoadBegin @0x82757E40 asserts
//     ==0 before the first load), maxLoadFlags must start at 0 (AreAllAssetsLoaded
//     @0x82706288 skips slots whose low two bits are clear), mauRenderingHistory
//     must start at 0 (Update @0x8274F740 treats non-zero as "recently drawn"),
//     and every maGraphicsStubs slot must start NULL (OnLoadBegin asserts exactly
//     that). muNumAssets starts 0 and is (re)set by SetAssetList.
//
// ⚠ THE ONE ADAPTATION -- the base Construct's parameter list changed after the
// leak. The leak calls the 3-parameter form:
//     BaseClass::Construct( E_POOL_TRAFFIC, E_ASSETSET_GRAPHICS, false );
// The SHIP base is 4-parameter (BrnBaseStreamer.h, pinned by
// WorldGraphicsStreamer::Construct @0x827CA388 passing list/pool/slot-pool/
// asset-set/allow-failure): `Construct( liPoolId, lbSlotPoolSystem, leAssetSet,
// lbAllowFailure )`. The inserted argument is lbSlotPoolSystem, and it is passed
// FALSE here for the same reason RaceCarBaseComponentStreamer::Construct passes
// false: InternalBaseStreamer::PostLoadRequest only adds the entry index to the
// pool id when that flag is set, and traffic graphics all land in the single
// E_POOL_TRAFFIC pool (one pool per streamer, not pool+slot).
// ⚠ FLAG: because this body is an export hole, `false` for lbSlotPoolSystem and
// the E_POOL_TRAFFIC value itself (15, DWARF ps3mem.h; see the enum banner in
// BrnGameDataEvents.h) are the two facts here that the ARTIST build does not
// directly attest. If a traffic bundle is later observed being requested from
// pool 15+slot instead of pool 15, this flag is the line to revisit.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::Construct()
{
    BaseClass::Construct( BrnResource::E_POOL_TRAFFIC,
                          false /*lbSlotPoolSystem -- see the FLAG above*/,
                          BrnResource::E_ASSETSET_GRAPHICS,
                          false /*lbAllowFailure*/ );

    muNumAssets = 0;

    for( u32 luAsset = 0; luAsset < KU_MAX_VEHICLE_ASSETS; luAsset++ )
    {
        maxLoadFlags[luAsset]         = E_LOADFLAG_NONE;
        mauLoadStates[luAsset]        = E_LOADSTATE_NOT_LOADED;
        mauRenderingHistory[luAsset]  = 0;

        // The leak spells this as the fully-expanded base-40 fold of the empty
        // string -- `((CgsID)0) + (CgsID)40 * (...)` twelve deep -- which is
        // CgsIDCompress("") and evaluates to 0. Written as the constant it is.
        maAssetIds[luAsset]           = 0;

        // Leak: `maGraphicsStubs[luAsset] = CgsResource::NULLResourcePtr;`. The
        // console spells that assignment as CreateFromHandle(slot, &<sentinel>+0x14)
        // -- the {mpThis, muThreadId} pair of the null sentinel -- which is the
        // committed CgsResource::NULLResourceHandle idiom (see the note on
        // ResourcePtr<T>::operator=(const ResourceHandle&) in CgsResourcePtr.h,
        // and RaceCarStreamer::Construct @0x822F7FA0 which does the same thing).
        maGraphicsStubs[luAsset]      = CgsResource::NULLResourceHandle;
    }
}


// -----------------------------------------------------------------------------
// @0x8274F690. The entire body is `*(this + 12280) = 0`.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::Destruct()
{
    muNumAssets = 0;
}


// -----------------------------------------------------------------------------
// @0x82753A38 -- THE HIGHEST-LEVERAGE FUNCTION OF THIS WAVE.
//
// Publish TrafficData's vehicle-asset catalogue into the streamer: for each
// asset, uncompress its CgsID to text, truncate at the first space (the baked
// ids are space-padded to 12 characters by CgsIDUnCompress), and re-compress the
// bare name through BrnResource::MakeTrafficVehicleId -- which prefixes "TVEH",
// i.e. the id the resource system streams VEH_T<code>_GR by. Then verify no two
// assets resolved to the same id.
//
// Store-for-store against the asm:
//   0x82753A38  a2 >= 0x40           -> assert "luNumAssets < KU_MAX_VEHICLE_ASSETS" (cpp:115)
//               !a3                  -> assert "lpaAssets"                           (cpp:116)
//               v4[3070] = a2        -> muNumAssets = luNumAssets
//               v8 = v4 + 2430       -> &maAssetIds[0]   (2430*4 == 9720 ✓)
//               v7 = a3, v7 += 8     -> the VehicleAsset stride is 8 ✓ (one CgsID)
//               CgsIDUnCompress(*(v7 + 4), v62)   -- Hex-Rays' split of the 8-byte
//                                       load; the source reads lpaAssets[i].GetVehicleId()
//               strstr(v62, " ")     -> truncate at the first space
//               strlen == 0          -> assert "Traffic asset <n> has no name"       (cpp:131)
//               strlen  > 8          -> assert "...longer than the max 8 characters"  (cpp:132)
//               *v8 = MakeTrafficVehicleId(v62)
//   then the O(n^2) duplicate scan (v22/v25 walking by 2 dwords == 8 bytes)
//               -> assert "Duplicate asset id ... - ids <i> and <j>"                  (cpp:147)
//
// The line numbers moved by two versus the leak (113/114/129/130/145) -- normal
// post-Feb-2007 drift, and a useful confirmation that this really is the same
// function rather than a look-alike.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::SetAssetList( u32 luNumAssets, const VehicleAsset* lpaAssets )
{
    CGS_ASSERT( luNumAssets < KU_MAX_VEHICLE_ASSETS, "luNumAssets < KU_MAX_VEHICLE_ASSETS" );
    CGS_ASSERT( lpaAssets != 0, "lpaAssets" );

    muNumAssets = luNumAssets;

    for( u32 luAsset = 0; luAsset < luNumAssets; luAsset++ )
    {
        char lacBuffer[KI_CGSID_STRING_LEN];
        CgsIDUnCompress( lpaAssets[luAsset].GetVehicleId(), lacBuffer );

        // CgsIDUnCompress right-pads with spaces; the asset name is the head.
        char* lpcLastChar = strstr( lacBuffer, " " );
        if( lpcLastChar != 0 )
        {
            *lpcLastChar = '\0';
        }

        CGS_ASSERT( strlen( lacBuffer ) > 0, "Traffic asset has no name" );
        CGS_ASSERT( strlen( lacBuffer ) <= 8,
                    "Traffic asset name is longer than the max 8 characters" );

        maAssetIds[luAsset] = BrnResource::MakeTrafficVehicleId( lacBuffer );
    }

    // The console keeps this scan unconditionally (it is an assert-only pass; the
    // ids are already published above).
    for( u32 luAsset = 0; luAsset < luNumAssets; luAsset++ )
    {
        for( u32 luTestAsset = luAsset + 1; luTestAsset < luNumAssets; luTestAsset++ )
        {
            CGS_ASSERT( maAssetIds[luAsset] != maAssetIds[luTestAsset],
                        "Duplicate asset id found in traffic asset list" );
        }
    }

    // ---- [T1-stream] bring-up probe (NOT in the X360 binary) ----------------
    // One-shot: how many assets the catalogue published. If this never prints,
    // nothing upstream ever called SetAssetList and no traffic bundle can load --
    // which is the exact failure this cluster exists to close.
    {
        static const bool sbTrafficDiag = ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        static bool sbLogged = false;
        if( sbTrafficDiag && !sbLogged && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[T1-stream] SetAssetList published " << static_cast< s32 >( luNumAssets )
                << " traffic vehicle assets [DELETE-WHEN-STABLE]\n";
        }
    }
}


// -----------------------------------------------------------------------------
// @0x8274F6A0. Flag every asset the caller's hull needs as REQUESTED this frame.
// The asm's loop body is literally `*(v3 + v7 + 9592) |= 1u` with
// `v7 = *(i + a3)` -- a byte index into maxLoadFlags.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::AddVehiclesToTargetList( u32 luNumVehicles, const u8* lpauVehicleAssetIds )
{
    CGS_ASSERT( luNumVehicles <= muNumAssets, "luNumVehicles <= muNumAssets" );
    CGS_ASSERT( lpauVehicleAssetIds != 0, "lpauVehicleAssetIds" );

    for( u32 luVehicle = 0; luVehicle < luNumVehicles; luVehicle++ )
    {
        const u32 luAsset = lpauVehicleAssetIds[luVehicle];

        maxLoadFlags[luAsset] |= E_LOADFLAG_REQUESTED;
    }
}


// -----------------------------------------------------------------------------
// @0x8274F740. Rebuild the base streamer's target list for this frame, then pump
// the base engine.
//
// ⭐ SHIP DIVERGENCE (the asm wins over the leak): Update takes an OVERRIDE bonus
// list. The asm's first block is `if (a3 > 2) assert("luNumBonusAssets <=
// KU_MAX_BONUS_STREAMED_ASSETS", cpp:195)`, then ClearTargetList, then a
// two-armed branch on a2:
//   * a2 != 0 -- use the caller's list verbatim: for each entry assert
//     "lpauOverrideBonusAssets[luAsset] < muNumAssets" (cpp:234) and OR in
//     E_LOADFLAG_BONUS. The rendering history is NOT aged in this arm.
//   * a2 == 0 -- the leak's behaviour: age every history word one frame
//     (`*v8 = *v8 >> 1`) and promote up to KU_MAX_BONUS_STREAMED_ASSETS
//     still-non-zero, not-already-requested assets to BONUS.
// The sole caller, TrafficEntityModule::UpdateStreaming @0x82748848, chooses
// between the two: it passes {0, 0} normally, and a pointer+count taken out of
// its network/replay record when one is active.
//
// Then, in both arms, every asset whose low two flag bits are set is pushed into
// the base target list as `AddEntry( maAssetIds[i], true, i )` -- the user id is
// the ASSET INDEX, which is exactly what the four On* hooks read back through
// GetUserId(liListIndex).
// -----------------------------------------------------------------------------
void TrafficCarStreamer::Update( const u8* lpauOverrideBonusAssets, u32 luNumBonusAssets )
{
    CGS_ASSERT( luNumBonusAssets <= KU_MAX_BONUS_STREAMED_ASSETS,
                "luNumBonusAssets <= KU_MAX_BONUS_STREAMED_ASSETS" );

    ClearTargetList();

    if( lpauOverrideBonusAssets != 0 )
    {
        for( u32 luAsset = 0; luAsset < luNumBonusAssets; luAsset++ )
        {
            CGS_ASSERT( lpauOverrideBonusAssets[luAsset] < muNumAssets,
                        "lpauOverrideBonusAssets[luAsset] < muNumAssets" );

            maxLoadFlags[lpauOverrideBonusAssets[luAsset]] |= E_LOADFLAG_BONUS;
        }
    }
    else
    {
        u32 luBonusAssetsUsed = 0;

        for( u32 luAsset = 0; luAsset < muNumAssets; luAsset++ )
        {
            // Age the 64-frame "was drawn" history one frame.
            mauRenderingHistory[luAsset] >>= 1;

            if( luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS )
            {
                if( mauRenderingHistory[luAsset] != 0 )
                {
                    if( ( maxLoadFlags[luAsset] & E_LOADFLAG_REQUESTED ) == E_LOADFLAG_REQUESTED )
                    {
                        // Already wanted for real; it does not spend a bonus slot.
                        continue;
                    }

                    maxLoadFlags[luAsset] |= E_LOADFLAG_BONUS;
                    luBonusAssetsUsed++;
                }
            }
        }
    }

    u32 luNumEntriesAdded = 0;

    for( u32 luAsset = 0; luAsset < muNumAssets; luAsset++ )
    {
        if( maxLoadFlags[luAsset] & E_LOADFLAG_SHOULD_BE_LOADED )
        {
            AddEntry( maAssetIds[luAsset], true /*lbIsSafe*/, luAsset /*luUserId*/ );
            luNumEntriesAdded++;
        }
    }

    {
        // [T1-stream] ONE-SHOT -- NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
        //
        // ⭐ THE LINE THAT ANSWERS THE WHOLE WAVE-1 QUESTION. Everything upstream of here can
        // look healthy and still request nothing: the rounds-1+2 boot printed
        // "[T1-stream] SetAssetList published 27 traffic vehicle assets" and then never asked
        // for a bundle, because UpdateStreaming (the only pump of this function) was gated.
        // This prints the FIRST frame on which a non-empty target list actually reaches the
        // base streamer -- which is the frame a VEH_T*_GR request can first be posted.
        //
        // THE CHAIN THIS SITS ON, verified end to end for round 3 rather than assumed:
        //   AddEntry(...)                        -> the base target list
        //   BaseClass::Update()                  -> InternalBaseStreamer::Update
        //   ...UpdateLoading, E_LOADSTREAM_REQUEST -> PostLoadRequest(slot)
        //   PostLoadRequest                      -> mGDRequestInterface.mRequestQueue
        //                                           .AddEvent(LoadGameDataEvent, 26)
        //   TrafficEntityModule::UpdateStreaming -> Append that queue into
        //                                           OutputBuffer_PostPhysics's
        //                                           mResourceRequestInterface
        // (BrnBaseStreamer.cpp:354/364/416.) Note the ORDER dependence: UpdateLoading clears
        // mGDRequestInterface at the top of its WAIT stage, i.e. on the NEXT visit -- so the
        // Append has to happen in the same frame as this Update, after it. UpdateStreaming
        // does exactly that (its step 3 then its step 5), which is why the pump and the
        // carry-over cannot be split across functions.
        static const bool sbTrafficDiag = ( getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        static bool sbLogged = false;
        if( sbTrafficDiag && !sbLogged && luNumEntriesAdded != 0
            && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[T1-stream] FIRST traffic bundle request: " << static_cast< s32 >( luNumEntriesAdded )
                << " of " << static_cast< s32 >( muNumAssets )
                << " assets pushed into the base streamer target list"
                << " [DELETE-WHEN-STABLE]\n";
        }
    }

    BaseClass::Update();
}


// -----------------------------------------------------------------------------
// @0x8274F8C0. SHIP-ONLY. Report which assets are currently held as BONUS (flag
// bits == exactly E_LOADFLAG_BONUS, i.e. bonus and NOT requested), as byte
// indices. This is the producer whose output feeds back into Update's override
// arm across a network/replay boundary.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::GetBonusAssets( u8* lpauOutBonusAssets, u32* lpuOutNumBonusAssets ) const
{
    CGS_ASSERT( lpauOutBonusAssets != 0, "lpauOutBonusAssets" );
    CGS_ASSERT( lpuOutNumBonusAssets != 0, "lpuOutNumBonusAssets" );

    u32 luBonusAssetsUsed = 0;

    for( u32 luAsset = 0; luAsset < muNumAssets; luAsset++ )
    {
        if( ( maxLoadFlags[luAsset] & E_LOADFLAG_SHOULD_BE_LOADED ) == E_LOADFLAG_BONUS )
        {
            CGS_ASSERT( luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS,
                        "luBonusAssetsUsed < KU_MAX_BONUS_STREAMED_ASSETS" );

            lpauOutBonusAssets[luBonusAssetsUsed] = static_cast< u8 >( luAsset );
            luBonusAssetsUsed++;
        }
    }

    *lpuOutNumBonusAssets = luBonusAssetsUsed;
}


// -----------------------------------------------------------------------------
// The two "should I load/unload one of these candidates?" hooks. The traffic
// streamer declines to arbitrate -- it drives everything through the target list
// it rebuilds each Update -- so both are `return 0`.
//
// ⚠ NEITHER IS IN THE X360 LEDGER for this class. progress/identity.json holds
// FOURTEEN BrnTraffic::TrafficCarStreamer symbols -- AddVehiclesToTargetList,
// AreAllAssetsLoaded, Destruct, GetBonusAssets, GetGraphicsSpec,
// GetWheelGraphicsSpec, IsTrafficAssetLoaded, OnLoadBegin, OnLoadComplete,
// OnUnloadBegin, OnUnloadComplete, SetAssetList, TrafficCarStreamer (the ctor),
// Update -- and QueryLoad/QueryUnload are not among them. (An earlier revision of
// this banner said "twelve"; corrected 2026-08-21 in the C5 fix round. The
// conclusion was and is unchanged.)
// That is expected rather than suspicious: a body that is one `li r3,0 ; blr` is
// identical for every streamer that declines, so the console's ICF folds them all
// onto one shared thunk and no per-class symbol survives. Rung 2 settles that they
// are nevertheless real members of this class: the DecFIGS DWARF
// (dwarfdump/.../BrnTrafficCarStreamer.h) declares both, at .cpp:247 and .cpp:258,
// in the protected virtual block. The bodies are the leak's
// (BrnTrafficCarStreamer.cpp:242 / :258) verbatim, and they MUST exist because
// InternalBaseStreamer declares both pure virtual.
// -----------------------------------------------------------------------------
s32 TrafficCarStreamer::QueryLoad( const BrnWorld::StreamerTargetEntry* lpPotentialList,
                                   s32 liPotentialListLength )
{
    (void)lpPotentialList;
    (void)liPotentialListLength;

    return 0;
}

s32 TrafficCarStreamer::QueryUnload( const BrnWorld::StreamerTargetEntry* lpPotentialList,
                                     s32 liPotentialListLength )
{
    (void)lpPotentialList;
    (void)liPotentialListLength;

    return 0;
}


// -----------------------------------------------------------------------------
// @0x82757E40. The base engine has begun loading list slot liListIndex; move the
// asset it stands for into the LOAD_STARTED state.
//
// The asm reads the user id as `*(24 * a2 + *(a1 + 12) + 16)` -- the current-entry
// list (console +12) at a 24-byte stride, field +16 -- which is exactly
// InternalBaseStreamer::GetUserId( liListIndex ), and it reads it as a 64-bit
// value (`__int64 v3`). Written by name.
//
// SHIP DIVERGENCE: the leak has FOUR asserts here; the ship has THREE -- the
// leak's "Loading traffic asset <n>, which is currently not requested" flags
// check is gone (the asm's three baked lines are 331 / 333 / 334).
// -----------------------------------------------------------------------------
void TrafficCarStreamer::OnLoadBegin( s32 liListIndex )
{
    const u64 luAsset = GetUserId( liListIndex );
    CGS_ASSERT( luAsset < muNumAssets, "luAsset < muNumAssets" );

    CGS_ASSERT( mauLoadStates[luAsset] == E_LOADSTATE_NOT_LOADED,
                "Traffic asset was in the wrong state when starting loading" );
    CGS_ASSERT( CgsResource::NULLResourcePtr.IsEqual( &maGraphicsStubs[luAsset] ),
                "Loading a traffic asset which already has a resource" );

    mauLoadStates[luAsset] = E_LOADSTATE_LOAD_STARTED;

    TrafficStreamDiag_NoteLoadState( luAsset, E_LOADSTATE_LOAD_STARTED, "OnLoadBegin" );
}


// -----------------------------------------------------------------------------
// @0x82758008. The base engine has begun unloading slot liListIndex: mark the
// asset UNLOAD_STARTED and drop its resource pointer back to null.
//
// The asm's last instruction is `CreateFromHandle( &maGraphicsStubs[asset],
// &dword_8300D44C )`, where dword_8300D44C == the null-sentinel + 0x14, i.e. the
// {mpThis, muThreadId} pair -- the committed NULLResourceHandle idiom.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::OnUnloadBegin( s32 liListIndex )
{
    const u64 luAsset = GetUserId( liListIndex );
    CGS_ASSERT( luAsset < muNumAssets, "luAsset < muNumAssets" );

    CGS_ASSERT( mauLoadStates[luAsset] == E_LOADSTATE_LOADED,
                "Traffic asset was in the wrong state when starting unloading" );
    CGS_ASSERT( ( maxLoadFlags[luAsset] & E_LOADFLAG_SHOULD_BE_LOADED ) == 0,
                "Unloading a traffic asset which is currently requested" );
    CGS_ASSERT( !CgsResource::NULLResourcePtr.IsEqual( &maGraphicsStubs[luAsset] ),
                "Unloading a traffic asset which has no resource" );

    mauLoadStates[luAsset] = E_LOADSTATE_UNLOAD_STARTED;

    maGraphicsStubs[luAsset] = CgsResource::NULLResourceHandle;

    TrafficStreamDiag_NoteLoadState( luAsset, E_LOADSTATE_UNLOAD_STARTED, "OnUnloadBegin" );
}


// -----------------------------------------------------------------------------
// @0x82758270. The bundle arrived: mark the asset LOADED and bind its
// GraphicsStub resource pointer from the response's handle.
//
// The asm's binding step is
//     CgsResource::BaseResourcePtr::CreateFromHandle( &maGraphicsStubs[asset],
//                                                     lpEvent + 32 );
// -- event byte +0x20, which is the GetTrafficVehicleGraphicsResponse's own
// resource handle (see the ⭐ banner over that struct in BrnGameDataEvents.h for
// why the committed model reaches it through the inherited `mHandle`, and why
// re-declaring a handle on the derived type would be a bug). The downcast below
// is the leak's (BrnTrafficCarStreamer.cpp:342) and is what gives the response
// type its consumer.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent,
                                         s32 liListIndex )
{
    CGS_ASSERT( lpEvent != 0, "lpEvent" );

    const u64 luAsset = GetUserId( liListIndex );
    CGS_ASSERT( luAsset < muNumAssets, "luAsset < muNumAssets" );

    CGS_ASSERT( mauLoadStates[luAsset] == E_LOADSTATE_LOAD_STARTED,
                "Traffic asset was in the wrong state when finished loading" );
    CGS_ASSERT( CgsResource::NULLResourcePtr.IsEqual( &maGraphicsStubs[luAsset] ),
                "Loading a traffic asset which already has a resource" );

    mauLoadStates[luAsset] = E_LOADSTATE_LOADED;

    const BrnResource::GameDataIO::GetTrafficVehicleGraphicsResponse* lpResponse =
        static_cast< const BrnResource::GameDataIO::GetTrafficVehicleGraphicsResponse* >( lpEvent );
    maGraphicsStubs[luAsset] = lpResponse->GetTrafficVehicleGraphicsObjectHandle();

    CGS_ASSERT( !CgsResource::NULLResourcePtr.IsEqual( &maGraphicsStubs[luAsset] ),
                "Didn't get a resource back after loading a traffic asset" );

    TrafficStreamDiag_NoteLoadState( luAsset, E_LOADSTATE_LOADED, "OnLoadComplete" );
}


// -----------------------------------------------------------------------------
// @0x82753F08. The unload finished: the slot is free again. (The resource
// pointer was already dropped in OnUnloadBegin.)
// -----------------------------------------------------------------------------
void TrafficCarStreamer::OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent,
                                           s32 liListIndex )
{
    (void)lpEvent;   // the console reads nothing out of it

    const u64 luAsset = GetUserId( liListIndex );
    CGS_ASSERT( luAsset < muNumAssets, "luAsset < muNumAssets" );

    CGS_ASSERT( mauLoadStates[luAsset] == E_LOADSTATE_UNLOAD_STARTED,
                "Traffic asset was in the wrong state when finished unloading" );

    mauLoadStates[luAsset] = E_LOADSTATE_NOT_LOADED;

    TrafficStreamDiag_NoteLoadState( luAsset, E_LOADSTATE_NOT_LOADED, "OnUnloadComplete" );
}


// -----------------------------------------------------------------------------
// Never called. See the header for why only the pointer-free runs are pinned.
// -----------------------------------------------------------------------------
void TrafficCarStreamer::_AssertLayout()
{
    // The three POINTER-FREE runs. Each of these spans is identical on the X360
    // and on x64 because every element is a fixed-width integer type, so the
    // console's own arithmetic (9080 + 512 == 9592 == maxLoadFlags, + 64 ==
    // mauLoadStates, + 64 == maAssetIds, + 512 == maGraphicsStubs) holds here as
    // a RELATIVE fact even though every absolute offset has moved.
    static_assert( sizeof( u64 ) * KU_MAX_VEHICLE_ASSETS == 512,
                   "mauRenderingHistory is 64 x u64 == the console's 9592-9080 span" );
    static_assert( sizeof( CgsID ) * KU_MAX_VEHICLE_ASSETS == 512,
                   "maAssetIds is 64 x CgsID == the console's 10232-9720 span "
                   "(Update @0x8274F740 walks it at an 8-byte stride)" );

    static_assert( offsetof( TrafficCarStreamer, maxLoadFlags )
                     - offsetof( TrafficCarStreamer, mauRenderingHistory ) == 512,
                   "maxLoadFlags follows mauRenderingHistory immediately (console +9592)" );
    static_assert( offsetof( TrafficCarStreamer, mauLoadStates )
                     - offsetof( TrafficCarStreamer, maxLoadFlags ) == KU_MAX_VEHICLE_ASSETS,
                   "mauLoadStates follows maxLoadFlags immediately (console +9656; the two "
                   "byte tables are exactly 0x40 apart in every exported body)" );
    static_assert( offsetof( TrafficCarStreamer, maAssetIds )
                     - offsetof( TrafficCarStreamer, mauLoadStates ) == KU_MAX_VEHICLE_ASSETS,
                   "maAssetIds follows mauLoadStates immediately (console +9720)" );

    // ORDER ONLY past this point -- maGraphicsStubs holds pointers, so its stride
    // is 32 on the console and larger here. Pinning its span would be precisely
    // the X360-offsets-on-x64 bug this project keeps re-learning.
    static_assert( offsetof( TrafficCarStreamer, maGraphicsStubs )
                     > offsetof( TrafficCarStreamer, maAssetIds ),
                   "maGraphicsStubs follows maAssetIds (console +10232)" );
    static_assert( offsetof( TrafficCarStreamer, muNumAssets )
                     > offsetof( TrafficCarStreamer, maGraphicsStubs ),
                   "muNumAssets is the last member (console +12280)" );

    // The catalogue capacity itself, which IsTrafficAssetLoaded @0x82706160 bakes
    // as the literal 0x40 in its first assert.
    static_assert( KU_MAX_VEHICLE_ASSETS == 64,
                   "KU_MAX_VEHICLE_ASSETS == 64 (cmplwi r28,0x40 @0x82706178)" );
}

}   // namespace BrnTraffic
