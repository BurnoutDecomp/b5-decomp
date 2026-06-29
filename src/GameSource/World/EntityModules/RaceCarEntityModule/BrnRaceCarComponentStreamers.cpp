#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarComponentStreamers.h"

#include <cstring>                                                            // strstr

#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                               // CgsID(Un)Compress, CgsIDConvertToString
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // CgsDev::Log::gpDebugPrint
#include "GameSource/Resource/SharedIO/BrnAssetIds.h"                        // BrnResource::MakeVehicleId, EAssetSet
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"                  // GameDataAssetEvent::GetGameDataId
#include "SharedClasses/DataLists/VehicleList.h"                             // BrnResource::VehicleList(Entry)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarStreamer.h"          // RaceCarStreamer (owner)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"    // InputBuffer/OutputBuffer_PreScene

// ============================================================================
// BrnWorld::RaceCarAudioStreamer
//   (GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarComponentStreamers.h
//    DWARF home; the four trivial sibling streamers' bodies live with RaceCarStreamer.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The
// audio streamer is the heavyweight leaf of the per-race-car component-streamer family:
// on top of the shared RaceCarBaseComponentStreamer asset bookkeeping it owns an
// eight-slot streaming-sound state machine (maEntries) plus an outgoing audio-loaded
// event queue, and FULLY overrides the base load/unload completion hooks (it drives its
// own mLoadedEntries bit set + per-slot state rather than the base desired/loaded tables).
//
//   AddEntry            @ 0x822A5730   (register a desired streaming-sound asset for a car)
//   Construct           @ 0x822ECA68   (base Construct + init the 8 slots; boot-trace EXEC)
//   IsVehicleAssetLoaded@ 0x822A58D0   (slot reached LOADEDANDATTACHED?)
//   OnLoadComplete      @ 0x822C08C8   (bundle finished loading -> LOADEDBUNDLES)
//   OnUnloadComplete    @ 0x822A55C0   (bundle finished unloading -> clear slot)
//   SendLoadRequest     @ 0x822D5080   (publish a "load bundles" request event)
//   SendUnLoadRequest   @ 0x822D4E40   (publish an "unload bundles" request event)
//   Update              @ 0x822ECC00   (pump replies + step every slot + flush requests)
//
// Member offsets are by NAME; the X360 absolute offsets (maEntries @ +0x1368 stride 24,
// mLoadedEntries @ +0x1360, mAudioCarLoadedDataQueue @ +0x1428, mpStreamer @ +0x15B8) are
// 32-bit-build facts and are NOT load-bearing for the host build's by-name access.
//
// The X360 inlines the per-slot debug prints (gated on the TU-local
// KI_PRINT_CAR_LOADING_STATES toggle, default off) through a dedicated StrStream; here
// they are routed through the engine's unified CgsDev::Log::gpDebugPrint sink, preserving
// the gate and the message text. The assert tripwires that streamed an index into the
// message buffer are reproduced with the house CGS_ASSERT carrying the static expression.
// ============================================================================

namespace BrnWorld
{

// BrnRaceCarComponentStreamers.cpp:25 (DWARF). Per-build debug toggle: when non-zero the
// streamer logs each load/unload request it emits. Default OFF (the X360 ships it 0).
int32_t KI_PRINT_CAR_LOADING_STATES = 0;

// ---------------------------------------------------------------------------
// @ 0x822ECA68. Default-construct the audio streamer: stash the owner, construct the
// outgoing audio-loaded queue, forward the base Construct (pool 6 == cars, assetset 2 ==
// sound, allow-failure false), then initialise all eight streaming-sound slots to IDLE
// with their slot index baked into miUserID. (Boot-trace EXECUTED.)
// ---------------------------------------------------------------------------
void RaceCarAudioStreamer::Construct( RaceCarStreamer* lpStreamer )
{
    CGS_ASSERT( lpStreamer != nullptr, "lpStreamer != NULL" );
    mpStreamer = lpStreamer;

    mAudioCarLoadedDataQueue.Construct();

    // Pool id 6 (the streaming-SOUND pool; the X360 Construct passes liPoolId == 6, asset
    // set 2 == E_ASSETSET_SOUND, allow-failure false). The memory-pool enum (PS3 ps3mem.h
    // E_POOL_SOUND) has no reconstructed b5-decomp home yet, so the X360 literal is used.
    static const s32 KI_POOL_SOUND = 6;
    RaceCarBaseComponentStreamer::Construct( KI_POOL_SOUND, false, BrnResource::E_ASSETSET_SOUND );

    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; ++liActiveRaceCar )
    {
        RaceCarStreamingSound& lEntry = maEntries[liActiveRaceCar];
        lEntry.mDesiredId        = 0;
        lEntry.mLoadedBundleId   = 0;
        lEntry.meState           = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_IDLE;
        lEntry.miUserID          = static_cast<s8>( liActiveRaceCar );
        lEntry.mbLoadedIsPlayer  = false;
        lEntry.mbDesiredIsPlayer = false;
    }
}

// ---------------------------------------------------------------------------
// @ 0x822A5730. Register the desired streaming-sound asset luAssetID for car luUserId.
// Asserts the slot is currently free and the id decodes to a "VEH_..." name. When the car
// is the player's, clears every other slot's desired-player flag first (only one player
// car exists at a time). Always returns true.
// ---------------------------------------------------------------------------
bool RaceCarAudioStreamer::AddEntry( CgsID luAssetID, u64 luUserId, bool lbIsPlayer )
{
    CGS_ASSERT( luUserId < static_cast<u64>( KI_MAX_ACTIVE_RACE_CARS ),
                "luUserId >= 0 && luUserId < static_cast<uint64_t>(KI_MAX_ACTIVE_RACE_CARS)" );

    RaceCarStreamingSound& lEntry = maEntries[luUserId];

    // The X360 streamed the racecar index into this assert's message buffer.
    CGS_ASSERT( lEntry.mDesiredId == 0,
                "Adding a streaming asset that for a racecar that already has one" );
    CGS_ASSERT( luAssetID != 0, "lAssetID" );

    char lacVehicleID[KI_CGSID_STRING_LEN];
    CgsIDUnCompress( luAssetID, lacVehicleID );
    CGS_ASSERT( strstr( lacVehicleID, "VEH_" ) != nullptr, "strstr(lacVehicleID, \"VEH_\")" );

    lEntry.mDesiredId = luAssetID;

    if( lbIsPlayer )
    {
        for( s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i )
        {
            maEntries[i].mbDesiredIsPlayer = false;
        }
        lEntry.mbDesiredIsPlayer = true;
    }
    else
    {
        lEntry.mbDesiredIsPlayer = false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// @ 0x822A58D0. True when car liActiveRaceCar's streaming sound has fully loaded AND
// attached. lAssetId/lbIsPlayer are validation-only (the X360 ignores them here).
// ---------------------------------------------------------------------------
bool RaceCarAudioStreamer::IsVehicleAssetLoaded( CgsID lAssetId, s32 liActiveRaceCar, bool lbIsPlayer )
{
    (void)lAssetId;
    (void)lbIsPlayer;

    CGS_ASSERT( liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS,
                "liActiveRaceCar >= 0 && liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    return maEntries[liActiveRaceCar].meState
               == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADEDANDATTACHED;
}

// ---------------------------------------------------------------------------
// @ 0x822C08C8. A streaming-sound BUNDLE finished loading for the car at current-list slot
// liListIndex. Map it back to the race-car index, set the loaded bit, record the loaded
// bundle id, then advance the slot LOADINGBUNDLES -> LOADEDBUNDLES. This is a FULL override
// of the base hook (it does NOT dispatch OnAssetLoaded -- the audio slot machine owns that).
// ---------------------------------------------------------------------------
void RaceCarAudioStreamer::OnLoadComplete( const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent, s32 liListIndex )
{
    const s32 liActiveRaceCar = static_cast<s32>( GetUserId( liListIndex ) );

    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    mLoadedEntries.SetBit( static_cast<u32>( liActiveRaceCar ) );

    RaceCarStreamingSound& lEntry = maEntries[liActiveRaceCar];

    CGS_ASSERT( lEntry.mLoadedBundleId == 0, "lpEntry->mLoadedBundleId == kCGSID_NULL" );

    lEntry.mLoadedBundleId = lpEvent->GetGameDataId();

    CGS_ASSERT( lEntry.meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADINGBUNDLES,
                "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADINGBUNDLES" );

    lEntry.mbLoadedIsPlayer = true;
    lEntry.meState          = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADEDBUNDLES;
}

// ---------------------------------------------------------------------------
// @ 0x822A55C0. A streaming-sound bundle finished UNLOADING for the car at current-list
// slot liListIndex. Validate the slot, clear its loaded bundle id + loaded-player flag,
// then advance UNLOADINGBUNDLES -> IDLE. Full override of the base hook.
// ---------------------------------------------------------------------------
void RaceCarAudioStreamer::OnUnloadComplete( const BrnResource::GameDataIO::UnloadGameDataResponse* lpEvent, s32 liListIndex )
{
    const s32 liActiveRaceCar = static_cast<s32>( GetUserId( liListIndex ) );

    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    RaceCarStreamingSound& lEntry = maEntries[liActiveRaceCar];

    CGS_ASSERT( lpEvent != nullptr, "lpEvent" );
    CGS_ASSERT( lEntry.mLoadedBundleId != 0, "lpEntry->mLoadedBundleId" );
    CGS_ASSERT( lEntry.mbLoadedIsPlayer, "lpEntry->mbLoadedIsPlayer" );
    CGS_ASSERT( lEntry.mLoadedBundleId == lpEvent->GetGameDataId(),
                "lpEntry->mLoadedBundleId == lpEvent->GetGameDataId()" );

    lEntry.mLoadedBundleId  = 0;
    lEntry.mbLoadedIsPlayer = false;

    CGS_ASSERT( lEntry.meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_UNLOADINGBUNDLES,
                "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_UNLOADINGBUNDLES" );

    lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_IDLE;
}

// ---------------------------------------------------------------------------
// @ 0x822D5080. Publish a "load these streaming-sound bundles" request for lpEntry onto the
// outgoing queue. Re-derives the bare vehicle id from the slot's desired id, resolves the
// VehicleListEntry through the owner's vehicle list, advances ATTACH -> ATTACHING, then
// enqueues an E_REQUEST_LOAD_DATA event. Returns the queue AddEvent result.
// ---------------------------------------------------------------------------
bool RaceCarAudioStreamer::SendLoadRequest( RaceCarStreamingSound* lpEntry )
{
    CGS_ASSERT( lpEntry != nullptr, "lpEntry" );
    CGS_ASSERT( lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACH,
                "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACH" );

    lpEntry->meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHING;

    // The desired id is the full "VEH_<name>" bundle id; strip the "VEH_" prefix back to the
    // bare model id used to key the vehicle list (the X360 decodes then re-compresses [+4]).
    char lacVehicleID[KI_CGSID_STRING_LEN];
    CgsIDUnCompress( lpEntry->mDesiredId, lacVehicleID );
    CGS_ASSERT( strstr( lacVehicleID, "VEH_" ) != nullptr, "strstr(lacVehicleID, \"VEH_\")" );

    const CgsID lModelId = CgsIDCompress( lacVehicleID + 4 );

    const BrnResource::VehicleList* lpVehicleList = mpStreamer->GetVehicleList();
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex( lModelId );
    CGS_ASSERT( liVehicleIndex != -1, "liVehicleIndex != -1" );

    const BrnResource::VehicleListEntry* lpVehicleEntry = lpVehicleList->GetVehicleData( liVehicleIndex );
    CGS_ASSERT( lpVehicleEntry != nullptr, "lpVehicleEntry != NULL" );

    RaceCarEntityModuleIO::AudioCarDataLoadedEvent lEvent;
    lEvent.meMessageType         = RaceCarEntityModuleIO::AudioCarDataLoadedEvent::E_REQUEST_LOAD_DATA;
    lEvent.mpVehicleListEntry    = lpVehicleEntry;
    lEvent.mAssetID              = lModelId;
    lEvent.miActiveRaceCarIndex  = static_cast<u8>( lpEntry->miUserID );
    lEvent.mbIsPlayer            = lpEntry->mbDesiredIsPlayer;

    if( KI_PRINT_CAR_LOADING_STATES )
    {
        *CgsDev::Log::gpDebugPrint << "RaceCarAudioStreamer::SendLoadRequest: "
                                   << static_cast<s32>( lpEntry->miUserID )
                                   << " Asset:" << lpEntry->mDesiredId << "\n";
    }

    return mAudioCarLoadedDataQueue.AddEvent( lEvent );
}

// ---------------------------------------------------------------------------
// @ 0x822D4E40. Publish an "unload these streaming-sound bundles" request for lpEntry onto
// the outgoing queue. Validates the slot owns a loaded bundle, advances DETACH -> DETACHING,
// then enqueues an E_REQUEST_UNLOAD_DATA event (carrying the LOADED ids/flags). Returns the
// queue AddEvent result.
// ---------------------------------------------------------------------------
bool RaceCarAudioStreamer::SendUnLoadRequest( RaceCarStreamingSound* lpEntry )
{
    CGS_ASSERT( lpEntry != nullptr, "lpEntry" );
    // The X360 streamed the racecar index into this assert's message buffer.
    CGS_ASSERT( lpEntry->mLoadedBundleId != 0, "Removing an already unloaded car." );

    char lacVehicleID[KI_CGSID_STRING_LEN];
    CgsIDUnCompress( lpEntry->mLoadedBundleId, lacVehicleID );
    CGS_ASSERT( strstr( lacVehicleID, "VEH_" ) != nullptr, "strstr(lacVehicleID, \"VEH_\")" );

    const CgsID lModelId = CgsIDCompress( lacVehicleID + 4 );

    CGS_ASSERT( lpEntry->mLoadedBundleId != 0, "lpEntry->mLoadedBundleId" );
    CGS_ASSERT( lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACH,
                "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACH" );

    lpEntry->meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHING;

    RaceCarEntityModuleIO::AudioCarDataLoadedEvent lEvent;
    lEvent.meMessageType         = RaceCarEntityModuleIO::AudioCarDataLoadedEvent::E_REQUEST_UNLOAD_DATA;
    lEvent.mpVehicleListEntry    = nullptr;
    lEvent.mAssetID              = lModelId;
    lEvent.miActiveRaceCarIndex  = static_cast<u8>( lpEntry->miUserID );
    lEvent.mbIsPlayer            = lpEntry->mbLoadedIsPlayer;

    if( KI_PRINT_CAR_LOADING_STATES )
    {
        *CgsDev::Log::gpDebugPrint << "RaceCarAudioStreamer::SendUnLoadRequest: "
                                   << static_cast<s32>( lpEntry->miUserID )
                                   << " Asset:" << lpEntry->mLoadedBundleId << "\n";
    }

    return mAudioCarLoadedDataQueue.AddEvent( lEvent );
}

// ---------------------------------------------------------------------------
// @ 0x822ECC00. One frame of the streaming-sound state machine.
//   1. Forward the base streamer Update.
//   2. Consume the input buffer's "data (un)loaded" replies, advancing the matching slot
//      ATTACHING -> ATTACHED or DETACHING -> DETACHED.
//   3. Step every one of the eight slots through the attach/detach machine.
//   4. Flush this frame's outgoing (un)load requests onto the output buffer and reset the
//      local queue.
// ---------------------------------------------------------------------------
void RaceCarAudioStreamer::Update( const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
                                   RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput )
{
    InternalBaseStreamer::Update();

    // ---- 2. drain the incoming "data is (un)loaded" replies --------------------------
    const AudioCarLoadedDataQueue* lpInQueue = lpInput->GetAudioCarLoadedDataQueue();
    for( s32 liEvent = 0; liEvent < lpInQueue->GetLength(); ++liEvent )
    {
        const RaceCarEntityModuleIO::AudioCarDataLoadedEvent& lEvent = lpInQueue->GetEvent( liEvent );

        const s32 liActiveRaceCar = lEvent.miActiveRaceCarIndex;
        CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

        RaceCarStreamingSound& lEntry = maEntries[liActiveRaceCar];

        if( lEvent.meMessageType == RaceCarEntityModuleIO::AudioCarDataLoadedEvent::E_DATA_IS_LOADED )
        {
            char lacAssetID[KI_CGSID_STRING_LEN];
            CgsIDConvertToString( lEvent.mAssetID, lacAssetID );

            if( !lEntry.mbLoadedIsPlayer )
            {
                lEntry.mLoadedBundleId = BrnResource::MakeVehicleId( lacAssetID );
            }

            CGS_ASSERT( lEntry.mLoadedBundleId == BrnResource::MakeVehicleId( lacAssetID ),
                        "lpEntry->mLoadedBundleId == BrnResource::MakeVehicleId(lacAssetID)" );
            CGS_ASSERT( lEntry.meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHING,
                        "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHING" );

            lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHED;
        }
        else if( lEvent.meMessageType == RaceCarEntityModuleIO::AudioCarDataLoadedEvent::E_DATA_IS_UNLOADED )
        {
            if( !lEntry.mbLoadedIsPlayer )
            {
                lEntry.mLoadedBundleId = 0;
            }

            CGS_ASSERT( lEntry.meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHING,
                        "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHING" );

            lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHED;
        }
        else
        {
            CGS_ASSERT( false, "We shouldn't be in this state at this time" );
        }
    }

    // ---- 3. step every slot through the attach/detach state machine -------------------
    for( s32 liActiveRaceCar = 0; liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS; ++liActiveRaceCar )
    {
        RaceCarStreamingSound& lEntry = maEntries[liActiveRaceCar];

        switch( lEntry.meState )
        {
        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_IDLE:
            if( lEntry.mDesiredId != 0 )
            {
                if( lEntry.mbDesiredIsPlayer )
                {
                    InternalBaseStreamer::AddEntry( lEntry.mDesiredId, false,
                                                    static_cast<u64>( lEntry.miUserID ) );
                    lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADINGBUNDLES;
                }
                else
                {
                    lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACH;
                }
            }
            break;

        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADINGBUNDLES:
        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHING:
        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHING:
        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_UNLOADINGBUNDLES:
            // In-flight states: waiting on an async (un)load reply -- nothing to drive.
            break;

        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADEDBUNDLES:
            CGS_ASSERT( lEntry.mbLoadedIsPlayer, "lpEntry->mbLoadedIsPlayer" );
            CGS_ASSERT( lEntry.mLoadedBundleId != 0, "lpEntry->mLoadedBundleId != kCGSID_NULL" );

            if( lEntry.mDesiredId == lEntry.mLoadedBundleId )
            {
                lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACH;
                SendLoadRequest( &lEntry );
            }
            else
            {
                lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHED;
            }
            break;

        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACH:
            SendLoadRequest( &lEntry );
            break;

        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHED:
            // Notify the owning streamer the audio component has loaded, then re-evaluate.
            OnAssetLoaded( lEntry.miUserID, nullptr );
            // fall through
        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADEDANDATTACHED:
            CGS_ASSERT( lEntry.mLoadedBundleId != 0, "lpEntry->mLoadedBundleId != kCGSID_NULL" );

            if( lEntry.mDesiredId == lEntry.mLoadedBundleId
                && lEntry.mbLoadedIsPlayer == lEntry.mbDesiredIsPlayer )
            {
                if( lEntry.mbLoadedIsPlayer && lEntry.mbDesiredIsPlayer )
                {
                    if( mpStreamer->HACK_IsWaitingForAudioAfterCarSelect() )
                    {
                        mpStreamer->HACK_SetWaitingForAudioAfterCarSelect( false );
                        mpStreamer->SetAudioLoadDataStatus( lEntry.miUserID, true );
                    }
                }
            }
            else
            {
                lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACH;
            }
            break;

        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACH:
            SendUnLoadRequest( &lEntry );
            break;

        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_DETACHED:
            lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_UNLOADBUNDLES;
            // fall through
        case RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_UNLOADBUNDLES:
            if( lEntry.mbLoadedIsPlayer )
            {
                InternalBaseStreamer::RemoveEntry( lEntry.mLoadedBundleId,
                                                   static_cast<u64>( lEntry.miUserID ) );
                lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_UNLOADINGBUNDLES;
            }
            else
            {
                lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_IDLE;
            }
            break;

        default:
            CGS_ASSERT( false, "Bad state" );
            break;
        }
    }

    // ---- 4. flush this frame's outgoing (un)load requests onto the output buffer -------
    lpOutput->GetAudioCarLoadedDataQueue()->Clear();
    lpOutput->GetAudioCarLoadedDataQueue()->Append( mAudioCarLoadedDataQueue );
    mAudioCarLoadedDataQueue.Clear();
}

// ---------------------------------------------------------------------------
// The two per-car notification hooks the base streamer dispatches into. They are not in
// this TU's boot-trace function set, but the audio streamer is instantiated by value by
// RaceCarStreamer, so the overrides are bodied here (forwarding to the owner, matching the
// graphics/physics/attribute/wheel-graphics streamers' identical shape).
// ---------------------------------------------------------------------------
void RaceCarAudioStreamer::OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent )
{
    (void)lpEvent;
    mpStreamer->OnAudioLoaded( liActiveRaceCar );
}

void RaceCarAudioStreamer::OnAssetUnloading( s32 liActiveRaceCar )
{
    mpStreamer->OnAudioUnloading( liActiveRaceCar );
}

} // namespace BrnWorld
