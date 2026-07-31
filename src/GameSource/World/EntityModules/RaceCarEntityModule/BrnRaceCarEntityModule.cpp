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
#include "SharedClasses/DataLists/VehicleList.h"                                         // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"                                    // BrnResource::VehicleListEntry
#include "SharedClasses/DataLists/WheelList.h"                                           // BrnResource::WheelList / WheelListEntry

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

    // X360: the module Construct brings the streamer up along with the rest of the
    // interior (the receiver queue and the streamer are adjacent members, +0x100E8 and
    // +0x11100). Without this the five component streamers have no request/receiver
    // queues, so no AddEntry can ever reach a request ring.
    mRaceCarStreamer.Construct();
    mfTimeStep = 0.0f;

    // X360 0x822FD898: the two per-slot Construct sweeps, `ActiveRaceCar::Construct(i)`
    // x8 (asm 0x822FDBxx) and `RaceCar::Construct(i)` x35. Both arrays are real members
    // now, so both sweeps are restored (pose wave 2026-07-31). ActiveRaceCar::Construct
    // is what stores meActiveRaceCarIndex, which Attach then uses for the car's
    // handling-body VolumeInstanceId, and RaceCar::Construct is what sets
    // miGlobalRaceCarIndex -- AddToWorld asserts on it ("Using unprepared RaceCar").
    for( s32 liActive = 0; liActive < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActive )
    {
        maActiveRaceCars[liActive].Construct( static_cast<EActiveRaceCarIndex>( liActive ) );
    }
    for( s32 liGlobal = 0; liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liGlobal )
    {
        maRaceCars[liGlobal].Construct( static_cast<EGlobalRaceCarIndex>( liGlobal ) );
    }

    mbIsInGameMode            = false;
    mbInCarSelectScreen       = false;
    mbCarSelectDontStreamAudio = false;

    mpVehicleList        = 0;
    mpWheelList          = 0;
    mbCarColoursBound    = false;
    mbBringUpCarRequested = false;

    // The three render switches the dispatch leg reads (see the header for the offset
    // fit). The console seeds them from its debug-variable table, which is not live on
    // this build; both body and coronas default ON.
    mbRenderCarsDuringCrash = true;
    mbRenderRaceCarCoronas  = true;

    // [FLAG PC bring-up] mbRenderWheels OFF. RenderRaceCar's wheel block is not
    // reconstructed: its only submission path is
    // CgsGraphics::Model::SetupShaderConstantsForInstancing (absent from the tree) and its
    // draw leaf DrawInstancedIndexedPrimitive_Custom is an explicit CGS_ASSERT(false) trap
    // (CgsDispatcherCommands.cpp:1045 / :1177). Set true when both land.
    mbRenderWheels = false;
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
        // The console re-arms the stage to 1 here and runs the per-car Prepare sweep --
        // RaceCar::Prepare x35 then ActiveRaceCar::Prepare x8 (asm 0x82304xxx). Both are
        // real bodies now, so the sweep is restored (pose wave 2026-07-31); the stage is
        // parked at 3 so a re-entry is idempotent rather than replaying
        // LoadGlobalResources.
        for( s32 liGlobal = 0; liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liGlobal )
        {
            maRaceCars[liGlobal].Prepare();
        }
        for( s32 liActive = 0; liActive < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActive )
        {
            maActiveRaceCars[liActive].Prepare();
        }
        //
        // What IS reproduced is the stage-3 TAIL: the console stores the module's vehicle-list
        // pointer into the streamer (`*(a1 + 69888) = *(a1 + 99380)`), which is
        // RaceCarStreamer::Prepare -- the assert at BrnRaceCarStreamer.cpp:87 sits right there.
        mePrepareStage         = 3;
        miPrepareCarIndex      = 0;

        if( mpVehicleList != 0 )
        {
            mRaceCarStreamer.Prepare( mpVehicleList, mpWheelList );
        }

        return true;

    default:
        CGS_ASSERT( false, "Invalid Stage\n" );   // X360 :776
        return false;
    }
}

// ============================================================================
// THE PER-FRAME STREAMING PUMP (race-car streamer wave 2026-07-31).
//
// This is the leg that turns the (already faithful, already mounted) RaceCarStreamer
// stack from orphaned code into a live subsystem. Three functions:
//   PreSceneUpdate     @0x8230D928 -- partial slice; latches mfTimeStep, calls UpdateStreaming
//   UpdateStreaming    @0x822FEFE0 -- full body except the ActiveRaceCar sweep (see below)
//   PostPhysicsUpdate  @0x82307538 -- partial slice; calls SendStreamerEvents
//   SendStreamerEvents @0x82304F70 -- full body
// ============================================================================

// X360 0x822FEFE0 (DWARF BrnRaceCarEntityModule.h:563).
//
// The head of the console body is a verbatim INLINE of RaceCarStreamer::Update
// @0x822F80C0 -- the same four InternalBaseStreamer::Update calls in the same order
// (attributes +0x1380, physics +0x26F0, graphics +0x10, wheel graphics +0x3A60), then
// RaceCarAudioStreamer::Update, then `mfTimeSinceLastLoad += mfTimeStep` (streamer
// +0x6740 += module +0x18398), then RaceCarStreamer::UpdateDesiredCars. Written here as
// the one call the inline came from.
//
// The tail is a sweep of the eight active-car slots:
//     if (GetActiveRaceCar(i)->muState == 1 /* waiting for load */) {
//         if (mRaceCarStreamer.IsRaceCarLoaded(i) &&
//             (car->IsPlayer() || !car->IsOnRaceStartState(0)))
//                  OnRaceCarResourcesLoaded(i, lpOutput->GetVehicleInputInterface());
//         else     lbAllLoaded = false;
//     }
// followed by the car-select wait latches (this+99168..99175, gated on
// mbInCarSelectScreen at +0x186C9), the streaming-complete publish
// (lpOutput[10337] = lbAllLoaded) and the "streaming finished" edge
// (this[99144] -> this[99185]).
//
// The RESOURCE-COMPLETE sweep IS reproduced now that the ActiveRaceCar interior is homed
// (pose wave 2026-07-31). What still is not, and is FLAGGED rather than paraphrased:
//   * OnRaceCarResourcesLoaded @0x822FEBF8 -- it needs ActiveRaceCar::OnResourcesLoaded
//     (which dereferences the vehicle's PHYSICS def for the centre-of-mass transform),
//     SetupCarColour and PlaceRaceCarOnLoad. It is UNREACHABLE on this build anyway:
//     IsRaceCarLoaded() demands all five resource bits and VEH_<id>_AT.bin (attributes +
//     physics) is not ported, so the predicate is constant-false. Left as an explicit
//     call site guarded by the console's own condition so the moment the data lands the
//     only missing piece is that one function.
//   * the car-select wait latches (this+99168..99175) and the streaming-complete publish
//     (lpOutput[10337]) -- neither member is modelled.
void RaceCarEntityModule::UpdateStreaming(
        const RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );   // X360 :4241

    SpawnFirstUnlockedCarBringUp();

    mRaceCarStreamer.Update( lpInput, lpOutput, mfTimeStep );

    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );

        if( !lpActiveRaceCar->IsWaitingForLoad() )
        {
            continue;
        }

        if( mRaceCarStreamer.IsRaceCarLoaded( liCar )
            && ( lpActiveRaceCar->IsPlayer()
              || !lpActiveRaceCar->IsOnRaceStartState(
                     ActiveRaceCar::E_RACE_START_STATE_ON_START_LINE ) ) )
        {
            // [FLAG PC bring-up] OnRaceCarResourcesLoaded @0x822FEBF8 goes here and is not
            // reconstructed -- see the banner. Unreachable on this build (IsRaceCarLoaded
            // is constant-false while VEH_<id>_AT.bin is unported), so nothing is silently
            // skipped today.
        }
        else
        {
            // Console: `lbAllLoaded = false` (the flag feeds the streaming-complete
            // publish, whose output member is not modelled here).
            // [FLAG PC bring-up] and this is where the promote stands in -- see its banner.
            PromoteAttachedCarToActiveBringUp( lpActiveRaceCar );
        }
    }
}

// ============================================================================
// [FLAG PC bring-up] PromoteAttachedCarToActiveBringUp -- NOT an X360 function.
//
// THE ONE REMAINING FICTION IN THE RACE-CAR RENDER PATH, and it is worth reading in full
// because everything around it is now console code.
//
// The console walks E_STATE_ATTACHED -> E_STATE_WAITING -> E_STATE_ACTIVE through exactly
// two functions, and NEITHER is reachable on this build:
//
//   ActiveRaceCar::OnResourcesLoaded @0x822EB168   (muState = 2)
//       not reconstructed, AND gated on RaceCarStreamer::IsRaceCarLoaded() == all five
//       resource bits. As of 2026-08-01 three of the five arrive (LOADEDGFX,
//       LOADEDPHYSICS, LOADEDATTRS -- the VEH_<id>_AT.BIN port landed and the streamed
//       vault allocator now hands out slots); LOADEDWHEELGFX still waits on the deferred
//       `LoadWheel` GameData handler (id 36) and LOADEDAUDIO on the audio streamer.
//   RaceCarEntityModule::ResetActiveRaceCar @0x822F4880   (muState = 3)
//       the ONLY writer of E_STATE_ACTIVE in the whole XEX. Its only caller is
//       PlaceOnTrackManager::PrePhysicsUpdate, and it needs a
//       BrnPhysics::Vehicle::VehicleInputInterface plus ActiveRaceCar::AddHandlingModel.
//
// And the render pose itself has exactly ONE console producer:
//   ActiveRaceCar::UpdatePhysicsState @0x822D4418, whose only caller is
//   ReadUpdatedActiveRaceCarDataFromPhysics -- it stores
//   `CalcBodyTransform()` into mRenderParams.mBodyTransform.
//
// WHAT MAKES THIS HONEST RATHER THAN A FABRICATED TRANSFORM. CalcBodyTransform is
// `Mult(mCentreOfMassTransform, mPhysicsState.mTransform)`. Until OnResourcesLoaded runs,
// mCentreOfMassTransform is the IDENTITY that Prepare/Attach leave behind, and
// mPhysicsState.mTransform is what Attach seeded from RaceCar::GetTransform(). So for a
// car that is standing still, running the console's own CalcBodyTransform here produces
// bit-for-bit what UpdatePhysicsState would have stored. Nothing about the pose is
// invented: the position comes from RaceCar::AddToWorld, the maths is the console's.
// (Once OnResourcesLoaded lands it will replace the identity with the authored
// body-to-chassis offset out of the now-loaded physics def, and the pose gets *better*.)
//
// WHAT IS STILL A LIE, stated plainly:
//   * muState is forced to E_STATE_ACTIVE without the physics handling model existing;
//   * mLOD is forced to LOD 0. Reset() seeds state 4 and the console's own else-arm never
//     writes mLOD either -- on the console the scene/replay arm sets it via
//     ShadowMap::CalcOptimisedLod. The Cavalry's part models do not all carry state 4, so
//     LOD 0 is a bring-up choice, not a reconstruction.
//   * the car never moves, because nothing simulates it.
//
// DELETE-WHEN: VEH_<id>_AT.bin is ported AND ActiveRaceCar::OnResourcesLoaded +
// PlaceOnTrackManager::PrePhysicsUpdate + ResetActiveRaceCar land. At that point this
// whole function goes and the two console calls above take over.
// ============================================================================
void RaceCarEntityModule::PromoteAttachedCarToActiveBringUp( ActiveRaceCar* lpActiveRaceCar )
{
    const s32 liCar = static_cast<s32>( lpActiveRaceCar->GetActiveRaceCarIndex() );

    if( !mRaceCarStreamer.IsGraphicsLoadedBringUp( liCar ) )
    {
        return;   // the body has not arrived yet
    }

    lpActiveRaceCar->SetActiveBringUp();
    lpActiveRaceCar->GetRenderParams()->SetLOD( CgsGraphics::Model::E_STATE_LOD_0 );

    // ⚠️ [FLAG PC bring-up] FORCE mbDamaged OFF -- and this one is a MEASURED defect, not
    // a tidy-up. ActiveRaceCar::Attach sets mbDamaged = RaceCar::ToBeRenderedDamaged(),
    // which is true for EVERY player car (that is console behaviour and it stays in
    // Attach). mbDamaged selects RenderRaceCar's DAMAGED technique (0/3 instead of 1/2),
    // and the damaged technique reads the per-car deformation verlet block out of shader
    // constants 22/23 -- which this build never uploads, because
    // ShaderConstantTable::SetShaderConstantArrayData is declaration-only for all five
    // overloads (CgsShaderConstants.h). The shader therefore offsets every vertex by
    // whatever is left in those registers: the Cavalry renders with metre-long spikes
    // where its doors and bonnet should be. Verified by A/B capture -- the render wave's
    // stand-in producer never wrote mbDamaged, which is the only reason its screenshot
    // looked clean.
    // DELETE-WHEN: SetShaderConstantArrayData is bodied (then the damaged technique has
    // its data and mbDamaged can carry Attach's console value through).
    lpActiveRaceCar->GetRenderParams()->SetDamaged( false );

    Matrix44Affine lBodyTransform;
    lpActiveRaceCar->CalcBodyTransform( lBodyTransform );
    lpActiveRaceCar->GetRenderParams()->SetBodyTransform( lBodyTransform );

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[FLAG PC bring-up] race car " << liCar
            << " promoted to E_STATE_ACTIVE without the physics handling model "
               "(all-resources loaded = "
            << ( mRaceCarStreamer.IsRaceCarLoaded( liCar ) ? 1 : 0 )
            << ", OnResourcesLoaded + ResetActiveRaceCar not reconstructed). Body "
               "transform is the console's own CalcBodyTransform over the attach-time "
               "pose: ("
            << lBodyTransform.wAxis.x << ", " << lBodyTransform.wAxis.y
            << ", " << lBodyTransform.wAxis.z << ")\n";
    }
}

// [FLAG PC bring-up] NOT an X360 function -- see the header. Reports the first active
// slot's render pose so the world module's bring-up tour camera can frame it.
bool RaceCarEntityModule::GetSpawnedCarPositionBringUp( Vector3& lrPosition ) const
{
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        const ActiveRaceCar& lrActiveRaceCar = maActiveRaceCars[liCar];
        if( lrActiveRaceCar.IsActive() )
        {
            lrPosition = lrActiveRaceCar.GetRenderParams()->GetBodyTransform().wAxis;
            return true;
        }
    }
    return false;
}

// ============================================================================
// AttachActiveRaceCar  @ 0x822F4DB0
//
// Bind a global RaceCar to one of the eight active-race-car slots and start that slot's
// asset load. Slot selection (asm 0x822F4E80..0x822F50C4):
//   leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID:
//       the car already knows a slot  -> re-use it (must not already be attached)
//       otherwise                     -> first slot whose muState is E_STATE_INACTIVE
//                                        ("Ran out of active race cars to use")
//   otherwise: use it (must not already be attached).
// Then Prepare() the slot, copy the module's mbIsInGameMode into it, Attach the car, and
// hand the model/wheel pair to the streamer.
//
// ⚠️ NOT [VMX] -- the PC FLAG INVENTORY files this under [VMX] and the console body has
// no vector instruction at all.
// ============================================================================
EActiveRaceCarIndex RaceCarEntityModule::AttachActiveRaceCar(
        RaceCar* lpRaceCar, EActiveRaceCarIndex leActiveRaceCarIndex )
{
    CGS_ASSERT( lpRaceCar != 0, "lpRaceCar != NULL" );                       // X360 :2112
    CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
    CGS_ASSERT( lpRaceCar->IsInWorld(), "lpRaceCar->IsInWorld()" );          // X360 :2113
    CGS_ASSERT( !lpRaceCar->HasActiveRaceCar(), "!lpRaceCar->HasActiveRaceCar()" );  // X360 :2114

    EActiveRaceCarIndex leSlot = leActiveRaceCarIndex;

    if( leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        if( lpRaceCar->GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID )
        {
            s32 liFree = 0;
            while( liFree < E_ACTIVE_RACE_CAR_INDEX_COUNT
                   && GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liFree ) )->IsAttached() )
            {
                ++liFree;
            }

            CGS_ASSERT( liFree < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        "Ran out of active race cars to use" );              // X360 :2132
            leSlot = static_cast<EActiveRaceCarIndex>( liFree );
        }
        else
        {
            leSlot = lpRaceCar->GetActiveRaceCarIndex();
            CGS_ASSERT( !GetActiveRaceCar( leSlot )->IsAttached(),
                        "Error attaching car, previous index is already attached" );  // X360 :2138
        }
    }
    else
    {
        CGS_ASSERT( !GetActiveRaceCar( leSlot )->IsAttached(),
                    "!GetActiveRaceCar(leActiveRaceCarIndex)->IsAttached()" );        // X360 :2144
    }

    CGS_ASSERT( leSlot >= E_ACTIVE_RACE_CAR_INDEX_0,
                "No active race cars available to make race car active" );   // X360 :2147
    CGS_ASSERT( leSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );    // X360 :2148

    ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leSlot );

    lpActiveRaceCar->Prepare();
    lpActiveRaceCar->SetInGameMode( mbIsInGameMode );          // this[99140] -> car+0x777
    lpActiveRaceCar->Attach( lpRaceCar, mbCarSelectDontStreamAudio );

    // The streamer's "stream this car's audio" flag: true only for the PLAYER's car, and
    // only when we are not sitting in the car-select screen with audio streaming disabled.
    const bool lbStreamAudio =
        ( !mbInCarSelectScreen || !mbCarSelectDontStreamAudio )
        && lpRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER;

    mRaceCarStreamer.AddVehicleData( leSlot,
                                     lpRaceCar->GetModelId(),
                                     lpRaceCar->GetWheelModelId(),
                                     lbStreamAudio );

    return leSlot;
}

// ============================================================================
// [FLAG PC bring-up] SpawnFirstUnlockedCarBringUp -- NOT an X360 function.
//
// WHAT CHANGED (pose wave 2026-07-31). The previous revision of this stand-in called
// RaceCarStreamer::AddVehicleData directly, because AttachActiveRaceCar and the whole
// ActiveRaceCar interior were absent. Both are real now, so ALL this function still does
// is pull the trigger that SpawnRaceCar's seven callers pull on the console:
//     RaceCar::AddToWorld            (console, 0x822BE4F0)
//     RaceCar::UpdatePositioningData (console, 0x822D3788)
//     AttachActiveRaceCar            (console, 0x822F4DB0)
// Every state change -- muState, mpRaceCar, the render snapshot, the streamer request --
// is now console code operating on real console members. (UpdatePositioningData is the
// one console call the first revision of this made and then dropped -- it dereferences
// the module's mWorldMap2D, which this header still keeps opaque; see the call site.)
//
// WHAT IS STILL FAKE, precisely:
//   * THE TRIGGER. SpawnRaceCar @0x822FE5D8 itself is not reconstructed: its tail posts a
//     BrnAI::AIModuleIO::AttachAIControlEvent into the AI module's variable event queue,
//     and its callers (AddRaceCarToStartingGridOrFreeburnLobby, SetUpPlayerCarForMode, ...)
//     need BrnGameState::GameModeParams. None of that exists here.
//   * THE SPAWN POSE. It is the console's own data, not invented: the eight world
//     positions AddRaceCarToStartingGridOrFreeburnLobby @0x82300B38 bakes into its code as
//     the FREEBURN LOBBY grid (immediates at 0x82300B70..0x82300D20). Slot 0 is used.
//     The console builds the car's basis from those positions plus a DIRECTION its caller
//     supplies (Camera::Utils::CreateLookAt(pos, pos + dir)); that direction comes from
//     GameModeParams, so the identity basis below is the one part of the pose that is a
//     bring-up choice rather than console data.
//   * Vehicle index 0 is the Hunter Cavalry (PUSMC01) -- the one car unlocked at the
//     Junkyard. The ID derivation is SpawnRaceCar's own
//     (entry->GetId() / WheelList::FindWheelIndexFromName(entry->GetDefaultWheelName())).
//
// DELETE-WHEN: SpawnRaceCar + one of its callers land.
// ============================================================================
void RaceCarEntityModule::SpawnFirstUnlockedCarBringUp()
{
    if( mbBringUpCarRequested || mpVehicleList == 0 || mpWheelList == 0 )
    {
        return;
    }
    mbBringUpCarRequested = true;

    const s32 KI_BRINGUP_VEHICLE_INDEX  = 0;   // PUSMC01, "Hunter Cavalry"

    const BrnResource::VehicleListEntry* lpEntry =
        mpVehicleList->GetVehicleData( KI_BRINGUP_VEHICLE_INDEX );
    if( lpEntry == 0 )
    {
        return;
    }

    const CgsID lModelId       = lpEntry->GetId();
    const char* lpcWheelName   = lpEntry->GetDefaultWheelName();

    s32 liWheelIndex = mpWheelList->FindWheelIndexFromName( lpcWheelName );
    if( liWheelIndex < 0 )
    {
        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "*** Couldn't find Wheel: " << lpcWheelName << " for Vehicle: "
                << lpEntry->GetName() << "\n";
        }
        liWheelIndex = 0;
    }

    const BrnResource::WheelListEntry* lpWheelEntry = mpWheelList->GetWheelData( liWheelIndex );
    if( lpWheelEntry == 0 )
    {
        return;
    }
    const CgsID lWheelId = lpWheelEntry->mID;

    // The console's freeburn-lobby grid, slot 0. AddRaceCarToStartingGridOrFreeburnLobby
    // @0x82300B38 loads all eight of these as immediates and indexes them by opponent
    // index; they are authored, road-level Paradise City positions (every Y is between
    // -4.13 and -0.94).
    Matrix44Affine lSpawnTransform;
    lSpawnTransform.xAxis = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
    lSpawnTransform.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lSpawnTransform.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    lSpawnTransform.wAxis = Vector3{ 3008.1699f, -1.16f, -1874.3f, 0.0f };

    RaceCar* lpRaceCar = GetGlobalRaceCar( E_GLOBAL_RACE_CAR_INDEX_0 );

    // The player's car has no rival id and no rival/opponent index (SpawnRaceCar passes
    // the same sentinels for E_RACE_CAR_TYPE_PLAYER).
    const CgsID KU_NO_RIVAL_ID      = 0;
    const s8    KI_NO_RIVAL_INDEX   = -1;
    const s32   KI_NO_OPPONENT_IDX  = -1;

    // AddToWorld stores mTransform, which is the whole pose the attach chain needs.
    // [FLAG PC bring-up] SpawnRaceCar additionally calls
    // RaceCar::UpdatePositioningData(transform, &mWorldMap2D) to resample the car's
    // district. mWorldMap2D lives inside this header's opaque tail span, so there is no
    // map to sample and the call would dereference NULL. Dropped, not faked: the only
    // thing it adds beyond AddToWorld is mPreviousPosition and mWorldRegion, neither of
    // which the render leg reads.
    lpRaceCar->AddToWorld( E_RACE_CAR_TYPE_PLAYER, lSpawnTransform,
                           KU_NO_RIVAL_ID, lModelId, lWheelId,
                           KI_NO_RIVAL_INDEX, KI_NO_OPPONENT_IDX );

    const EActiveRaceCarIndex leSlot =
        AttachActiveRaceCar( lpRaceCar, E_ACTIVE_RACE_CAR_INDEX_INVALID );

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[FLAG PC bring-up] RaceCar: no spawn path exists, so pulling "
               "SpawnRaceCar's trigger by hand -- '" << lpEntry->GetName()
            << "' wheels '" << lpcWheelName << "' (index " << liWheelIndex
            << ") into active slot " << static_cast<s32>( leSlot )
            << " at the console's freeburn-lobby position 0 ("
            << lSpawnTransform.wAxis.x << ", " << lSpawnTransform.wAxis.y
            << ", " << lSpawnTransform.wAxis.z
            << "). AddToWorld / UpdatePositioningData / AttachActiveRaceCar are all "
               "console code; only the trigger and the facing are stand-ins.\n";
    }
}

// X360 0x82304F70. Two statements: assert the output buffer, then hand the buffer's
// resource-request interface to the streamer's five-queue drain.
void RaceCarEntityModule::SendStreamerEvents(
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput" );   // X360 :5156

    if( lpOutput == 0 )
    {
        return;
    }

    mRaceCarStreamer.AppendGameDataRequests( lpOutput->GetResourceRequestInterface() );
}

// X360 0x8230D928 -- PARTIAL SLICE.
//
// The console body is a 15-step per-frame spine (replay enter/leave edge, the camera-vector
// copy, the serialiser read, UpdateSerialiser, HandleGameActions, the per-slot AI/palette
// pass, UpdateRaceCars_PreScene / UpdateInAndOutOfRangeCars, UpdateStreaming,
// WriteUpdatedAIData, UpdatePropBoundingBoxes_PreScene, UpdateOutputInterfaces,
// UpdateActiveToAICarLookup, UpdateDisconnectedPlayers). Every one of those except the
// two reproduced here reaches the un-homed RaceCar/ActiveRaceCar/manager interiors.
//
// REPRODUCED, exactly as the console orders them:
//   * step 10: the sim time step latch. `if (update-set bit 0) mfTimeStep = 0 else
//     mfTimeStep = simStatus->mfBaseTimeStep * simStatus->mfTimeStepMultiplier`
//     (asm `*(v52+28) * *(v52+32)` on the input's TimerStatusInterface, whose sim block
//     starts at +24). Bit 0 of the update set is the "sim paused" bit.
//   * step 11: `if (!mbInReplay) UpdateStreaming(lpInput, lpOutput)`. mbInReplay
//     (+0x18774... console +99828) is not modelled here; the PC build has no replay, so
//     the guard is constant-false and the call is unconditional. FLAGGED rather than faked.
//
// [FLAG PC bring-up] everything else is dropped, NOT paraphrased.
// DELETE-WHEN: the interior lands and the full spine is reconstructed.
void RaceCarEntityModule::PreSceneUpdate(
        RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput,
        BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpInput != 0, "lpInputBuffer" );     // X360 :986
    CGS_ASSERT( lpOutput != 0, "lpOutputBuffer" );   // X360 :987

    if( lpInput == 0 || lpOutput == 0 )
    {
        return;
    }

    lpInput->LockForRead();
    lpOutput->LockForWrite();

    // ---- step 10: latch the sim time step -----------------------------------
    if( ( lUpdateSet & 1 ) != 0 )
    {
        mfTimeStep = 0.0f;
    }
    else
    {
        const RaceCarEntityModuleIO::TimerStatusInterface* lpTimers =
            lpInput->GetTimerStatusInterface();
        mfTimeStep = ( lpTimers != 0 )
            ? lpTimers->GetSimTimerStatus()->GetCurrentTimeStep()
            : 0.0f;
    }

    // ---- step 11: pump the five component streamers -------------------------
    UpdateStreaming( lpInput, lpOutput );

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
}

// X360 0x82307538 -- PARTIAL SLICE. The console body runs the post-physics half of the
// module (ProcessCreateVehicleEvents, the crash/takedown queues, the director vehicle
// input, the replay request interface, ...). Only the streamer drain is reproduced --
// and it is the load-bearing one: InternalBaseStreamer::Update clears its own request ring
// at the top of every frame, so a load request that is not drained in the frame it was
// posted is silently lost.
// [FLAG PC bring-up] the rest is dropped, NOT paraphrased. DELETE-WHEN the interior lands.
void RaceCarEntityModule::PostPhysicsUpdate(
        RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput,
        BrnUpdateSet lUpdateSet )
{
    (void)lpInput;
    (void)lUpdateSet;

    if( lpOutput == 0 )
    {
        return;
    }

    lpOutput->LockForWrite();
    SendStreamerEvents( lpOutput );
    lpOutput->UnlockForWrite();
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
