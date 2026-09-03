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

    RaceCarStreamingSound& lEntry = maEntries[liActiveRaceCar];
    CGS_ASSERT( lEntry.meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHED,
                "lpEntry->meState == RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_ATTACHED" );
    lEntry.meState = RaceCarStreamingSound::E_RACECARSTREAMINGSOUND_LOADEDANDATTACHED;
}

void RaceCarAudioStreamer::OnAssetUnloading( s32 liActiveRaceCar )
{
    mpStreamer->OnAudioUnloading( liActiveRaceCar );
}

// ---------------------------------------------------------------------------
// @ 0x822C0B60. Drop car luUserId's desired streaming sound.
//
// SIGNATURE RECOVERED FROM THE ASM, not from the PC declaration: the console form
// is RemoveEntry(CgsID lAssetId, u64 luUserId) returning bool, and the CALLER
// (RaceCarStreamer::AddVehicleData @0x822EBE18) reads the slot's own desired id at
// the call site -- `v47 = *(24*(i+207)+this+19920); RemoveEntry(HIDWORD(v47), v47)`
// -- so the two asserts below are validation of an invariant the caller maintains.
// The 1-argument overload underneath is the encapsulated form the PC call site uses;
// it re-derives exactly the id the console passed, so behaviour is identical.
//
// Body: assert the index (BrnRaceCarComponentStreamers.cpp:812), assert the slot HAS
// a desired id (:816) and that it matches the one being removed (:817), log under the
// KI_PRINT_CAR_LOADING_STATES gate, then clear mbDesiredIsPlayer and mDesiredId.
// NOTE it does NOT touch mLoadedBundleId / meState -- the slot's own state machine
// (Update) notices mDesiredId == 0 and drives the DETACH/unload sequence itself.
// ---------------------------------------------------------------------------
bool RaceCarAudioStreamer::RemoveEntry( CgsID lAssetId, u64 luUserId )
{
    CGS_ASSERT( luUserId < static_cast<u64>( KI_MAX_ACTIVE_RACE_CARS ),
                "luUserId >= 0 && luUserId < static_cast<uint64_t>(KI_MAX_ACTIVE_RACE_CARS)" );

    RaceCarStreamingSound& lEntry = maEntries[luUserId];

    // The X360 streamed the racecar index into both message buffers.
    CGS_ASSERT( lEntry.mDesiredId != 0,
                "Removing a streaming asset that hasn't been set, racecar=" );
    CGS_ASSERT( lEntry.mDesiredId == lAssetId,
                "Removing a different asset from a racecar from the one that was added, racecar=" );

    if( KI_PRINT_CAR_LOADING_STATES != 0 && CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint << "RaceCarAudioStreamer::RemoveEntry: " << luUserId
                                   << " Asset:" << lAssetId << "\n";
    }

    lEntry.mbDesiredIsPlayer = false;
    lEntry.mDesiredId        = 0;

    return true;
}

// The encapsulated 1-argument form the PC RaceCarStreamer call site uses (see the
// signature note above): it reads the slot's own desired id, which is precisely what
// the console open-codes at the call site.
void RaceCarAudioStreamer::RemoveEntry( s32 liActiveRaceCar )
{
    CGS_ASSERT( liActiveRaceCar >= 0, "liActiveRaceCar >= 0" );
    CGS_ASSERT( liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS, "liActiveRaceCar < KI_MAX_ACTIVE_RACE_CARS" );

    RemoveEntry( maEntries[liActiveRaceCar].mDesiredId, static_cast<u64>( liActiveRaceCar ) );
}

// The X360 inlines the audio leaf's Destruct into RaceCarStreamer::Destruct
// @0x822EBC98 exactly as it does the other four (clear the owner back-pointer);
// same shape as RaceCarGraphicsStreamer::Destruct below.
void RaceCarAudioStreamer::Destruct()
{
    mpStreamer = 0;
}

// ===========================================================================
// The four non-audio leaves.
//
// The X360 INLINED each of these Constructs into RaceCarStreamer::Construct
// @0x822F7FA0, so they have no standalone export -- but the inlined call sites are
// unambiguous. Reading that function's asm, each leaf is set up with
//   RaceCarBaseComponentStreamer::Construct(this, r4 = liPoolId,
//                                           r5 = lbAllowFailure, r6 = leAssetSet)
// followed by `stw r31, 0x1368(leaf)` (the mpStreamer back-pointer):
//
//   leaf @ streamer+0x0010 : r4=4     r5=0  r6=0  -> pool 4,  slot-pool false, GRAPHICS
//   leaf @ streamer+0x26F0 : r4=0x11  r5=1  r6=1  -> pool 17, SLOT-POOL true, PHYSICS   [r5 = lbSlotPoolSystem, corrected 2026-09-02]
//   leaf @ streamer+0x1380 : r4=0x11  r5=1  r6=4  -> pool 17, SLOT-POOL true, ATTRIBS
//   leaf @ streamer+0x3A60 : r4=4     r5=0  r6=0  -> pool 4,  slot-pool false, GRAPHICS
//   leaf @ streamer+0x4DD0 : RaceCarAudioStreamer::Construct (pool 6, SOUND -- above)
//
// The offset->member mapping is fixed independently by RaceCarEntityModule::
// UpdateStreaming @0x822FEFE0, whose four InternalBaseStreamer::Update calls go
// +0x1380, +0x26F0, +0x0010, +0x3A60 in that order -- which is exactly the DWARF
// member order this header declares for RaceCarStreamer::Update (attributes, physics,
// graphics, wheel graphics). So +0x1380 is the ATTRIBUTE streamer and +0x26F0 the
// PHYSICS one, and both share pool 17.
//
// Pool 4 is independently corroborated at RUNTIME: a probe that loaded
// 'Vehicles\VEH_PUSMC01_GR.bin' logged "-> pool 4: 273 resources", and pool 4's
// memory-map entry (BrnMemoryMapData.h) depends on pool 25 CarSharedPool, which is
// exactly where RaceCarEntityModule::LoadGlobalResources puts VEHICLETEX.BIN.
// ===========================================================================

namespace
{
    // The two pool ids the inlined Constructs pass. No reconstructed enum home exists for
    // the memory-pool ids yet (see the KI_POOL_SOUND note above), so the X360 literals
    // stand, named after their BrnMemoryMapData.h rows.
    const s32 KI_POOL_CAR       = 4;    // "CarPool"
    const s32 KI_POOL_CAR_PHYS  = 0x11; // 17
}

void RaceCarGraphicsStreamer::Construct( RaceCarStreamer* lpStreamer )
{
    CGS_ASSERT( lpStreamer != nullptr, "lpStreamer != NULL" );
    RaceCarBaseComponentStreamer::Construct( KI_POOL_CAR, false /*shared car pool*/, BrnResource::E_ASSETSET_GRAPHICS );
    mpStreamer = lpStreamer;
}

void RaceCarGraphicsStreamer::Destruct()
{
    mpStreamer = 0;
}

void RaceCarPhysicsStreamer::Construct( RaceCarStreamer* lpStreamer )
{
    CGS_ASSERT( lpStreamer != nullptr, "lpStreamer != NULL" );
    RaceCarBaseComponentStreamer::Construct( KI_POOL_CAR_PHYS, true /*slot-pool: pool 17 + slot*/, BrnResource::E_ASSETSET_PHYSICS );
    mpStreamer = lpStreamer;
}

void RaceCarPhysicsStreamer::Destruct()
{
    mpStreamer = 0;
}

void RaceCarAttributeStreamer::Construct( RaceCarStreamer* lpStreamer )
{
    CGS_ASSERT( lpStreamer != nullptr, "lpStreamer != NULL" );
    RaceCarBaseComponentStreamer::Construct( KI_POOL_CAR_PHYS, true /*slot-pool: pool 17 + slot*/, BrnResource::E_ASSETSET_ATTRIBS );
    mpStreamer = lpStreamer;
}

void RaceCarAttributeStreamer::Destruct()
{
    mpStreamer = 0;
}

void RaceCarWheelGraphicsStreamer::Construct( RaceCarStreamer* lpStreamer )
{
    CGS_ASSERT( lpStreamer != nullptr, "lpStreamer != NULL" );
    RaceCarBaseComponentStreamer::Construct( KI_POOL_CAR, false /*shared car pool*/, BrnResource::E_ASSETSET_GRAPHICS );
    mpStreamer = lpStreamer;
}

void RaceCarWheelGraphicsStreamer::Destruct()
{
    mpStreamer = 0;
}

// The four leaves' notification hooks. Same shape as the audio pair above: forward to the
// owning RaceCarStreamer, which folds the per-resource bit into maxLoadFlags[] and caches
// the pointer. The three that cache a resource take a ResourcePtr; the base's
// OnLoadComplete hands the whole GameDataAssetEvent down, and its mHandle is the only
// member carrying the acquired resource, so the ptr is bound from it.
// FLAG: the ResourcePtr construction is INFERRED from the base's call shape plus the
// console On*Loaded signatures (which take the ptr by value) -- these four leaves were
// inlined on the X360 and have no standalone asm to read it off.
void RaceCarGraphicsStreamer::OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent )
{
    mpStreamer->OnGraphicsLoaded( liActiveRaceCar,
                                  RaceCarStreamer::GraphicsResourcePtr( lpEvent->mHandle ) );
}

void RaceCarGraphicsStreamer::OnAssetUnloading( s32 liActiveRaceCar )
{
    mpStreamer->OnGraphicsUnloading( liActiveRaceCar );
}

void RaceCarPhysicsStreamer::OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent )
{
    mpStreamer->OnPhysicsLoaded( liActiveRaceCar,
                                 RaceCarStreamer::PhysicsResourcePtr( lpEvent->mHandle ) );
}

void RaceCarPhysicsStreamer::OnAssetUnloading( s32 liActiveRaceCar )
{
    mpStreamer->OnPhysicsUnloading( liActiveRaceCar );
}

void RaceCarAttributeStreamer::OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent )
{
    (void)lpEvent;   // the attribute set is registered with AttribSys, not cached here
    mpStreamer->OnAttributesLoaded( liActiveRaceCar );
}

void RaceCarAttributeStreamer::OnAssetUnloading( s32 liActiveRaceCar )
{
    mpStreamer->OnAttributesUnloading( liActiveRaceCar );
}

void RaceCarWheelGraphicsStreamer::OnAssetLoaded( s32 liActiveRaceCar, const BrnResource::GameDataIO::GameDataAssetEvent* lpEvent )
{
    mpStreamer->OnWheelGraphicsLoaded( liActiveRaceCar,
                                       RaceCarStreamer::WheelGraphicsResourcePtr( lpEvent->mHandle ) );
}

void RaceCarWheelGraphicsStreamer::OnAssetUnloading( s32 liActiveRaceCar )
{
    mpStreamer->OnWheelGraphicsUnloading( liActiveRaceCar );
}

// X360: RaceCarEntityModule::Prepare @0x82303E78's stage-3 tail stores the module's
// vehicle-list pointer into the streamer (`*(a1 + 69888) = *(a1 + 99380)`), and
// BrnRaceCarStreamer.cpp:87 asserts it is non-null right there. The wheel list is the
// second half of the DWARF signature.
void RaceCarStreamer::Prepare( const BrnResource::VehicleList* lpVehicleList,
                               const BrnResource::WheelList* lpWheelList )
{
    CGS_ASSERT( lpVehicleList != 0, "lpVehicleList != NULL" );   // X360 BrnRaceCarStreamer.cpp:87
    (void)lpWheelList;   // FLAG: the console stores only the vehicle list at this site
    mpVehicleList = lpVehicleList;
}

// @ 0x82302038. Drain the five component streamers' own GameData request rings onto the
// module output buffer's request interface. Each leaf's ring is at its object +0x18
// (InternalBaseStreamer::mGDRequestInterface, a RequestInterface<2048>), and the console
// appends them in the SAME order UpdateStreaming pumps them: attributes (+0x1398),
// physics (+0x2708), graphics (+0x28), wheel graphics (+0x3A78), audio (+0x4DE8).
//
// ⭐ This is load-bearing, not bookkeeping: InternalBaseStreamer::Update CLEARS its own
// request ring at the top of every frame, so a load request that is not drained in the
// same frame it was posted never reaches the GameData module.
void RaceCarStreamer::AppendGameDataRequests(
        RaceCarEntityModuleIO::ResourceRequestInterface* lpInterface ) const
{
    CGS_ASSERT( lpInterface != 0, "lpInterface" );   // X360 BrnRaceCarStreamer.cpp:378

    if( lpInterface == 0 )
    {
        return;
    }

    lpInterface->Append( *mAttributeStreamer.GetGameDataRequestInterface() );
    lpInterface->Append( *mPhysicsStreamer.GetGameDataRequestInterface() );
    lpInterface->Append( *mGraphicsStreamer.GetGameDataRequestInterface() );
    lpInterface->Append( *mWheelGraphicsStreamer.GetGameDataRequestInterface() );
    lpInterface->Append( *mAudioStreamer.GetGameDataRequestInterface() );
}

} // namespace BrnWorld
