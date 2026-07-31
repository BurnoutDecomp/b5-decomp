// ============================================================================
// BrnWorld::RaceCarEntityModule -- the race-car entity module (file TU).
//
// This is the CgsEntityModule/ModuleSingleBuffered subclass that owns and ticks
// the player/rival race-car fleet through the scene-update interface; it is the
// spine of the race-car subsystem. The X360 TU declares 81 methods (the lifecycle
// Construct/Prepare/Release/Destruct spine, the Pre/Post Scene & Physics update
// pumps, the streaming/AI/network handlers, the render/corona submission, and the
// many per-car update passes).
//
// FOUNDATION SCOPE OF THIS .cpp
// ----------------------------------------------------------------------------
// The owning type is homed in BrnRaceCarEntityModule.h as OPAQUE attested-offset
// storage (the full ~100KB object embeds ~25 not-yet-homed aggregates: the
// streamer, boost/near-miss/crash-play/power-parking managers, WorldMap2D, the
// replay serialiser, the RaceCar[35]/ActiveRaceCar[8] arrays, RNGs, etc.). Of the
// 81 functions, the overwhelming majority either:
//   * reach into those un-homed RaceCar / ActiveRaceCar / *Manager interiors, or
//   * call un-homed sibling methods, or
//   * are multi-stage VMX (lvx128/stvx128) physics-integration / transform /
//     visibility pipelines, or
//   * depend on un-recoverable rodata lookup tables.
// Per the project anti-fabrication rule, those are DECLARATION-ONLY and FLAGGED
// here (and in the header); they are NOT paraphrased to scalar and NOT given
// invented bodies. They belong to later race-car-interior / VMX passes once the
// member aggregates are themselves homed.
//
// BODIED HERE (fully asm-grounded, touch only the module's own scalar tail at
// X360-attested, DWARF-named offsets -- no un-homed interior reach, no VMX, no
// fabricated rodata):
//   RaceCarEntityModule::AddTrainingRequest   X360 0x822A47A8
//   RaceCarEntityModule::UpdateTailgateTimer  X360 0x822CE508
//
// (The self-contained player-scoring map + GetGameModeFlag slice was bodied in a
// prior work item in BrnRaceCarEntityModule_ScoringMapping.cpp.)
//
// See the FLAG INVENTORY block at the bottom for the per-function disposition of
// the remaining 79 methods.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "SharedClasses/Progression/BrnTrainingTypes.h"   // BrnProgression::ETrainingType

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // OutputBuffer_Prepare
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"                    // CgsResource::ResourceHandle
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"                  // AcquireResourceRequest / *Response
#include "GameShared/GameClasses/System/Resource/CgsResourceId.h"                        // CgsResource::ID::HashString
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // CgsDev::Log::gpDebugPrint
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"                               // BrnWorld::GlobalColourPalette
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"                              // GameDataAssetEvent (reply shape)

#include <cstring>   // memset

namespace BrnWorld
{

// X360 0x822A47A8. Append one training request to the per-frame ring.
//
// The X360 reads miPendingRequestCount (this+0x18394), asserts it is below the
// ring depth and that the incoming type is in range, then -- only while the ring
// is not full -- stores the 32-bit enum at mePendingTrainingRequestQueue[count]
// (this + 0x18374 + count*4, asm `stwx`) and bumps the count. If the ring is full
// the append is silently dropped (the guarded `if (count < 8)`).
//
// Both asserts are non-fatal (the X360 falls through after EndAssert), matching
// the project CGS_ASSERT semantics. The upper bound on the type is
// E_TRAINING_TYPE_COUNT (256) -- the literal the asm compares against (`cmpwi 0x100`).
void RaceCarEntityModule::AddTrainingRequest(BrnProgression::ETrainingType leTrainingType)
{
    CGS_ASSERT(miPendingRequestCount < KI_TRAINING_REQUEST_QUEUE_SIZE,
               "miPendingRequestCount < KI_TRAINING_REQUEST_QUEUE_SIZE");
    CGS_ASSERT(leTrainingType >= 0 && leTrainingType < BrnProgression::E_TRAINING_TYPE_COUNT,
               "leTrainingType >= 0 && leTrainingType < BrnProgression::E_TRAINING_TYPE_COUNT");

    if (miPendingRequestCount < KI_TRAINING_REQUEST_QUEUE_SIZE)
    {
        mePendingTrainingRequestQueue[miPendingRequestCount] = leTrainingType;
        ++miPendingRequestCount;
    }
}

// X360 0x822CE508. Advance the player's continuous-tailgating timer.
//
// The X360 calls IsPlayerCarTailgatingOtherRaceCars (passing the player active-car
// index loaded from this+0x182F8 and &maActiveRaceCars[0] == this+0x1A60); if it
// returns true it accumulates lfDeltaTime into mfCurrentTailgateDuration
// (this+0x182F0), otherwise it resets the duration to 0.0f. The predicate's boolean
// result is returned. The sibling query reaches the un-homed ActiveRaceCar interior
// and so is declaration-only (FLAGGED in the header).
bool RaceCarEntityModule::UpdateTailgateTimer(f32 lfDeltaTime)
{
    const EActiveRaceCarIndex lePlayerActiveRaceCarIndex = mePlayerActiveRaceCarIndex;

    const bool lbTailgating =
        IsPlayerCarTailgatingOtherRaceCars(lePlayerActiveRaceCarIndex,
                                           GetActiveRaceCar(E_ACTIVE_RACE_CAR_INDEX_0));

    if (lbTailgating)
    {
        mfCurrentTailgateDuration += lfDeltaTime;
    }
    else
    {
        mfCurrentTailgateDuration = 0.0f;
    }

    return lbTailgating;
}

// ============================================================================
// The module's own global-resource load + the Prepare stage machine that drives it.
// ============================================================================

namespace
{
    // The GameData reply ids LoadGlobalResources waits on (the X360 case immediates it
    // compares the queue record's type against: 0x34 == 52 GetVehicleList,
    // 0x3B == 59 GetWheelList).
    const s32 KI_REPLY_ID_GET_VEHICLE_LIST = 52;
    const s32 KI_REPLY_ID_GET_WHEEL_LIST   = 59;

    // X360 `li r27,5` / `li r6,0x19` -- the two pool ids LoadGlobalResources names.
    // 5  == the resident game-data pool the vehicle/wheel lists and CarColours live in.
    // 25 == "CarSharedPool" (BrnMemoryMapData.h), the shared vehicle-texture pool every
    //       per-car graphics bundle imports from.
    const s32 KI_POOL_GAMEDATA   = 5;
    const s32 KI_POOL_CAR_SHARED = 25;

    // Read the head of a receiver queue as a GameData reply, or 0 when it is empty.
    // (The X360 open-codes this as buffer + miStartOffset + 8 with a `count > 0` guard.)
    const BrnResource::GameDataIO::GameDataAssetEvent*
    PeekGameDataReply( const CgsModule::BaseEventReceiverQueue& lrQueue, s32* lpiReplyId )
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        const s32 liType = lrQueue.GetFirstEvent( &lpEvent, &liSize );
        if( lpEvent == 0 )
        {
            *lpiReplyId = -1;
            return 0;
        }
        *lpiReplyId = liType;
        return reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>( lpEvent );
    }
}

// The module's Construct. The X360 body (reached by WorldModule::Construct's fleet
// cascade) brings up the whole interior; the slice bodied here is the state
// LoadGlobalResources owns -- without the receiver-queue Construct the queue has no
// backing buffer, so BaseEventReceiverQueue::AddEvent rejects every GameData reply
// addressed to it and Prepare stage 2 waits forever.
// [FLAG PC bring-up] the rest of the interior (streamer, boost/near-miss/crash-play/
// power-parking managers, WorldMap2D, the RaceCar[35]/ActiveRaceCar[8] arrays) is still
// opaque storage in this header and is NOT constructed here.
void RaceCarEntityModule::Construct()
{
    mePrepareStage             = 0;
    miPrepareCarIndex          = 0;
    meLoadGlobalResourcesStage = 0;
    miExpectedResponseCount    = 0;

    mReceiverQueue.Construct();

    mpVehicleList     = 0;
    mpWheelList       = 0;
    mbCarColoursBound = false;
}

// X360 0x82300730. The module's resumable global-resource load, driven from Prepare
// stage 2 under the output buffer's write lock. Four steps, each "publish a request
// naming mReceiverQueue, then yield until the reply arrives":
//
//   0/1 : AcquireResourceRequest (type 4, 24 B) for ID::HashString("CarColours"),
//         pool 5 -> mReceiverQueue.Clear()
//   2   : wait; bind mCarColoursResource from the reply's handle
//   3   : RequestInterface<8192>::LoadBundle(&mReceiverQueue, 0, 25,
//         "Vehicles/VEHICLETEX.BIN", false) -> mReceiverQueue.Clear()
//   4   : wait
//   5   : RequestInterface<8192>::GetVehicleList(&mReceiverQueue, 0) -> Clear()
//   6   : wait; assert the reply id is 52 and its fail flag is clear; take mpVehicleList
//   9   : RequestInterface<8192>::GetWheelList(&mReceiverQueue, 0) -> Clear()
//   10  : wait; assert the reply id is 59 and its fail flag is clear; take mpWheelList
//   0x11: done
//
// Every wait compares the queue's event count against miExpectedResponseCount (always
// staged to 1) and returns false while short. The two asserts are the console's
// "Invalid event id received\n" pair (BrnRaceCarEntityModule.cpp:6408/6416 and
// :6449/:6457) and are non-gating there, so they are non-gating here.
//
// [FLAG PC bring-up] The console's stage 0/1 posts the CarColours acquire through the
// SAME output request interface, as a raw type-4 AcquireResourceRequest built inline
// (AddEvent(queue, record, 4, 24)); reproduced literally below rather than through
// RequestInterface::AcquireResource, whose single attested call site packs the pool id
// into the resource id's top nibble -- a shape this call site does not use.
bool RaceCarEntityModule::LoadGlobalResources( RaceCarEntityModuleIO::OutputBuffer_Prepare* lpOutput )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // X360 :6324
    if( lpOutput == 0 )
        return false;

    RaceCarEntityModuleIO::ResourceRequestInterface* lpRequests =
        lpOutput->GetResourceRequestInterface();

    switch( meLoadGlobalResourcesStage )
    {
    case 0:
    case 1:
    {
        meLoadGlobalResourcesStage = 1;
        miExpectedResponseCount    = 1;

        CgsResource::Events::AcquireResourceRequest lRequest;
        memset( &lRequest, 0, sizeof( lRequest ) );
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = 0;
        lRequest.miPoolId  = KI_POOL_GAMEDATA;
        lRequest.mResourceId.SetHash( static_cast<u64>( static_cast<u32>(
            CgsResource::ID::HashString( reinterpret_cast<const u8*>( "CarColours" ) ) ) ) );
        lRequest.mbCheckRefCount = false;

        // X360 posts 24 bytes -- its 32-bit sizeof(AcquireResourceRequest). On x64 the
        // record is wider, and the consumer reads it back by NAME, so the host must copy
        // the whole struct (same convention as every committed GameDataModule request).
        lpRequests->mRequestQueue.AddEvent(
            reinterpret_cast<const CgsModule::Event*>( &lRequest ),
            4 /*AcquireResource*/, static_cast<s32>( sizeof( lRequest ) ) );
        mReceiverQueue.Clear();
    }
    // fall through
    case 2:
    {
        meLoadGlobalResourcesStage = 2;
        if( mReceiverQueue.GetCount() < miExpectedResponseCount )
            return false;

        s32 liReplyId = -1;
        const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
            PeekGameDataReply( mReceiverQueue, &liReplyId );
        if( lpReply != 0 )
        {
            CgsResource::ResourcePtr<GlobalColourPalette> lPalette( lpReply->mHandle );
            mCarColoursResource  = lPalette;
            mbCarColoursBound    = ( lpReply->mHandle.mpResourceMemory != 0 );
        }
    }
    // fall through
    case 3:
        meLoadGlobalResourcesStage = 3;
        miExpectedResponseCount    = 1;
        lpRequests->LoadBundle( &mReceiverQueue, 0, KI_POOL_CAR_SHARED,
                                "Vehicles/VEHICLETEX.BIN", false );
        mReceiverQueue.Clear();
    // fall through
    case 4:
        meLoadGlobalResourcesStage = 4;
        if( mReceiverQueue.GetCount() < miExpectedResponseCount )
            return false;
    // fall through
    case 5:
        meLoadGlobalResourcesStage = 5;
        miExpectedResponseCount    = 1;
        lpRequests->GetVehicleList( &mReceiverQueue, 0 );
        mReceiverQueue.Clear();
    // fall through
    case 6:
    {
        meLoadGlobalResourcesStage = 6;
        if( mReceiverQueue.GetCount() < miExpectedResponseCount )
            return false;

        s32 liReplyId = -1;
        const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
            PeekGameDataReply( mReceiverQueue, &liReplyId );
        CGS_ASSERT( lpReply != 0 && liReplyId == KI_REPLY_ID_GET_VEHICLE_LIST,
                    "Invalid event id received\n" );          // X360 :6408
        if( lpReply != 0 )
        {
            CGS_ASSERT( !lpReply->mbFailFlag, "Invalid event id received\n" );   // X360 :6416
            mpVehicleList = static_cast<const BrnResource::VehicleList*>(
                lpReply->mHandle.mpResourceMemory );
        }
    }
    // fall through
    case 9:
        meLoadGlobalResourcesStage = 9;
        miExpectedResponseCount    = 1;
        lpRequests->GetWheelList( &mReceiverQueue, 0 );
        mReceiverQueue.Clear();
    // fall through
    case 10:
    {
        meLoadGlobalResourcesStage = 10;
        if( mReceiverQueue.GetCount() < miExpectedResponseCount )
            return false;

        s32 liReplyId = -1;
        const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
            PeekGameDataReply( mReceiverQueue, &liReplyId );
        CGS_ASSERT( lpReply != 0 && liReplyId == KI_REPLY_ID_GET_WHEEL_LIST,
                    "Invalid event id received\n" );          // X360 :6449
        if( lpReply != 0 )
        {
            CGS_ASSERT( !lpReply->mbFailFlag, "Invalid event id received\n" );   // X360 :6457
            mpWheelList = static_cast<const BrnResource::WheelList*>(
                lpReply->mHandle.mpResourceMemory );
        }

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[RaceCar] LoadGlobalResources done: VEHICLETEX -> pool "
                << KI_POOL_CAR_SHARED
                << ", vehicleList=" << ( mpVehicleList != 0 ? 1 : 0 )
                << " wheelList=" << ( mpWheelList != 0 ? 1 : 0 )
                << " carColours=" << ( mbCarColoursBound ? 1 : 0 ) << "\n";
        }
    }
    // fall through
    case 0x11:
        meLoadGlobalResourcesStage = 0x11;
        return true;

    default:
        CGS_ASSERT( false, "Invalid load stage\n" );   // X360 :6471
        return false;
    }
}

// X360 0x82303E78. Prepare's four-stage machine (the stage word is this+0x22C).
//
//   0 : one-time bring-up -- WorldMap2D::Construct against the district-map resource,
//       BoostManager::Prepare, the near-miss/crash-play/power-parking scalar reset block,
//       PowerParkingManager::Prepare and three DebugComponent::Register calls.
//   1 : ModuleSingleBuffered::Prepare (the module base)
//   2 : LockForWrite(lpOutput); LoadGlobalResources; UnlockForWrite
//   3 : RaceCar::Prepare x35, ActiveRaceCar::Prepare x8, the two per-module RNG seeds,
//       the identity transform + tail scalar block, ClearAllActiveRaceCarToPlayer-
//       ScoringMappings, then hand mpVehicleList to the streamer
//       (BrnRaceCarStreamer.cpp:87 asserts it is non-null).
//
// [FLAG PC bring-up] Stages 0 and 3 are NOT reproduced: every call they make reaches an
// interior this header still models as opaque storage (WorldMap2D / BoostManager /
// PowerParkingManager, and the RaceCar[35] / ActiveRaceCar[8] arrays are size-only
// placeholders here). They are deliberately skipped rather than paraphrased. Stage 2 --
// the only stage that talks to the outside world, and the one that owns
// Vehicles/VEHICLETEX.BIN and the two data-list pointers -- is real.
bool RaceCarEntityModule::Prepare( RaceCarEntityModuleIO::OutputBuffer_Prepare* lpOutput,
                                   const CgsResource::ResourceHandle& lrDistrictMapHandle )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // X360 :663
    (void)lrDistrictMapHandle;

    if( CgsDev::Log::gpDebugPrint != 0 )
        *CgsDev::Log::gpDebugPrint << "[RaceCar] Prepare stage " << mePrepareStage
                                   << " load " << meLoadGlobalResourcesStage
                                   << " replies " << mReceiverQueue.GetCount() << "\n";

    switch( mePrepareStage )
    {
    case 0:
        // [FLAG PC bring-up] stage 0 skipped -- see the note above.
        mePrepareStage = 0;
    // fall through
    case 1:
        mePrepareStage = 1;
        // [FLAG PC bring-up] ModuleSingleBuffered::Prepare is not reachable from this
        // minimal header slice (the base is inside maPrecedingState); the console gate is
        // "advance when it reports done", which is what falling through does.
    // fall through
    case 2:
    {
        mePrepareStage = 2;
        lpOutput->LockForWrite();
        const bool lbLoaded = LoadGlobalResources( lpOutput );
        lpOutput->UnlockForWrite();
        if( !lbLoaded )
            return false;
    }
    // fall through
    case 3:
        // The console re-arms the stage to 1 here and runs the per-car Prepare sweep.
        // [FLAG PC bring-up] the sweep is skipped (opaque RaceCar/ActiveRaceCar storage);
        // the stage is parked at 3 so a re-entry is idempotent rather than replaying
        // LoadGlobalResources.
        mePrepareStage         = 3;
        miPrepareCarIndex      = 0;
        return true;

    default:
        CGS_ASSERT( false, "Invalid Stage\n" );   // X360 :776
        return false;
    }
}

// ============================================================================
// FLAG INVENTORY -- the 79 functions NOT bodied in this foundation pass.
//
// Each is in the X360 TU postmortem dossier; none is bodied here because doing so
// honestly requires homing the embedded race-car/manager aggregates and/or a
// dedicated VMX pass. Categories:
//
// [VMX]      multi-stage lvx128/stvx128 pipelines (transform/visibility/integrate):
//   Construct, Destruct, GenerateDispatchLists, RenderRaceCar, SubmitCoronasForRaceCar,
//   PreSceneUpdate, PostSceneUpdate, PrePhysicsUpdate, PostPhysicsUpdate,
//   UpdateActiveRaceCarTransforms, UpdateActiveRaceCarColours, ReadUpdatedActiveRaceCarDataFromPhysics,
//   WriteUpdatedAIData, ReadOutOfRangeRaceCarDataFromAI, UpdateOutputInterfaces,
//   ResetActiveRaceCar, AttachActiveRaceCar, OnRaceCarResourcesLoaded, AddRivalCar,
//   AddRaceCarToStartingGridOrFreeburnLobby, SetUpAIForMode, SetUpPlayerCarForMode,
//   SetupOpponents, HandlePrepareForModeAction, HandleResetPlayerCarAction,
//   HandleStopModeAction, HandleGameActions, ProcessPlayerVehicleInput,
//   ProcessCreateVehicleEvents, ProcessRaceCarCrashCompleteEvents, ProcessResetOnTrackResultQueue,
//   UpdateBoost, UpdateNearMisses, UpdateInAndOutOfRangeCars, UpdateSerialiser,
//   UpdateReplayStreaming, CheckForResetOnTrackConditions, DebugRenderPosition (38 total).
//
// [INTERIOR] reach un-homed RaceCar/ActiveRaceCar/*Manager/Streamer/BrnAI/BrnTraffic
//            interiors or call un-homed sibling methods:
//   Release-adjacent state writes aside, this covers: DetachActiveRaceCar,
//   ProcessTakedownEvents, ProcessPropContactQueue, ProcessLeapedAndStompedCars,
//   ProcessPowerParking, ProcessRaceCarCrashEvents_PostPhysics, UpdatePowerParking,
//   UpdateCrashingPlayerContacts, UpdateCurrentWorldRegion, UpdateHidingEvents,
//   UpdateRaceCars_PreScene, UpdatePropBoundingBoxes_PreScene, UpdateRaceCarContacts,
//   UpdateActiveCars, UpdateDisconnectedPlayers, UpdateTrafficAndRaceCarNearMisses,
//   SendGameEvents, SendStreamerEvents, SendRaceCarSceneUpdates, SendAddedForCollisionStateToPhysics,
//   SetAllActiveCarsInGameMode, SetAllCarsOnStartLine, SetupCarColour, SetHiddenDelay,
//   RemoveRivals, RemoveAllRivalsFromWorld, RemoveAllNetworkCarsFromWorld, RemoveAllRaceCars,
//   ChangePlayerCarColour, GetDamagedCarCount, GetPersistentDamageCarCount,
//   EnterReplay, LeaveReplay, LoadGlobalResources, IsCarColourInUse,
//   IsPlayerCarTailgatingOtherRaceCars, UpdatePowerParking (Pre/Post variants),
//   UpdateReplayStreaming-adjacent helpers (28+ total).
//
// [RODATA]   depend on an un-recovered rodata lookup table (NEVER fabricated):
//   HandleCarTypeTrainingMessage (dword_82CDB4A4: ECarType->ETrainingType, 3 cells),
//   GetRandomCarColour (palette tables), AddTrainingRequest's callers' tables.
//
// [LIFECYCLE-tail] Release: fully scalar but its mid/far-tail offsets (the sim-time
//   reset block @+0x180D8.., the streamer/manager stage flags) map to DWARF members
//   whose specific per-offset names are not unambiguously pinnable from this TU's asm
//   alone; bodying it would require homing those members. Left declaration-only to
//   avoid fabricating member names. (Construct/Destruct are additionally [VMX].)
// ============================================================================

}   // namespace BrnWorld
