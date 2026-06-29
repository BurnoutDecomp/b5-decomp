#pragma once

// ============================================================================
// BrnWorld::RaceCarBaseComponentStreamer
//   GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarBaseComponentStreamer.h
//   (DWARF home; this TU's real .cpp home is the matching .cpp beside it).
//
// The shared per-race-car asset-streaming base. It derives the fixed-capacity
// BrnWorld::BaseStreamer<8> (one slot per active race car, KI_MAX_ACTIVE_RACE_CARS
// == 8) and turns the generic streamer state-machine notifications into per-car
// "asset loaded / asset unloading" hooks that the concrete leaves
// (RaceCarAudioStreamer, RaceCarStreamer) implement.
//
// Per race-car index it tracks:
//   maDesiredAssets[i]  -- the CgsID the car currently WANTS loaded (0 == none)
//   maLoadedAssets[i]   -- the CgsID actually loaded for that car (0 == none)
//   mAddedEntries       -- bit set: car i has an outstanding add/request
//   mLoadedEntries      -- bit set: car i's asset is loaded
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match):
//   Construct        @ 0x822D4C08  (forwards InternalBaseStreamer::Construct, zeroes the tables + both bit arrays)
//   OnUnloadBegin    @ 0x822C0220  (clear loaded bit + loaded asset, notify OnAssetUnloading)
//   OnLoadComplete   @ 0x822C0450  (set loaded bit + loaded asset, notify OnAssetLoaded)
//   OnUnloadComplete @ 0x822A5518  (debug-log only)
// Declaration shape (members, the two new pure-virtual hooks, the accessor set) is
// from the DecFIGS DWARF for BrnRaceCarBaseComponentStreamer.h. The Feb-2007 partial
// source is the style/idiom reference; behaviour+layout authority is the X360 asm.
//
// Only the four functions above are bodied by this TU. The remaining members the
// DWARF lists (Destruct/AddEntry/RemoveEntry/IsVehicleAssetLoaded/QueryLoad/
// QueryUnload/OnLoadBegin) are DECLARED here for home completeness and are bodied by
// the dedicated streamer TUs / the leaf classes -- do NOT fork this header, GROW it.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // CgsID (typedef u64)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"           // CgsContainers::BitArray<N>
#include "GameSource/World/BrnBaseStreamer.h"                        // BrnWorld::BaseStreamer<N>, StreamerTargetEntry
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"                // BrnResource::EAssetSet
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"          // BrnResource::GameDataIO::GameDataAssetEvent

namespace BrnWorld
{

// The shared asset streamer for the per-race-car components. Abstract: the two
// OnAsset* hooks are pure-virtual and supplied by RaceCarAudioStreamer/RaceCarStreamer.
class RaceCarBaseComponentStreamer : public BaseStreamer<8>
{
public:
    typedef BaseStreamer<8> BaseClass;

    // One streamer slot per simultaneously-active race car (== 8 on this build; the
    // X360 asserts spell "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS"). Class-scoped to
    // keep the bound self-contained (the CrashModule sibling owns a namespace-scope
    // copy; class scope here avoids any cross-header redefinition).
    static const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

    // @ 0x822D4C08. Forward the base streamer Construct (8 slots) and clear the
    // desired/loaded asset tables. lbAllowFailure follows the X360 (constant true).
    void Construct( s32 liPoolId, bool lbAllowFailure, BrnResource::EAssetSet leAssetSet );

    void Destruct();

    void AddEntry( CgsID lAssetId, s32 liActiveRaceCar );
    void RemoveEntry( CgsID lAssetId, s32 liActiveRaceCar );
    void RemoveEntry( s32 liActiveRaceCar );

    bool  IsVehicleAssetLoaded( CgsID lAssetId, s32 liActiveRaceCar );
    CgsID GetDesiredAsset( s32 liActiveRaceCar ) const;
    CgsID GetLoadedAsset( s32 liActiveRaceCar ) const;

protected:
    // New per-car notification hooks supplied by the concrete streamer (DWARF: this
    // class adds them at vtable +0x20 / +0x24). OnLoadComplete dispatches OnAssetLoaded;
    // OnUnloadBegin dispatches OnAssetUnloading.
    virtual void OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent ) = 0;
    virtual void OnAssetUnloading( s32 liActiveRaceCar ) = 0;

    // InternalBaseStreamer state-machine notifications.
    virtual s32  QueryLoad( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength );
    virtual s32  QueryUnload( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength );
    virtual void OnLoadBegin( s32 liListIndex );
    virtual void OnUnloadBegin( s32 liListIndex );
    virtual void OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex );
    virtual void OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent, s32 liListIndex );

protected:
    // The RaceCarAudioStreamer leaf FULLY overrides OnLoadComplete/OnUnloadComplete and
    // drives mLoadedEntries itself (the X360 inlines mLoadedEntries.SetBit/UnSetBit at the
    // override sites @0x822C08C8/0x822A55C0), so these per-car tables/bit-sets are protected
    // rather than private. The base's own load/unload hooks still own them by name.
    CgsID                       maDesiredAssets[KI_MAX_ACTIVE_RACE_CARS]; // BrnRaceCarBaseComponentStreamer.h:146
    CgsID                       maLoadedAssets[KI_MAX_ACTIVE_RACE_CARS];  // BrnRaceCarBaseComponentStreamer.h:147
    CgsContainers::BitArray<8u> mAddedEntries;                            // BrnRaceCarBaseComponentStreamer.h:148
    CgsContainers::BitArray<8u> mLoadedEntries;                           // BrnRaceCarBaseComponentStreamer.h:149
};

} // namespace BrnWorld
