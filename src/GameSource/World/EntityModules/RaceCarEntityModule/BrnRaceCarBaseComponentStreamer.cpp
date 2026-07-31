#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarBaseComponentStreamer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // CgsDev::Log::gpDebugPrint

// ============================================================================
// BrnWorld::RaceCarBaseComponentStreamer
//   (GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarBaseComponentStreamer.h DWARF home)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Only the four functions the TU's
// ledger lists are bodied here; member access is by NAME (the X360 absolute member
// offsets -- maDesiredAssets @ +0x12D8, maLoadedAssets @ +0x1318, mAddedEntries @
// +0x1358, mLoadedEntries @ +0x1360 in the Construct asm -- are X360 32-bit facts,
// not load-bearing for the host build's member-by-name access).
//
// The X360 build inlines BitArray<8>::IsBitSet/SetBit/UnSetBit at each call site,
// dragging in the BitArray's own internal bounds asserts (CgsBitArray.h:203/:222/:241);
// those belong to the container and are emitted at the inlined call site, so they are
// NOT reproduced here (the committed CgsContainers::BitArray omits them by design and
// the Feb-2007 reference does not show them). The class-level race-car asserts the
// Feb-2007 source spells are reproduced via the house CGS_ASSERT.
//
// The "STRM:" debug prints are unconditional -- the X360 asm for OnLoadComplete
// (0x822C0450) and OnUnloadComplete (0x822A5518) calls the StrStreamBase operator<<
// chain directly with no gxMessageFilterFlags load/test anywhere in either function
// (the Feb-2007 reference wraps them in a message-filter check, but that does not
// match the ASM here and is not reproduced).
// ============================================================================

namespace BrnWorld
{

// @ 0x822D4C08. Forward the base streamer Construct (8 slots, lbAllowFailure from the
// caller -- the X360 always passes 1/true) and clear both per-car asset tables to 0,
// then clear the two per-car bit sets. The X360 Construct asm zeroes maDesiredAssets
// (0x12D8..0x1310) AND maLoadedAssets (0x1318..0x1350) AND mAddedEntries (0x1358) AND
// mLoadedEntries (0x1360) -- the Feb-2007 source does the bit-array clear via the
// inlined BitArray::Construct(); on the committed BitArray that is UnSetAll().
void RaceCarBaseComponentStreamer::Construct( s32 liPoolId, bool lbAllowFailure, BrnResource::EAssetSet leAssetSet )
{
    // BaseStreamer<8>::Construct( liPoolId, lbSlotPoolSystem, leAssetSet, lbAllowFailure ).
    // [fixed on mount] the previous 3-argument call predates the BaseStreamer<N> header
    // landing and did not compile. The X360 race-car component streamers are NOT slot-pool
    // streamers -- each posts its load into the ONE pool its Construct names (pool 4 for
    // graphics/wheel-graphics, 17 for physics/attribs, 6 for sound), with no per-slot pool
    // offset; InternalBaseStreamer::PostLoadRequest only adds the list index to the pool id
    // when mbSlotPoolSystem is set, and the runtime-observed load really did land in pool 4
    // rather than 4+slot.
    BaseClass::Construct( liPoolId, false /*lbSlotPoolSystem*/, leAssetSet, lbAllowFailure );

    mAddedEntries.UnSetAll();
    mLoadedEntries.UnSetAll();

    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; liActiveRaceCar++ )
    {
        maDesiredAssets[liActiveRaceCar] = 0;
        maLoadedAssets[liActiveRaceCar]  = 0;
    }
}

// @ 0x822B6D30. Record the desired asset for a race-car slot and forward the
// request into the base streamer engine. Sets the per-car 'added' bit + desired-
// asset table, then InternalBaseStreamer::AddEntry(lAssetId, lbIsSafe=false,
// luUserId=liActiveRaceCar). The X360 inlines BitArray<8>'s own bounds asserts
// (CgsBitArray.h:203/:222) at the SetBit/IsBitSet sites; those belong to the
// container and are NOT reproduced here, matching the committed style of this TU.
void RaceCarBaseComponentStreamer::AddEntry( CgsID lAssetId, s32 liActiveRaceCar )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    CGS_ASSERT( !mAddedEntries.IsBitSet( static_cast<u32>( liActiveRaceCar ) ),
                "Adding a streaming asset that for a racecar that already has one, racecar=" );

    mAddedEntries.SetBit( static_cast<u32>( liActiveRaceCar ) );
    maDesiredAssets[liActiveRaceCar] = lAssetId;

    InternalBaseStreamer::AddEntry( lAssetId, false, static_cast<u64>( liActiveRaceCar ) );
}

// @ 0x822B6FA0. The counterpart of AddEntry: drop the race-car slot's request.
// The X360 asserts the index (BrnRaceCarBaseComponentStreamer.h:211) and that the
// slot actually HAS an added entry (:212, "Removing a streaming asset that hasn't
// been set, racecar=" with the index streamed into the message buffer), then calls
// InternalBaseStreamer::RemoveEntry with the slot's own desired asset id and the
// race-car index as the user id (asm: the id is read from `8*(i+603)+this`, i.e.
// maDesiredAssets[i], and passed as the id/userId pair), clears the added bit
// (`this+0x1358` == mAddedEntries) and finally zeroes maDesiredAssets[i].
// The X360 inlines BitArray<8>'s own bounds asserts (CgsBitArray.h:203/:241) at the
// IsBitSet/UnSetBit sites; per this TU's committed style they are not reproduced.
void RaceCarBaseComponentStreamer::RemoveEntry( s32 liActiveRaceCar )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    CGS_ASSERT( mAddedEntries.IsBitSet( static_cast<u32>( liActiveRaceCar ) ),
                "Removing a streaming asset that hasn't been set, racecar=" );

    InternalBaseStreamer::RemoveEntry( maDesiredAssets[liActiveRaceCar],
                                       static_cast<u64>( liActiveRaceCar ) );

    mAddedEntries.UnSetBit( static_cast<u32>( liActiveRaceCar ) );
    maDesiredAssets[liActiveRaceCar] = 0;
}

// @ 0x822A1358. Return the CgsID the given race-car slot currently WANTS loaded
// (0 == none). Pure accessor into maDesiredAssets (this+0x12D8).
CgsID RaceCarBaseComponentStreamer::GetDesiredAsset( s32 liActiveRaceCar ) const
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    return maDesiredAssets[liActiveRaceCar];
}

// @ 0x822B7198. True iff the race-car slot's LOADED asset (mLoadedEntries bit set)
// matches lAssetId. Gated read of maLoadedAssets (this+0x1318) behind the per-car
// loaded bit (mLoadedEntries, this+0x1360). The X360 inlines BitArray<8>::IsBitSet's
// own bounds assert (CgsBitArray.h:203) at the test site; not reproduced here.
bool RaceCarBaseComponentStreamer::IsVehicleAssetLoaded( CgsID lAssetId, s32 liActiveRaceCar )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    if( !mLoadedEntries.IsBitSet( static_cast<u32>( liActiveRaceCar ) ) )
    {
        return false;
    }

    return maLoadedAssets[liActiveRaceCar] == lAssetId;
}

// The base streamer's load/unload query hooks. The X360 ARTIST build did not emit a
// standalone body for these in this TU (they are not in the boot-trace/X360 ledger --
// the load/unload list selection is driven from the leaf streamers/base engine), but
// the DecFIGS DWARF homes them in this .cpp (lines 89/108/123) and the Feb-2007
// reference bodies them trivially. Reproduced here so the class vtable is complete and
// links cleanly; behaviour matches the trivial Feb-2007 bodies.
s32 RaceCarBaseComponentStreamer::QueryLoad( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength )
{
    (void)lpPotentialList;
    (void)liPotentialListLength;
    return 0;
}

s32 RaceCarBaseComponentStreamer::QueryUnload( const StreamerTargetEntry* lpPotentialList, s32 liPotentialListLength )
{
    (void)lpPotentialList;
    (void)liPotentialListLength;
    return 0;
}

void RaceCarBaseComponentStreamer::OnLoadBegin( s32 liListIndex )
{
    (void)liListIndex;
}

// @ 0x822C0220. The streamer is unloading list slot liListIndex: map it back to the
// race-car index via the base GetUserId(), clear the loaded bit + loaded asset, and
// notify the concrete streamer.
void RaceCarBaseComponentStreamer::OnUnloadBegin( s32 liListIndex )
{
    const s32 liActiveRaceCar = static_cast<s32>( GetUserId( liListIndex ) );

    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( mLoadedEntries.IsBitSet( static_cast<u32>( liActiveRaceCar ) ),
                "Trying to unload a vehicle asset that hasn't been loaded yet" );

    mLoadedEntries.UnSetBit( static_cast<u32>( liActiveRaceCar ) );
    maLoadedAssets[liActiveRaceCar] = 0;

    OnAssetUnloading( liActiveRaceCar );
}

// @ 0x822C0450. A load of list slot liListIndex completed: map it to the race-car
// index, set the loaded bit + record the loaded asset id. If the car is no longer
// added, or the car's desired asset has changed since the request, just log and bail;
// otherwise notify the concrete streamer.
void RaceCarBaseComponentStreamer::OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex )
{
    const s32 liActiveRaceCar = static_cast<s32>( GetUserId( liListIndex ) );

    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );
    CGS_ASSERT( !mLoadedEntries.IsBitSet( static_cast<u32>( liActiveRaceCar ) ),
                "Loading a vehicle asset twice" );

    mLoadedEntries.SetBit( static_cast<u32>( liActiveRaceCar ) );
    maLoadedAssets[liActiveRaceCar] = lpEvent->GetGameDataId();

    if( !mAddedEntries.IsBitSet( static_cast<u32>( liActiveRaceCar ) ) )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "(base) Completed load of asset for racecar: "
                                   << static_cast<s32>( GetUserId( liListIndex ) )
                                   << ", but racecar is no longer added\n";
        return;
    }

    if( maDesiredAssets[liActiveRaceCar] != maLoadedAssets[liActiveRaceCar] )
    {
        *CgsDev::Log::gpDebugPrint << "STRM: " << "(base) Completed load of asset for racecar: "
                                   << static_cast<s32>( GetUserId( liListIndex ) )
                                   << ", but desired resource has changed\n";
        return;
    }

    OnAssetLoaded( liActiveRaceCar, lpEvent );
}

// @ 0x822A5518. An unload completed: debug-log only (lpEvent unused -- the X360 reads
// only the race-car index back out of the current-entry list via GetUserId()).
void RaceCarBaseComponentStreamer::OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent, s32 liListIndex )
{
    (void)lpEvent;

    *CgsDev::Log::gpDebugPrint << "STRM: " << "(base) Completed unload of asset for racecar: "
                               << static_cast<s32>( GetUserId( liListIndex ) ) << "\n";
}

} // namespace BrnWorld
