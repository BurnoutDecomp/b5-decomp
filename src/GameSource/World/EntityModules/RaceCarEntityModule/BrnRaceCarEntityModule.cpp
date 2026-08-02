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
#include "GameSource/GameState/BrnGameActions.h"                                         // GameStateModuleIO::ResetPlayerCarAction (game action 0)
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"                         // BrnAI::AIModuleIO::RaceCarAIInterface / AttachAIControlEvent
#include "GameSource/Math/BrnMathUtils.h"                                                // BrnMath::BuildTransform / IsNormal
#include "rw/math/vpu/vector3_operation.h"                                               // rw::math::vpu::IsValid(Vector3)
#include "rw/math/vpu/matrix44affine_operation.h"                                        // rw::math::vpu::IsValid(Matrix44Affine)

#include <cstring>   // memset

namespace BrnWorld
{

// The null CgsID sentinel, spelled exactly as BrnRaceCar.cpp does (a "spawn/reset" record
// carries 0 for "no car" / "derive the wheel set" / "no rival").
static const CgsID KU_CGSID_NULL = 0;

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

        // ⭐ FIXED 2026-08-02 (colour-picker wave) -- THIS BLOCK READ PAST THE END OF THE
        // EVENT. The stage-0/1 request above is a type-4 AcquireResourceRequest, so the pool
        // module answers with a CgsResource::Events::AcquireResourceResponse (see
        // PoolModule::DoAcquireResourceRequest @0x828FCD48, tag 6). The reply was being read
        // through a BrnResource::GameDataIO::GameDataAssetEvent view, which is a DIFFERENT
        // record: GameDataAssetEvent::mHandle sits at +40, while AcquireResourceResponse is
        // only {PoolEvent, mpResourceMemory, mpSourceEntry} == 32 bytes on x64. So both
        // `mCarColoursResource` and `mbCarColoursBound` were built from bytes past the end of
        // the record -- which is why "[RaceCar] LoadGlobalResources done: ... carColours=0"
        // reported a FALSE NEGATIVE even once the palette resource really was resident.
        // Read it the way WorldDataController::Prepare's identical stage does: by name off
        // the response's own two lanes.
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        mReceiverQueue.GetFirstEvent( &lpEvent, &liEventSize );
        if( lpEvent != 0 )
        {
            const CgsResource::Events::AcquireResourceResponse* lpAcquire =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEvent );

            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
            lHandle.mpSourceEntry    = lpAcquire->mpSourceEntry;

            CgsResource::ResourcePtr<GlobalColourPalette> lPalette( lHandle );
            mCarColoursResource  = lPalette;
            mbCarColoursBound    = ( lpAcquire->mpResourceMemory != 0 );
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
        // ⚠️ ONE EXCEPTION (drivable wave 2026-08-01): mPlaceOnTrackManager.Construct(this).
        // The console does it in RaceCarEntityModule::Construct @0x822FD898, which is
        // declaration-only on this build -- the SAME seam that left mePlayerActiveRaceCarIndex
        // at slot 0 last wave. Without it the manager's mpRaceCarEntityModule is whatever the
        // module memory held and PrePhysicsUpdate dereferences it. DELETE when Construct lands.
        mPlaceOnTrackManager.Construct( this );
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

    // ⭐ THE BRING-UP SPAWN IS GONE (2026-08-01). SpawnFirstUnlockedCarBringUp used to be pulled
    // here; the player's car is now placed by the console's own chain --
    //   GameStateModule::PreWorldUpdate's latch -> SendSetupPlayerCarEvent
    //     -> CarSelectManager::EnterJunkyardAtStartOfGame  (posts ResetPlayerCarAction)
    //     -> BridgeGameStateToWorld -> BridgeActionsToRaceCarModule
    //     -> RaceCarEntityModule::HandleGameActions case 0
    //     -> HandleResetPlayerCarAction -> SpawnRaceCar + AttachActiveRaceCar
    // -- which puts it at the JUNKYARD spawn location (maSpawnLocations[1]) instead of slot 0 of
    // the freeburn-lobby grid.

    mRaceCarStreamer.Update( lpInput, lpOutput, mfTimeStep );

    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );

        if( !lpActiveRaceCar->IsWaitingForLoad() )
        {
            continue;
        }

        // [FLAG PC bring-up] the console's predicate is IsRaceCarLoaded() (all five resource
        // bits); this build's DATA delivers three. See
        // RaceCarStreamer::IsRaceCarLoadedForStateMachineBringUp for the measurement and the
        // DELETE-WHEN. This is the ONE gate relaxation in the whole promote chain.
        if( mRaceCarStreamer.IsRaceCarLoadedForStateMachineBringUp( liCar )
            && ( lpActiveRaceCar->IsPlayer()
              || !lpActiveRaceCar->IsOnRaceStartState(
                     ActiveRaceCar::E_RACE_START_STATE_ON_START_LINE ) ) )
        {
            // ⭐ REAL as of 2026-08-01 (drivable wave). This is the console's own
            // ATTACHED -> WAITING step and it is what retired the promote fiction.
            OnRaceCarResourcesLoaded( static_cast<EActiveRaceCarIndex>( liCar ),
                                      lpOutput->GetVehicleInputInterface() );
        }
        else
        {
            // Console: `lbAllLoaded = false` (the flag feeds the streaming-complete
            // publish, whose output member is not modelled here).
        }
    }
}

// ============================================================================
// OnRaceCarResourcesLoaded  @ 0x822FEBF8   (drivable wave 2026-08-01)
//
// ⭐ THIS AND ResetActiveRaceCar BELOW ARE WHAT RETIRED PromoteAttachedCarToActiveBringUp.
// The old function forced muState / mLOD / mbDamaged directly because BOTH console steps
// between E_STATE_ATTACHED and E_STATE_ACTIVE were absent. Both are here now.
//
// The console body, in asm order:
//   assert IsWaitingForLoad() (:2201), assert IsAttached(), assert mpRaceCar (:2204)
//   liModelIndex = mpVehicleList->GetVehicleIndex(mpRaceCar->GetModelId());  assert >= 0
//   lpListEntry  = mpVehicleList->GetVehicleData(liModelIndex);              assert != 0
//   lVelocity    = 0, unless this car is the one at module+100212 in which case it is
//                  (module+100192) * splat(module+100208)
//   ActiveRaceCar::OnResourcesLoaded(streamerPhysicsRes, streamerGraphicsRes,
//                                    lVelocity, lpListEntry->GetAttribKey())
//   SetupCarColour(index)
//   PlaceRaceCarOnLoad(mpRaceCar)
//   car->mbComingInRange = false; car->mbIsJoiningGameMode = false     (+1909/+1910)
//
// [FLAG PC bring-up] TWO legs are not reproduced and are not paraphrased:
//   * the initial-velocity override. It reads three module members inside maTailPadB1
//     (+100192 a Vector3, +100208 a float, +100212 a RaceCar*) that this header does not
//     name. They form a "spawn this specific car moving" hook; the start-of-game path
//     spawns a stationary car, so zero is the value the console would compute anyway.
//   * SetupCarColour @0x822F5170 -- it reaches the colour-palette resource plus the
//     per-car graphics attrib collection ActiveRaceCar::OnResourcesLoaded is itself not
//     reading yet (see its own banner). Left as an explicit call site comment.
// ============================================================================
void RaceCarEntityModule::OnRaceCarResourcesLoaded(
        EActiveRaceCarIndex leActiveRaceCarIndex,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface )
{
    ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leActiveRaceCarIndex );

    CGS_ASSERT( lpActiveRaceCar->IsWaitingForLoad(),
                "lpActiveRaceCar->IsWaitingForLoad()" );                       // X360 :2201
    CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );               // BrnActiveRaceCar.h:1089

    RaceCar* lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
    CGS_ASSERT( lpRaceCar != 0, "lpRaceCar" );                                 // X360 :2204
    if( lpRaceCar == 0 || mpVehicleList == 0 )
    {
        return;
    }

    const s32 liModelIndex = mpVehicleList->GetVehicleIndex( lpRaceCar->GetModelId() );
    CGS_ASSERT( liModelIndex >= 0, "liModelIndex >= 0" );                      // X360 :2213

    const BrnResource::VehicleListEntry* lpListEntry =
        mpVehicleList->GetVehicleData( liModelIndex );
    CGS_ASSERT( lpListEntry != 0, "lpListEntry != NULL" );                     // X360 :2217

    // [FLAG PC bring-up] the module's spawn-with-velocity hook (see the banner). The
    // console's default -- and the value on every path this build drives -- is zero.
    const Vector3 lInitialVelocity = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

    lpActiveRaceCar->OnResourcesLoaded(
        mRaceCarStreamer.GetPhysicsResourceHandle( static_cast<s32>( leActiveRaceCarIndex ) ),
        mRaceCarStreamer.GetGraphicsResourceHandle( static_cast<s32>( leActiveRaceCarIndex ) ),
        lInitialVelocity,
        ( lpListEntry != 0 ) ? lpListEntry->GetAttribCollectionKeyHash() : 0ull );

    // [FLAG PC bring-up] SetupCarColour(leActiveRaceCarIndex) -- see the banner.

    PlaceRaceCarOnLoad( lpRaceCar );

    lpActiveRaceCar->SetComingInRange( false );          // +0x775 (1909)
    lpActiveRaceCar->SetJoiningGameMode( false );        // +0x776 (1910)

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[PLACEONTRACK] race car " << static_cast<s32>( leActiveRaceCarIndex )
            << " resources loaded -> E_STATE_WAITING\n";
    }

    (void)lpVehicleInputInterface;   // the console passes it straight through to
                                     // ActiveRaceCar::OnResourcesLoaded's caller chain;
                                     // AddHandlingModel is what actually consumes it.
}

// ============================================================================
// PlaceRaceCarOnLoad  @ 0x822CE588   (drivable wave 2026-08-01) -- PARTIAL SLICE.
//
// Decides WHERE a freshly loaded car goes. The console body is a five-way branch on the
// car's type + the module's game-mode state:
//
//   muType == E_RACE_CAR_TYPE_PLAYER (0):
//       *(module+99141)                -> RaceCar::RequestResetOnTrack(car, 1, 0, 0)
//       otherwise                      -> ⭐ ActiveRaceCar::RequestPlaceOnTrack(
//                                            car->GetPosition(), car->GetDirection(), 0.0f)
//   otherwise (AI / rival / traffic):  five further arms, all through
//                                      RaceCar::RequestResetOnTrack with a start-line /
//                                      buzz-by / grid offset.
//
// ONLY the player arm is reproduced, and deliberately: every other arm calls
// RaceCar::RequestResetOnTrack (not reconstructed -- it posts into the AI module's
// reset-on-track request interface, whose consumer is also absent) and two of them need
// BrnAI::BuzzBy::MaintainAheadOrBehind plus three unnamed module members. The player arm
// is the start-of-game path and the only one this build can reach: the junkyard spawn
// creates exactly one car, of type E_RACE_CAR_TYPE_PLAYER.
//
// [FLAG] *(module+99141) -- the byte after mbIsInGameMode in the DWARF bool run
// (BrnRaceCarEntityModule.h:370..), unnamed in this header's model of that run. It gates
// "reset onto the track properly" vs "just drop me here"; false is what a start-of-game
// junkyard spawn wants and false is what the zeroed module holds.
// ============================================================================
void RaceCarEntityModule::PlaceRaceCarOnLoad( RaceCar* lpRaceCar )
{
    CGS_ASSERT( lpRaceCar != 0, "lpRaceCar" );
    if( lpRaceCar == 0 )
    {
        return;
    }

    CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                "muType < E_RACE_CAR_TYPE_COUNT" );                     // BrnRaceCar.h:577

    if( lpRaceCar->GetType() == E_RACE_CAR_TYPE_PLAYER )
    {
        ActiveRaceCar* lpActiveRaceCar = lpRaceCar->GetActiveRaceCar();
        if( lpActiveRaceCar != 0 )
        {
            lpActiveRaceCar->RequestPlaceOnTrack( lpRaceCar->GetPosition(),
                                                  lpRaceCar->GetDirection(),
                                                  0.0f );
        }
        return;
    }

    // [FLAG PC bring-up] the four non-player arms -- see the banner. They all end in
    // RaceCar::RequestResetOnTrack, which is not reconstructed.
}

// ============================================================================
// ResetActiveRaceCar  @ 0x822F4880   (drivable wave 2026-08-01)
//
// ⭐⭐ THE ONLY WRITER OF ActiveRaceCar::E_STATE_ACTIVE IN THE ENTIRE XEX. Its only
// caller is PlaceOnTrackManager::PrePhysicsUpdate.
//
// ⚠️ SIGNATURE. Hex-Rays renders `(this, index, transform, vehicleInterface)` and DROPS
// the velocity, which arrives in the VECTOR register v1 (`vmr128 v127, v1` at the top,
// then `vmr128 v1, v127` immediately before each of the two callees). Incident TEN of the
// dropped-argument rule and the second time it is a vector register.
//
// The body is a two-arm branch on the slot's state:
//   IsActive()  -- re-reset a car that is ALREADY live (the wreck/road-rage path). It
//                  computes a deformation-reset kind from four ActiveRaceCar predicates
//                  and a "how close to totalled" scalar, calls
//                  VehicleInputInterface::ResetRaceCar, flips the slot's bit in the
//                  module's reset BitArray and calls ActiveRaceCar::ResetAfterCrash.
//   muState==2  -- ⭐ the PROMOTE. This is the arm the start-of-game path takes.
//
// [FLAG PC bring-up] THE IsActive() ARM IS NOT REPRODUCED, and this is a deliberate slice
// rather than a silent drop. It needs five module members that live inside maTailPadB1 and
// maTailPadA0 and are unnamed in this header (+99536/+99544/+99548 the deformation-amount
// pair and its one-shot flag, +99164 the game-mode flag word, +65760 the reset BitArray),
// plus ActiveRaceCar::ResetAfterCrash @0x822BF3A0 (the ledger says `reviewed`; the tree has
// no body -- same drift as BrnMath::BuildTransform last wave). It is UNREACHABLE on this
// build: nothing re-requests placement for a car that is already ACTIVE.
// DELETE-WHEN those members are named and ResetAfterCrash lands.
// ============================================================================
void RaceCarEntityModule::ResetActiveRaceCar(
        EActiveRaceCarIndex leActiveRaceCarIndex,
        const Matrix44Affine& lrTransform,
        const Vector3& lrVelocity,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface )
{
    // asm 0x822F489C..0x822F48C8: the four 16-byte lanes of the transform are copied into
    // the module's own scratch at +97568 BEFORE anything else. [FLAG] that member is inside
    // maTailPadA0 and unnamed; nothing reconstructed reads it back.

    ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leActiveRaceCarIndex );

    if( lpActiveRaceCar->IsActive() )
    {
        CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.h:1089
        // [FLAG PC bring-up] the already-ACTIVE re-reset arm -- see the banner.
        return;
    }

    if( !lpActiveRaceCar->IsWaitingForPlacement() )
    {
        // Console: neither arm. A slot that is merely ATTACHED (still streaming) is left
        // alone; so is an INACTIVE one.
        return;
    }

    // ---- the PROMOTE arm (asm 0x822F4D18..0x822F4E28) ----------------------------------
    RaceCar* lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
    CGS_ASSERT( lpRaceCar != 0, "mpRaceCar != NULL" );
    if( lpRaceCar == 0 || mpVehicleList == 0 )
    {
        return;
    }

    const CgsID lModelId    = lpRaceCar->GetModelId();
    const s32   liModelIndex = mpVehicleList->GetVehicleIndex( lModelId );
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        mpVehicleList->GetVehicleData( lModelId );
    CGS_ASSERT( lpVehicleListEntry != 0, "lpVehicleListEntry" );       // X360 :1934

    CGS_ASSERT( mRaceCarStreamer.GetCarModelId(
                    static_cast<s32>( lpActiveRaceCar->GetActiveRaceCarIndex() ) ) == lModelId,
                "mRaceCarStreamer.GetCarModelId( lpActiveRaceCar->GetActiveRaceCarIndex() ) "
                "== lpActiveRaceCar->GetGlobalRaceCar()->GetModelId()" );   // X360 :1939
    // [FLAG PC bring-up] same one gate relaxation as UpdateStreaming's predicate.
    CGS_ASSERT( mRaceCarStreamer.IsRaceCarLoadedForStateMachineBringUp(
                    static_cast<s32>( lpActiveRaceCar->GetActiveRaceCarIndex() ) ),
                "mRaceCarStreamer.IsRaceCarLoaded( lpActiveRaceCar->GetActiveRaceCarIndex() )" ); // :1940

    // asm `lbz r30, 0x1874A(r31)` -- module+100042, the byte between mbInCarSelectScreen
    // (+100041) and mbCarSelectDontStreamAudio (+100048) in the same DWARF run
    // (BrnRaceCarEntityModule.h:444..447 -> mbInCarModScreen). It is AddHandlingModel's
    // "reset the physics state" gate. [FLAG] not modelled here; false is what the zeroed
    // module holds and what a start-of-game spawn wants.
    const bool lbResettingPhysicsState = false;

    // The strength stat the console reads at lpVehicleListEntry+155.
    const u8 lu8CarStrengthStat = ( lpVehicleListEntry != 0 )
        ? lpVehicleListEntry->GetStrengthStat() : 0;

    const u64 luCarAssetAttribKey = ( lpVehicleListEntry != 0 )
        ? lpVehicleListEntry->GetAttribCollectionKeyHash() : 0ull;

    (void)liModelIndex;

    // ⭐ THE STATE WRITE. muState = E_STATE_ACTIVE, mbAIToBeActivated = true,
    // mbChangeCollisionState = false, mbChangeCullingGroup = false.
    lpActiveRaceCar->BecomeActiveForReset();

    lpActiveRaceCar->AddHandlingModel( lpVehicleInputInterface, luCarAssetAttribKey,
                                       lrTransform, lrVelocity,
                                       lbResettingPhysicsState, lu8CarStrengthStat );

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[PLACEONTRACK] race car " << static_cast<s32>( leActiveRaceCarIndex )
            << " -> E_STATE_ACTIVE at (" << lrTransform.wAxis.x << ", "
            << lrTransform.wAxis.y << ", " << lrTransform.wAxis.z << ")\n";
    }
}

// ============================================================================
// [FLAG PC bring-up] PublishRenderPoseWithoutPhysicsBringUp -- NOT an X360 function.
//
// WHAT SURVIVED OF PromoteAttachedCarToActiveBringUp, and why it had to survive.
//
// The console's ONLY producer of mRenderParams.mBodyTransform is
// ActiveRaceCar::UpdatePhysicsState @0x822D4418, whose only caller is
// ReadUpdatedActiveRaceCarDataFromPhysics -- the PHYSICS READBACK. There is no physics
// module on this build, so an ACTIVE car has no render pose, and retiring the promote
// wholesale would have made the junkyard car INVISIBLE, not drivable. (Measured by
// inspection before the change: SetBodyTransform and SetLOD had exactly one caller each,
// the promote.)
//
// WHAT IS HONEST ABOUT IT. UpdatePhysicsState stores `CalcBodyTransform()`. Until the
// physics def is read, mCentreOfMassTransform is the IDENTITY Prepare/Attach left behind
// and mPhysicsState.mTransform is what Attach seeded from RaceCar::GetTransform(), so for
// a stationary car the console's own CalcBodyTransform here produces bit-for-bit what
// UpdatePhysicsState would have stored. Nothing about the pose is invented: the position
// comes from the junkyard spawn location, the maths is the console's.
//
// WHAT IS STILL A LIE, stated plainly:
//   * mLOD is forced to LOD 0. On the console the scene/replay arm sets it via
//     ShadowMap::CalcOptimisedLod.
//   * mbDamaged is forced OFF. ActiveRaceCar::Attach sets it from
//     RaceCar::ToBeRenderedDamaged(), which is true for EVERY player car (console
//     behaviour, and it stays in Attach). mbDamaged selects RenderRaceCar's DAMAGED
//     technique, which reads the per-car deformation verlet block out of shader constants
//     22/23 -- constants this build never uploads, because
//     ShaderConstantTable::SetShaderConstantArrayData is declaration-only for all five
//     overloads. The Cavalry renders with metre-long spikes where its doors should be.
//     DELETE-WHEN SetShaderConstantArrayData is bodied.
//   * the car still does not MOVE, because nothing simulates it.
//
// It runs from PostPhysicsUpdate, which is where the console's own producer runs.
// DELETE-WHEN ReadUpdatedActiveRaceCarDataFromPhysics + UpdatePhysicsState land.
// ============================================================================
void RaceCarEntityModule::PublishRenderPoseWithoutPhysicsBringUp( ActiveRaceCar* lpActiveRaceCar )
{
    lpActiveRaceCar->GetRenderParams()->SetLOD( CgsGraphics::Model::E_STATE_LOD_0 );
    lpActiveRaceCar->GetRenderParams()->SetDamaged( false );

    Matrix44Affine lBodyTransform;
    lpActiveRaceCar->CalcBodyTransform( lBodyTransform );
    lpActiveRaceCar->GetRenderParams()->SetBodyTransform( lBodyTransform );
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
// SpawnRaceCar  @ 0x822FE5D8
//
// Take the first INACTIVE global race-car slot, reset it, put it in the world at
// lrTransform, and tell the AI module to attach control. This is the console's ONE spawn
// primitive -- all seven spawn paths (the starting grid / freeburn lobby, the mode set-ups,
// AddRivalCar, the network-car handler and HandleResetPlayerCarAction) funnel through it.
//
// The console's slot RESET is a verbatim INLINE of RaceCar::Prepare() -- every store between
// 0x822FEA64 and 0x822FEB34 matches that function field for field (identity transform, null
// ids, muType = E_RACE_CAR_TYPE_INACTIVE, the four -1 sentinels, the two colour -1s,
// mpActiveRaceCar = 0). Written here as the call the inline came from.
//
// [FLAG PC bring-up] TWO console legs are dropped rather than paraphrased:
//   * RaceCar::UpdatePositioningData(lrTransform, &mWorldMap2D) -- the module's own
//     CgsWorld::WorldMap2D lives inside this header's opaque tail (`this + 99072` in the
//     asm, inside maTailPadA0), so there is no map to sample and the call would dereference
//     the middle of a pad. All it adds beyond AddToWorld is mPreviousPosition and
//     mWorldRegion; nothing on the reconstructed path reads either. Same drop the previous
//     bring-up spawn made, for the same measured reason.
//   * the "Vehicle with ID <id> is not in the list" assert's StrStream formatting -- the
//     predicate IS reproduced, only the message is folded to static text.
// ============================================================================
EGlobalRaceCarIndex RaceCarEntityModule::SpawnRaceCar(
        BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
        const Matrix44Affine& lrTransform,
        ERaceCarType leType,
        CgsID lModelId,
        bool lbKeepResetSection,
        CgsID lWheelModelId,
        const CgsID* lpRivalId,
        s32 liOpponentIndex )
{
    CGS_ASSERT( lpRaceCarAIInterface != 0, "lpRaceCarAIInterface" );            // X360 :1969
    CGS_ASSERT( rw::math::vpu::IsValid( lrTransform ), "RwMath::IsValid( lTransform )" ); // :1970
    CGS_ASSERT( leType < E_RACE_CAR_TYPE_COUNT, "leType < E_RACE_CAR_TYPE_COUNT" );       // :1971
    CGS_ASSERT( mpVehicleList != 0 && mpVehicleList->GetVehicleIndex( lModelId ) >= 0,
                "Vehicle with ID <id> is not in the list" );                              // :1972

    if( mpVehicleList == 0 || mpWheelList == 0 )
    {
        return E_GLOBAL_RACE_CAR_INDEX_INVALID;
    }

    // ---- the car's AttribSys collection key + personality (the AI event's payload) -----
    const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex( lModelId );
    u64 lCarAssetAttribKey = 0;   // 64-bit: AttachAIControlEvent::mCarAssetAttribKey is u64
    s32 lePersonalityType = 0;   // BrnAI::EPersonalityType storage (its enum has no home yet)
    if( liVehicleIndex >= 0 )
    {
        const BrnResource::VehicleListEntry* lpEntry =
            mpVehicleList->GetVehicleData( liVehicleIndex );
        if( lpEntry != 0 )
        {
            lCarAssetAttribKey = lpEntry->GetAttribCollectionKeyHash();
            // asm: `lbz r11, 0xE8(entry); cmplwi r11, 1; bne skip; stw r11, personality`
            // -- i.e. the personality is 1 for car type 1 and 0 for everything else.
            if( lpEntry->GetCarType() == 1u )
            {
                lePersonalityType = 1;
            }
        }
    }

    const CgsID lRivalId = ( lpRivalId != 0 ) ? *lpRivalId : KU_CGSID_NULL;

    // ---- resolve the wheel set when the caller passed the null id --------------------
    CgsID lResolvedWheelId = lWheelModelId;
    if( lResolvedWheelId == KU_CGSID_NULL )
    {
        const s32 liEntryIndex = mpVehicleList->GetVehicleIndex( lModelId );
        const BrnResource::VehicleListEntry* lpEntry =
            ( liEntryIndex >= 0 ) ? mpVehicleList->GetVehicleData( liEntryIndex ) : 0;
        // The console passes `entry + 0x10` straight in; a null entry passes 0 (r3 = r30).
        s32 liWheelIndex = mpWheelList->FindWheelIndexFromName(
            ( lpEntry != 0 ) ? lpEntry->GetDefaultWheelName() : 0 );
        if( liWheelIndex == -1 )
        {
            liWheelIndex = 0;
        }
        const BrnResource::WheelListEntry* lpWheelEntry = mpWheelList->GetWheelData( liWheelIndex );
        if( lpWheelEntry != 0 )
        {
            lResolvedWheelId = lpWheelEntry->mID;
        }
    }

    // ---- find the first INACTIVE global slot ------------------------------------------
    s32 liGlobal = 0;
    while( liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT )
    {
        RaceCar* lpCandidate = GetGlobalRaceCar( static_cast<EGlobalRaceCarIndex>( liGlobal ) );
        CGS_ASSERT( lpCandidate->GetType() < E_RACE_CAR_TYPE_COUNT,
                    "muType < E_RACE_CAR_TYPE_COUNT" );                    // BrnRaceCar.h:482
        if( lpCandidate->GetType() == E_RACE_CAR_TYPE_INACTIVE )
        {
            break;
        }
        ++liGlobal;
        CGS_ASSERT( liGlobal <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                    "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT" );      // BurnoutConstants.h:84
    }
    CGS_ASSERT( liGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT, "Ran out of RaceCars to spawn" ); // :2013
    if( liGlobal >= E_GLOBAL_RACE_CAR_INDEX_COUNT )
    {
        return E_GLOBAL_RACE_CAR_INDEX_INVALID;
    }

    const EGlobalRaceCarIndex leGlobal = static_cast<EGlobalRaceCarIndex>( liGlobal );
    RaceCar* lpRaceCar = GetGlobalRaceCar( leGlobal );

    lpRaceCar->Prepare();          // the console's verbatim inline; see the banner
    lpRaceCar->AddToWorld( leType, lrTransform, lRivalId, lModelId, lResolvedWheelId,
                           static_cast<s8>( liGlobal ),   // asm `extsb r9, r29`
                           liOpponentIndex );
    // [FLAG PC bring-up] UpdatePositioningData -- see the banner.
    lpRaceCar->UpdateVelocity( Vector3{ 0.0f, 0.0f, 0.0f, 0.0f } );   // asm `vmr128 v1, v127` (zero)

    CGS_ASSERT( leGlobal >= E_GLOBAL_RACE_CAR_INDEX_0,
                "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0" );   // BrnRaceCarAIInterfaces.h:412
    CGS_ASSERT( leGlobal < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT" ); // :413

    // ---- tell the AI module to attach control ------------------------------------------
    // The console builds the record on the stack at the four offsets the DecFIGS DWARF names
    // (BrnRaceCarAIInterfaces.h:303..306) and posts it into the AI interface's own
    // VariableEventQueue<16384,16> at +0x2F8 (== mManagementQueue).
    BrnAI::AIModuleIO::AttachAIControlEvent lAttachEvent;
    lAttachEvent.meGlobalRaceCarIndex = leGlobal;
    lAttachEvent.mCarAssetAttribKey   = lCarAssetAttribKey;
    lAttachEvent.mePersonalityType    = lePersonalityType;
    lAttachEvent.mbKeepResetSection   = lbKeepResetSection;
    lpRaceCarAIInterface->mManagementQueue
        .AddEvent<BrnAI::AIModuleIO::AttachAIControlEvent>( &lAttachEvent, 0 );

    return leGlobal;
}

// ============================================================================
// HandleResetPlayerCarAction  @ 0x82304FE8   -- GAME ACTION 0
//
// ⭐ THIS IS THE FUNCTION THAT PUTS THE PLAYER'S CAR SOMEWHERE. Everything the junkyard
// flow does upstream -- FindNearestJunkyardID, SetupSpawnLocations, the 80-byte
// ResetPlayerCarAction, BridgeGameStateToWorld, BridgeActionsToRaceCarModule -- exists to
// deliver one of these records here.
//
// The body has two independent binary decisions, and it is worth stating them because the
// pseudocode hides both behind Hex-Rays' fake register pairs:
//   * lpAction->HasToChangeLocation()  -> build the pose from the action's own
//     position/direction (BrnMath::BuildTransform with the world up axis), else reuse the
//     player's CURRENT ActiveRaceCar transform.
//   * lpAction->mCarModelId != 0       -> RE-SPAWN (remove the old car, SpawnRaceCar the new
//     one, AttachActiveRaceCar it), else TELEPORT the existing car
//     (ActiveRaceCar::RequestPlaceOnTrack).
// The junkyard start-of-game record takes the "change location + re-spawn" corner, which is
// the one this build can execute end to end.
// ============================================================================
void RaceCarEntityModule::HandleResetPlayerCarAction(
        const BrnGameState::GameStateModuleIO::ResetPlayerCarAction* lpAction,
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput )
{
    if( lpAction == 0 || lpOutput == 0 )
    {
        return;
    }

    // ---- 1. the pose -------------------------------------------------------------------
    Matrix44Affine lTransform;
    if( lpAction->HasToChangeLocation() )
    {
        CGS_ASSERT( BrnMath::IsNormal( lpAction->mDirection ),
                    "BrnMath::IsNormal( lpAction->GetDirection() )" );          // X360 :7465
        // asm: v1 = action+0x00 (position), v2 = action+0x10 (direction),
        //      v3 = unk_82181510 (the world UP axis row the identity basis is built from).
        BrnMath::BuildTransform( lTransform, lpAction->mPosition, lpAction->mDirection,
                                 Vector3{ 0.0f, 1.0f, 0.0f, 0.0f } );
    }
    else
    {
        lTransform = GetActiveRaceCar( mePlayerActiveRaceCarIndex )->GetTransform();
    }

    // ---- 2. the wheel set --------------------------------------------------------------
    CgsID lWheelId = KU_CGSID_NULL;
    if( lpAction->mCarModelId != KU_CGSID_NULL )
    {
        if( lpAction->mWheelModelId != KU_CGSID_NULL )
        {
            lWheelId = lpAction->mWheelModelId;
        }
        else if( mpVehicleList != 0 && mpWheelList != 0 )
        {
            // asm: sub_82233A28(mpVehicleList, carId) -> the entry, then entry+0x10 (the
            // default wheel NAME) into FindWheelIndexFromName; -1 falls back to wheel 0.
            const BrnResource::VehicleListEntry* lpEntry =
                mpVehicleList->GetVehicleData( lpAction->mCarModelId );
            s32 liWheelIndex = mpWheelList->FindWheelIndexFromName(
                ( lpEntry != 0 ) ? lpEntry->GetDefaultWheelName() : 0 );
            if( liWheelIndex == -1 )
            {
                liWheelIndex = 0;
            }
            const BrnResource::WheelListEntry* lpWheelEntry =
                mpWheelList->GetWheelData( liWheelIndex );
            if( lpWheelEntry != 0 )
            {
                lWheelId = lpWheelEntry->mID;
            }
        }
    }

    // ---- 3. the three module flags the record carries ----------------------------------
    mbInCarSelectScreen        = lpAction->mbInCarSelectScreen;         // +0x40 -> +0x186C9
    mbCarSelectDontStreamAudio = lpAction->mbCarSelectDontStreamAudio;  // +0x41 -> +0x186D0
    // [FLAG] the +0x3C word (miInCarModification) is copied to the module's +0x186CC word,
    // which this header does not model yet (it is inside maTailPadB1). Nothing reconstructed
    // reads it; dropped rather than aimed at an unnamed pad byte.

    if( lpAction->mCarModelId != KU_CGSID_NULL )
    {
        // ================= RE-SPAWN ======================================================
        s32 liColourIndex   = 0;
        s32 liColourPalette = 0;
        bool lbWasInGameMode = false;

        if( mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID )
        {
            ActiveRaceCar* lpOldSlot = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
            RaceCar* lpOldCar = lpOldSlot->GetGlobalRaceCar();
            // [FLAG] the console gates the game-mode carry-over on the module bool at
            // +99141 -- the byte immediately after mbIsInGameMode(+99140) in the DWARF bool
            // run BrnRaceCarEntityModule.h:370.., whose NAME this header has not fitted (only
            // :370/:377/:378/:381 are pinned). Left as false rather than guessed onto a
            // neighbour; it only matters when a car ALREADY in a game mode is re-spawned,
            // which the start-of-game path this build drives never does.
            (void)lbWasInGameMode;
            // The console carries the OLD car's colour across ONLY when the model is unchanged.
            if( lpOldCar->GetModelId() == lpAction->mCarModelId )
            {
                liColourPalette = lpOldCar->GetColourPalette();
                liColourIndex   = lpOldCar->GetColourIndex();
                CGS_ASSERT( liColourPalette < 4, "Invalid Number of Palettes: " );   // :7535
                CGS_ASSERT( liColourIndex >= 0, "Invalid car colour: " );            // :7536
            }
            // [FLAG PC] RemoveRaceCar @ (this TU) is not reconstructed -- it walks the
            // streamer's per-slot release plus DetachActiveRaceCar, both of which reach the
            // un-homed manager interiors. UNREACHED on the start-of-game path, which is the
            // only path this build drives: mePlayerActiveRaceCarIndex is still INVALID there.
            // DELETE-WHEN RemoveRaceCar lands.
        }

        BrnAI::AIModuleIO::RaceCarAIInterface* lpAIInterface =
            lpOutput->GetRaceCarAIInterface();

        const EGlobalRaceCarIndex leGlobal =
            SpawnRaceCar( lpAIInterface, lTransform, E_RACE_CAR_TYPE_PLAYER,
                          lpAction->mCarModelId, lpAction->mbKeepResetSection,
                          lWheelId, 0 /* no rival id */, -1 /* no opponent index */ );

        if( leGlobal == E_GLOBAL_RACE_CAR_INDEX_INVALID )
        {
            return;
        }

        RaceCar* lpRaceCar = GetGlobalRaceCar( leGlobal );
        AttachActiveRaceCar( lpRaceCar, E_ACTIVE_RACE_CAR_INDEX_INVALID );

        CGS_ASSERT( liColourPalette < 4, "Invalid Number of Palettes: " );   // X360 :7556
        CGS_ASSERT( liColourIndex >= 0, "Invalid car colour: " );            // X360 :7557

        lpRaceCar->SetColourIndex( liColourIndex );
        lpRaceCar->SetColourPalette( liColourPalette );

        if( lbWasInGameMode )
        {
            lpRaceCar->SetInCurrentGameMode( true, true );
        }

        mePlayerActiveRaceCarIndex = lpRaceCar->GetActiveRaceCar()->GetActiveRaceCarIndex();

        // [FLAG] the console then clears ActiveRaceCar+0x1BF6 and zero-fills the eight
        // dwords at +0x1BF8. Both land INSIDE ActiveRaceCar::mRenderParams (0x7E0..0x1C80),
        // whose members at those offsets this tree has not named. They are render/deform
        // bookkeeping, not placement; dropped rather than poked by offset.

        // asm `*(this + 99144) = 1` -- the byte immediately after mbIsInGameMode(+99140) in
        // the same DWARF bool run. [FLAG] unnamed in this header's model of that run.
    }
    else
    {
        // ================= TELEPORT ======================================================
        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
        CGS_ASSERT( lpActiveRaceCar != 0, "lpActiveRaceCar" );                 // X360 :7581

        // [FLAG PC bring-up] ActiveRaceCar::RequestPlaceOnTrack(position, direction, 0.0f)
        // is not reconstructed (it hands the request to PlaceOnTrackManager, which owns the
        // physics teleport). The console's own debug line is kept so the drop is visible in
        // the log the moment a teleport record does arrive.
        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "*** HandleResetPlayerCarAction: Teleport ["
                << lTransform.wAxis.x << ", " << lTransform.wAxis.y << ", "
                << lTransform.wAxis.z << "]\n";
        }
    }

    // ---- 4. the unlock deformation ------------------------------------------------------
    if( lpAction->mfDeformationAmount >= 0.0f )
    {
        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
        CGS_ASSERT( lpActiveRaceCar != 0, "lpActiveRaceCar" );                 // X360 :7595
        // [FLAG] the console stores the (+0x34,+0x38) pair into ActiveRaceCar[499]/[498]
        // (inside mRenderParams, unnamed here) and into the module's own +99544/+99536 pair
        // (inside maTailPadB1). The start-of-game record posts the -1.0 sentinel, so this
        // arm is not taken on the path this build drives. DELETE-WHEN those members land.
        (void)lpActiveRaceCar;
    }

    // ---- 5. the player scoring slot -----------------------------------------------------
    if( static_cast<s32>( lpAction->mePlayerScoringIndex )
            < BrnGameState::GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT )
    {
        SetActiveRaceCarForPlayerScoringIndex( lpAction->mePlayerScoringIndex,
                                               mePlayerActiveRaceCarIndex );
    }
}

// ============================================================================
// HandleGameActions  @ 0x8230BE08 -- PARTIAL SLICE.
//
// The console body is one walk of the pre-scene input buffer's game-action queue with a
// ~100-case dispatch (two switch jump tables: ids > 100 at 0x8230CDBC, ids <= 100 at
// 0x8230C098). Nearly every case reaches an un-reconstructed handler or the un-homed
// RaceCar/ActiveRaceCar/manager interiors.
//
// REPRODUCED: the queue walk and case 0 (ResetPlayerCarAction). That is not an arbitrary
// choice -- action 0 is the only game action on this build that has a live producer
// (CarSelectManager) AND a fully reachable consumer.
//
// [FLAG PC bring-up] every other case is DROPPED, not paraphrased. The named handlers the
// console dispatches to and that are still un-reconstructed:
//   3   RaceCar::RequestResetOnTrack        4   HandleSetPlayerOpponentsAction
//   5   HandleSetupNetworkCarAction         7   the player-control-changed AI publish
//   11  HandleRemotePlayerDisconnected      23  HandlePrepareForModeAction
//   34  the payback arm                     39  HandleStopModeAction
//   73/74/76/77/79  the car-select / drive-thru arms
//   97/98/99        the network add/remove arms       + ~80 more.
// Because the walk itself is real, adding any one of them later is a case label, not a
// re-derivation. DELETE-WHEN the handlers land.
// ============================================================================
void RaceCarEntityModule::HandleGameActions(
        RaceCarEntityModuleIO::InputBuffer_PreScene* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput )
{
    if( lpInput == 0 || lpOutput == 0 )
    {
        return;
    }

    const RaceCarEntityModuleIO::InputBuffer_PreScene::GameActionQueue* lpQueue =
        const_cast<const RaceCarEntityModuleIO::InputBuffer_PreScene*>( lpInput )
            ->GetGameActionQueue();
    if( lpQueue == 0 )
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpQueue->GetFirstEvent( &lpEvent, &liSize );
    while( lpEvent != 0 )
    {
        switch( liType )
        {
        case BrnGameState::GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR:   // 0
            HandleResetPlayerCarAction(
                reinterpret_cast<const BrnGameState::GameStateModuleIO::ResetPlayerCarAction*>(
                    lpEvent ),
                lpOutput );
            break;

        default:
            break;   // [FLAG PC bring-up] see the banner
        }

        liType = lpQueue->GetNextEvent( lpEvent, &lpEvent, &liSize );
    }
}

// ============================================================================
// ⛔ RETIRED 2026-08-01 (reset-player-car wave): SpawnFirstUnlockedCarBringUp -- the
// stand-in trigger that pulled RaceCar::AddToWorld + AttachActiveRaceCar by hand and parked
// the Cavalry at slot 0 of the console's FREEBURN-LOBBY grid (3008.17, -1.16, -1874.30).
//
// It is replaced, not merely disabled: the car is now placed by the console's own
// ResetPlayerCarAction chain, which spawns it at the JUNKYARD spawn location the trigger
// data authored. Both the trigger and the pose it invented are gone with it, along with the
// mbBringUpCarRequested latch. What SpawnRaceCar does now is what that stand-in was
// paraphrasing -- RaceCar::Prepare + AddToWorld + UpdateVelocity + the AI attach event.
// ============================================================================

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

// ============================================================================
// X360 0x822CF208 -- UpdateActiveRaceCarColours. COMPLETE (paint wave 2026-08-02).
//
// ⭐ THIS IS THE ONLY WRITER OF THE CAR'S PAINT ANYWHERE IN THE IMAGE, and until now it
// existed in this tree only as a NAME inside the FLAG inventory below. RenderParams::
// SetPaintColour / SetPearlescentColour (BrnActiveRaceCar.h:331-332) had ZERO callers, so
// RenderParams::Reset()'s (1,1,1,1) stood for ever -- and RenderRaceCar uploads exactly
// those two as shader constants 20 (g_paintColour) and 21 (g_pearlescentColour). The main
// body-panel technique Vehicle_Opaque_BodypartsSkin_EnvMapped_Default declares NO diffuse
// sampler at all: its whole colour is g_paintColour x lighting. ⇒ the Hunter Cavalry was
// not untextured, it was WHITE, and this function is the reason.
//
// The console body, in asm order (0x822CF2A4 .. 0x822CF690):
//   for (slot = 0; slot < 8; slot++)                       // range-guarded operator++
//     car = GetActiveRaceCar(slot); if (!car->IsAttached()) continue;
//     rc = car->GetGlobalRaceCar();                        // its own IsAttached() assert
//     palette = (rc->GetColourPalette() == -1) ? 0 : rc->GetColourPalette();   // +0x98
//     colour  = (rc->GetColourIndex()   == -1) ? 0 : rc->GetColourIndex();     // +0x94
//     assert palette in [0,4)                              (:2969 "Invalid Palette Index: ")
//     assert colour  in [0, maPalettes[palette].miNumColours)
//                                                          (:2970 "Invalid Colour Index: ")
//     if (DEBUG_mbOverrideCarPalette) { clamp + re-read the two debug indices }
//     paint = maPalettes[palette].GetPaintColours()[colour];   // lvx128 v0,r11,r31 / stvx +0x1360
//     pearl = maPalettes[palette].GetPearlColours()[colour];   //                   / stvx +0x1370
//     if (DEBUG_mbOverrideCarColor)  { paint/pearl = the six debug floats, w = 1.0f }
//
// EVERY OFFSET CROSS-CHECKS against this tree's named members with no fudging:
//   ActiveRaceCar+0x6F0 (1776)   == mpRaceCar                        (BrnActiveRaceCar.h:575)
//   RaceCar+0x98 / +0x94         == miColourPalette / miColourIndex  (BrnRaceCar.h:181-182)
//   module +0x1843C (99388)      == mCarColoursResource              (this header :495)
//   ActiveRaceCar+0x1360 (4960)  == mRenderParams(+2016) + mPaintColour(+2944)
//   ActiveRaceCar+0x1370 (4976)  == mRenderParams(+2016) + mPearlescentColour(+2960)
//   palette stride 12, colour stride 16 (`slwi r11,r29,1; add r11,r29,r11; slwi r11,r11,2`
//   and `slwi r31,r28,4`) == sizeof(PlayerCarColourPalette) / sizeof(Vector4).
//
// ⭐ NOTHING HAD TO BE CHOSEN. RaceCar::Reset seeds both indices to -1 (BrnRaceCar.cpp:100),
// the console's own -1 -> 0 fallback resolves that to palette 0 / colour 0, and this build's
// own log already prints what palette 0 colour 0 is:
//   "[CarSelectLivery] palette 0 resolved: 25 colours; [0] paint=(0.784314, 0, 0, 1)
//    pearl=(0.588235, 0, 0, 1)"   -- the default Hunter Cavalry is RED.
//
// ⚠️ THE -1 -> 0 CLAMP MUST HAPPEN BEFORE THE ASSERTS, exactly as the asm does it
// (0x822CF2FC..0x822CF31C precede the 0x822CF31C range test): a verbatim assert on the RAW
// index would trap on the first frame of every freshly-Reset car.
//
// ⚠️ The two asserts are the CONSOLE'S OWN and both are reachable-but-quiet here: with the
// clamp in place palette == 0 < 4 and colour == 0 < 25. They are emitted verbatim, not
// softened -- if a future colour writer ever puts a bad index in a RaceCar, the console's
// own diagnostic is what should fire.
//
// ⚠️ mCarColoursResource is dereferenced WITHOUT a null check, exactly as the console does.
// That is safe here for a measured reason, not an assumed one: the only path that reaches
// the dereference is an ATTACHED slot, and a slot can only be attached through
// AttachActiveRaceCar, which needs mpVehicleList -- which LoadGlobalResources publishes in
// the SAME stage chain that binds this palette ("[RaceCar] LoadGlobalResources done:
// ... carColours=1", BrnGame.log:227). If that ever stops holding, ResourcePtr::operator->
// fires its own "Can not instance resource pointer" assert, which is the right diagnostic.
// ============================================================================
void RaceCarEntityModule::UpdateActiveRaceCarColours()
{
    for( EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++ )
    {
        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leActiveRaceCarIndex );

        if( !lpActiveRaceCar->IsAttached() )
        {
            continue;
        }

        // GetGlobalRaceCar() carries the console's second IsAttached() assert
        // (BrnActiveRaceCar.h:1089, the `li r5, 0x441` at 0x822CF2DC).
        RaceCar* lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();

        s32 liPaletteIndex = 0;
        s32 liColourIndex  = 0;

        if( lpRaceCar->GetColourPalette() != -1 )
        {
            liPaletteIndex = lpRaceCar->GetColourPalette();
        }

        if( lpRaceCar->GetColourIndex() != -1 )
        {
            liColourIndex = lpRaceCar->GetColourIndex();
        }

        // The X360 streams the offending index into the assert buffer through StrStream
        // ("Invalid Palette Index: " << idx). CGS_ASSERT forwards a plain string here, the
        // same shortening HandleResetPlayerCarAction's :7535/:7556 pair already uses.
        CGS_ASSERT( liPaletteIndex >= 0 && liPaletteIndex < E_NUM_PALETTES,
                    "Invalid Palette Index: " );                             // X360 :2969

        CGS_ASSERT( liColourIndex >= 0
                    && liColourIndex <
                       mCarColoursResource->maPalettes[liPaletteIndex].GetNumColours(),
                    "Invalid Colour Index: " );                              // X360 :2970

        // The palette override: clamp both debug indices into range, then use them.
        if( DEBUG_mbOverrideCarPalette )
        {
            if( DEBUG_miPaletteIndex >= E_NUM_PALETTES )
            {
                DEBUG_miPaletteIndex = E_NUM_PALETTES - 1;
            }
            liPaletteIndex = DEBUG_miPaletteIndex;

            if( DEBUG_miColourIndex >=
                mCarColoursResource->maPalettes[liPaletteIndex].GetNumColours() )
            {
                DEBUG_miColourIndex =
                    mCarColoursResource->maPalettes[liPaletteIndex].GetNumColours() - 1;
            }
            liColourIndex = DEBUG_miColourIndex;
        }

        const PlayerCarColourPalette& lrPalette =
            mCarColoursResource->maPalettes[liPaletteIndex];

        Vector4 lPaintColor = lrPalette.GetPaintColours()[liColourIndex];
        Vector4 lPearlColor = lrPalette.GetPearlColours()[liColourIndex];

        // The colour override: the six debug floats with w == 1.0f (`stfs f31` where
        // f31 == flt_82001C98 == 1.0f).
        if( DEBUG_mbOverrideCarColor )
        {
            lPaintColor.x = DEBUG_mfOverridePaintColorR;
            lPaintColor.y = DEBUG_mfOverridePaintColorG;
            lPaintColor.z = DEBUG_mfOverridePaintColorB;
            lPaintColor.w = 1.0f;

            lPearlColor.x = DEBUG_mfOverridePearlColorR;
            lPearlColor.y = DEBUG_mfOverridePearlColorG;
            lPearlColor.z = DEBUG_mfOverridePearlColorB;
            lPearlColor.w = 1.0f;
        }

        lpActiveRaceCar->GetRenderParams()->SetPaintColour( lPaintColor );
        lpActiveRaceCar->GetRenderParams()->SetPearlescentColour( lPearlColor );

        // [PC diagnostic] print the value at the PRODUCING end. The consuming end
        // (RenderRaceCar's shader constant 20) prints its own one-shot -- the
        // RaceCarState::operator= lesson: print BOTH ends of a transfer.
        {
            static bool sbLoggedFirstPaint = false;
            if( !sbLoggedFirstPaint && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstPaint = true;
                *CgsDev::Log::gpDebugPrint
                    << "[racecar-paint] UpdateActiveRaceCarColours: slot "
                    << static_cast<s32>( leActiveRaceCarIndex )
                    << " palette " << liPaletteIndex << " colour " << liColourIndex
                    << " of " << lrPalette.GetNumColours()
                    << " -> paint (" << lPaintColor.x << ", " << lPaintColor.y << ", "
                    << lPaintColor.z << ", " << lPaintColor.w << ")"
                    << " pearl (" << lPearlColor.x << ", " << lPearlColor.y << ", "
                    << lPearlColor.z << ", " << lPearlColor.w << ")\n";
            }
        }
    }
}

// ============================================================================
// X360 0x822F5CF8 -- UpdateOutputInterfaces. PARTIAL SLICE (the active-car publish).
//
// This is THE producer of RCEntityActiveRaceCarOutputInterface: nothing else in the
// whole image calls SetRaceCarState or SetPlayerActiveRaceCarData. Everything the
// director's cameras know about a car comes out of here.
//
// The console body, in order:
//   1. mWorldMap2D -> both active interfaces (two 48-byte copies from module +0x18300)
//   2. assert lpGlobalCarInterface != NULL                                    (:4900)
//   3. THE PLAYER BLOCK, keyed on mePlayerActiveRaceCarIndex:
//        idx == -1 : mbPlayerWrecked = false
//        else      : mbPlayerWrecked = GetActiveRaceCar(idx)->IsWrecked();
//                    if (slot->IsAttached())
//                    { SetPlayerActiveRaceCarData(idx, slot->meEngineState);
//                      SetPlayerGlobalRaceCarIndex(
//                          slot->GetGlobalRaceCar()->GetGlobalRaceCarIndex()); }
//   4. THE PER-SLOT LOOP (0..7): for every ATTACHED slot build the per-car flags word
//      and publish the whole snapshot with SetRaceCarState, then publish its
//      deformation-model resource ptr.
//   5. the per-GLOBAL-slot loop (0..34) -> RaceCar::FillInOutputInterface
//   6/7. the replay read/write legs against RaceCarEntitySerialiser::GetStaticLayout.
//
// REPRODUCED HERE: 2, 3, 4 (minus the deformation-ptr publish -- see below).
//
// [FLAG PC bring-up] NOT reproduced, and dropped rather than paraphrased:
//   * step 1 -- the module's own CgsWorld::WorldMap2D member is inside this header's
//     opaque attested-offset tail (it has no name here yet), so there is nothing to
//     copy FROM. Nothing on the reconstructed path reads the published copy.
//   * step 4's SetDeformationModelResourcePtr -- the console builds the ResourcePtr
//     from ActiveRaceCar+0x1CA4, which lands inside maPad1C90, the deliberately opaque
//     32-byte ResourcePtr pair at the end of ActiveRaceCar. Homing it would drag
//     StreamedDeformationSpec into every consumer of that header; the published handle
//     has no reader on this build.
//   * step 5 -- RaceCar::FillInOutputInterface @0x822BED20 is not reconstructed, so the
//     GLOBAL interface stays as Clear() left it. Its only reconstructed-path consumer is
//     BridgeWorldToDirector's 2416-byte copy into the director input's global block,
//     which is itself an honest-opaque span there.
//   * steps 6/7 -- replay. The PC has no replay path and RaceCarEntitySerialiser has no
//     static layout; the console gates both legs on its replay state word anyway.
// DELETE-WHEN: each named blocker lands.
//
// ⚠️ The console asserts "This car should be inactive" for muType == E_RACE_CAR_TYPE_INACTIVE
// on an ATTACHED slot and then still publishes flags == (IsActive?0x10:0) | 1. Reproduced
// as written -- the assert is non-gating on both platforms.
// ============================================================================
void RaceCarEntityModule::UpdateOutputInterfaces(
        RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveCarInterface,
        RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalCarInterface,
        RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpReplayActiveCarInterface,
        RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpReplayGlobalCarInterface )
{
    (void)lpReplayActiveCarInterface;    // [FLAG] steps 1 + 6/7 only
    (void)lpReplayGlobalCarInterface;    // [FLAG] steps 6/7 only

    CGS_ASSERT( lpGlobalCarInterface != 0, "lpGlobalCarInterface != NULL" );   // X360 :4900

    if( lpActiveCarInterface == 0 || lpGlobalCarInterface == 0 )
    {
        return;
    }

    // ---- step 3: the player block -------------------------------------------------
    if( mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        lpActiveCarInterface->SetPlayerWrecked( false );
    }
    else
    {
        ActiveRaceCar* lpPlayerSlot = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
        // [FLAG] the console's `mbPlayerWrecked = lpPlayerSlot->IsWrecked()` is OMITTED:
        // ActiveRaceCar::IsWrecked @0x822BFDA0 is its own un-reconstructed ledger function
        // (it walks the paired RaceCar's type, mPhysicsState's crash-cause bitfield at +484
        // bit 14, the crash flags at +488/+1100 and the slot's crash timer at +1252).
        // Publishing a fabricated answer would be worse than leaving the field at what
        // Clear() put there, which is the same `false` the -1 arm publishes. Nothing on the
        // reconstructed path reads mbPlayerWrecked. DELETE-WHEN IsWrecked lands.

        if( lpPlayerSlot->IsAttached() )
        {
            lpActiveCarInterface->SetPlayerActiveRaceCarData( mePlayerActiveRaceCarIndex,
                                                             lpPlayerSlot->GetEngineState() );
            lpGlobalCarInterface->SetPlayerGlobalRaceCarIndex(
                lpPlayerSlot->GetGlobalRaceCar()->GetGlobalRaceCarIndex() );
        }
    }

    // ---- step 4: the per-active-slot publish ---------------------------------------
    for( s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot )
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>( liSlot );
        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leSlot );

        if( !lpActiveRaceCar->IsAttached() )
        {
            CGS_ASSERT( liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" );
            continue;
        }

        CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );   // BrnActiveRaceCar.h:1089

        RaceCar* lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
        CGS_ASSERT( lpRaceCar != 0, "lpRaceCar != NULL" );             // X360 :4929
        if( lpRaceCar == 0 )
        {
            continue;
        }

        // The flags word. `v44 = IsActive() ? 0x10 : 0` then one type arm, then the
        // network/showtime arms -- each with the console's own guard asserts.
        u32 luFlags = lpActiveRaceCar->IsActive()
                        ? RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_LOADED
                        : 0u;
        luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_IN_USE;

        switch( lpRaceCar->GetType() )
        {
        case E_RACE_CAR_TYPE_PLAYER:
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_PLAYER;
            break;
        case E_RACE_CAR_TYPE_AI:
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_RIVAL;
            break;
        case E_RACE_CAR_TYPE_NETWORK:
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_NETWORK;
            break;
        default:
            CGS_ASSERT( false, "This car should be inactive" );        // X360 :4961
            break;
        }

        if( lpActiveRaceCar->GetOnlineState() == ActiveRaceCar::E_ONLINE_STATE_CONNECTING )
        {
            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
            CGS_ASSERT( lpRaceCar->IsNetworkDriven(), "lpRaceCar->IsNetworkDriven()" );   // X360 :4968
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_CONNECTING;
        }

        if( lpActiveRaceCar->IsDisconnectedFromNetwork() )
        {
            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
            CGS_ASSERT( lpRaceCar->IsNetworkDriven() || !lpRaceCar->IsInWorld(),
                        "lpRaceCar->IsNetworkDriven() || !lpRaceCar->IsInWorld()" );      // X360 :4973
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_DISCONNECTED;
        }
        else if( lpActiveRaceCar->IsNotSendingNetworkUpdates() )
        {
            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
            CGS_ASSERT( lpRaceCar->IsNetworkDriven() || !lpRaceCar->IsInWorld(),
                        "lpRaceCar->IsNetworkDriven() || !lpRaceCar->IsInWorld()" );      // X360 :4978
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_LOST_CONTACT;
        }

        if( lpActiveRaceCar->IsInShowtime() )
        {
            CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT, "muType < E_RACE_CAR_TYPE_COUNT" );
            CGS_ASSERT( lpRaceCar->IsPlayerDriven() || lpRaceCar->IsNetworkDriven(),
                        "lpRaceCar->IsPlayerDriven() || lpRaceCar->IsNetworkDriven()" );  // X360 :4984
            luFlags |= RaceCarEntityModuleIO::E_RACE_CAR_OUTPUT_FLAG_IN_SHOWTIME;
        }

        CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );   // :1118 (AI section read)
        CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );   // :1096 (paint colour read)

        const Vector4 lPaintColour = lpActiveRaceCar->GetRenderParams()->GetPaintColour();

        // Bring-up diagnostic (same cadence as BridgeWorldToDirector's own): the SOURCE end
        // of the world->director chain. Keeping both ends printed is what bisected the
        // RaceCarState::operator= empty-link-stub bug in one run -- the source read
        // (3008.17, -1.16, -1874.30) while the destination read the origin.
        {
            static u32 suSrcCount = 0;
            ++suSrcCount;
            if (suSrcCount == 1u || (suSrcCount % 3000u) == 0u)
            {
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    const Vector3& lP = lpActiveRaceCar->GetPhysicsState()->mTransform.Pos();
                    const Vector3& lR = lpActiveRaceCar->GetRenderParams()->GetBodyTransform().Pos();
                    *CgsDev::Log::gpDebugPrint
                        << "[uoi] #" << static_cast<s32>(suSrcCount) << " slot " << liSlot
                        << " physics (" << lP.x << ", " << lP.y << ", " << lP.z
                        << ") render (" << lR.x << ", " << lR.y << ", " << lR.z
                        << ") flags " << static_cast<s32>(luFlags)
                        << " engine " << static_cast<s32>(lpActiveRaceCar->GetEngineState())
                        << " playerIdx " << static_cast<s32>(mePlayerActiveRaceCarIndex) << "\n";
                }
            }
        }

        lpActiveCarInterface->SetRaceCarState(
            leSlot,
            lpRaceCar->GetGlobalRaceCarIndex(),
            lpRaceCar->GetRivalId(),
            lpRaceCar->GetModelId(),
            lpActiveRaceCar->GetPhysicsState(),
            luFlags,                                    // r9
            lpActiveRaceCar->GetCurrentAISection(),     // r10
            static_cast<u32>( lpRaceCar->GetColourIndex() ),     // stack +0x54
            lpRaceCar->GetColourPalette(),                       // stack +0x5C
            lPaintColour,                               // v1
            lpActiveRaceCar->GetCurrentInAirRotations(),// v2
            lpActiveRaceCar->HasCrashedIntoWater(),     // stack +0x87
            lpActiveRaceCar->CanDriveAwayFromCrash() ); // stack +0x8F

        CGS_ASSERT( liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" );
    }
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

    // ---- step 5: apply this frame's game actions ----------------------------
    // ⭐ NEW (reset-player-car wave 2026-08-01). The console runs HandleGameActions here,
    // before the per-slot passes, so a car spawned by an action is already in the world when
    // the streaming sweep below looks at it. Its source queue only became non-empty this wave
    // (WorldModule::BridgeActionsToRaceCarModule was an inert link stub).
    HandleGameActions( lpInput, lpOutput );

    // ---- step 11: pump the five component streamers -------------------------
    UpdateStreaming( lpInput, lpOutput );

    // ---- step 13: the PRE-SCENE output publish ------------------------------
    // ⭐⭐ ADDED 2026-08-01 (car-select hand-off wave). The console runs UpdateOutputInterfaces
    // TWICE per frame -- once here and once in PostPhysicsUpdate -- and only the PostPhysics
    // half was reproduced. VERIFIED in the asm, not inferred: PreSceneUpdate @0x8230D928 fetches
    // the FOUR output interfaces off its own OutputBuffer_PreScene at 0x8230E410..0x8230E434 and
    // calls UpdateOutputInterfaces at 0x8230E44C, in the same (active, global, replayActive,
    // replayGlobal) argument order the PostPhysics site uses.
    //
    // ⛔ WITHOUT IT the PRE-SCENE output buffer's RCEntityActiveRaceCarOutputInterface stays
    // Clear()ed for ever, so IsPlayerCarActive() is false there and
    // WorldModule::BridgeRaceCarModuleToWorldModule_PreScene -- the only producer of
    // WorldModule::meLocalPlayerActiveRaceCarIndex, and itself only just un-stubbed this wave --
    // correctly publishes E_ACTIVE_RACE_CAR_INDEX_INVALID every frame. MEASURED: with the bridge
    // real but this call missing, the bridge's one-shot "player active race-car index published"
    // diag never printed, while the PostPhysics publish was reporting `playerIdx 1` in the same
    // run. Two layers, both silent.
    UpdateOutputInterfaces( lpOutput->GetActiveRaceCarOutputInterface(),
                            lpOutput->GetGlobalRaceCarOutputInterface(),
                            lpOutput->GetReplayActiveRaceCarOutputInterface(),
                            lpOutput->GetReplayGlobalRaceCarOutputInterface() );

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
}

// X360 0x82307538 -- PARTIAL SLICE. The console body runs the post-physics half of the
// module (ProcessCreateVehicleEvents, the crash/takedown queues, the director vehicle
// input, the replay request interface, ...). Three of its 20-odd legs are reproduced, in the
// console's own relative order:
//   * UpdateActiveRaceCarColours -- the per-frame PAINT PUBLISH (added 2026-08-02). Nothing
//     else in the image writes RenderParams::mPaintColour / mPearlescentColour, and
//     RenderRaceCar uploads both as shader constants 20/21. Without it every car draws with
//     RenderParams::Reset()'s (1,1,1,1) -- i.e. WHITE, which is what the Hunter Cavalry was.
//   * UpdateOutputInterfaces -- the per-frame OUTPUT PUBLISH. Nothing else in the image
//     writes RCEntityActiveRaceCarOutputInterface, so without it every downstream consumer
//     (the world module's player-position latch, the scoring system, and -- through
//     BridgeWorldToDirector -- every camera behaviour's VehicleInfo) reads a Clear()ed
//     interface with mePlayerActiveRaceCarIndex == -1.
//   * SendStreamerEvents -- the load-bearing streamer drain: InternalBaseStreamer::Update
//     clears its own request ring at the top of every frame, so a load request that is not
//     drained in the frame it was posted is silently lost.
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

    // [FLAG PC bring-up] the console's ReadUpdatedActiveRaceCarDataFromPhysics runs HERE
    // and is what publishes every active car's render pose. It does not exist on this
    // build (no physics module), so the pose publish stands in for it -- see
    // PublishRenderPoseWithoutPhysicsBringUp's banner. It must run BEFORE
    // UpdateOutputInterfaces, exactly as the console's readback does.
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
        if( lpActiveRaceCar->IsActive() )
        {
            PublishRenderPoseWithoutPhysicsBringUp( lpActiveRaceCar );
        }
    }

    // ⭐ THE PAINT PUBLISH, at the console's own position. VERIFIED from the asm of
    // PostPhysicsUpdate @0x82307538: the call sequence is
    //   ReadUpdatedActiveRaceCarDataFromPhysics (0x8230761C)
    //   ... UpdateActiveRaceCarTransforms (0x82307688) ... StorePlayerRoutePortalPositions
    //   UpdateActiveRaceCarColours (0x823076C4)
    //   ... UpdateOutputInterfaces (0x8230771C) ... SendStreamerEvents (0x82307944)
    // so it sits AFTER the pose leg and BEFORE the output publish, which is exactly where
    // the two reproduced legs put it here. It is also OUTSIDE the paused branch: the
    // `bne cr6, loc_823076C0` at 0x82307610 skips the whole physics-readback run and lands
    // on the instruction pair that sets this call up, so the paint refresh runs every frame
    // whether the sim is paused or not.
    UpdateActiveRaceCarColours();

    // The console reads the four interfaces off the output buffer in this order
    // (replayGlobal, replayActive, global, active) and passes them
    // (active, global, replayActive, replayGlobal).
    UpdateOutputInterfaces( lpOutput->GetActiveRaceCarOutputInterface(),
                            lpOutput->GetGlobalRaceCarOutputInterface(),
                            lpOutput->GetReplayActiveRaceCarOutputInterface(),
                            lpOutput->GetReplayGlobalRaceCarOutputInterface() );
    SendStreamerEvents( lpOutput );
    lpOutput->UnlockForWrite();
}

// ============================================================================
// PrePhysicsUpdate  @ 0x82307160   (drivable wave 2026-08-01) -- PARTIAL SLICE.
//
// ⭐ THIS RETIRES A SILENT-DROP STUB (WorldLinkStubs.cpp:1374 -- a one-shot log that
// dropped both buffers). It is the frame slot the whole place-on-track chain ends in.
//
// The console body: assert the buffers + the player index, lock, then
//   if (paused)  assert the takedown queue is empty
//   else         CrashPlayManager::Update, UpdateActiveCars, UpdateTailgateTimer,
//                UpdateBoost, ProcessPlayerVehicleInput, ProcessTakedownEvents
//   ProcessResetOnTrackResultQueue
//   ⭐ mPlaceOnTrackManager.PrePhysicsUpdate(lpInput, lpOutput)
//   the eight-slot AI-request sweep, unlock.
//
// REPRODUCED: the asserts, the locks, and the PlaceOnTrackManager call.
// [FLAG PC bring-up] everything else is dropped, NOT paraphrased: every one of the nine
// other calls reaches an un-homed manager interior (CrashPlayManager, BoostManager, the
// takedown queues, the AI request interface).
//
// ⚠️ THE PLAYER-INDEX ASSERT (X360 :1726) IS THE CONSOLE'S OWN AND IT IS REACHABLE HERE,
// which the console's own flow guarantees it is not. mePlayerActiveRaceCarIndex stays
// E_ACTIVE_RACE_CAR_INDEX_INVALID until the junkyard reset action lands, and the world runs
// pre-physics frames before that -- so a verbatim CGS_ASSERT would HALT the game (the PC
// assert manager blocks until END) on EVERY frame of that window. It is raised ONCE, with
// this note, rather than dropped: the drop stays visible in the log and the boot survives.
// DELETE-WHEN the player car is attached before the world's first pre-physics frame.
// ============================================================================
void RaceCarEntityModule::PrePhysicsUpdate(
        RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput,
        BrnUpdateSet lUpdateSet )
{
    (void)lUpdateSet;

    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );        // X360 :1724
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );      // X360 :1725

    if( lpInput == 0 || lpOutput == 0 )
    {
        return;
    }

    // The console's :1726 assert, raised once -- see the banner.
    if( static_cast<u32>( mePlayerActiveRaceCarIndex ) >= E_ACTIVE_RACE_CAR_INDEX_COUNT )
    {
        static bool sbReportedNoPlayerCar = false;
        if( !sbReportedNoPlayerCar )
        {
            sbReportedNoPlayerCar = true;
            if( CgsDev::Log::gpDebugPrint != 0 )
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] RaceCarEntityModule::PrePhysicsUpdate: "
                       "( mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && "
                       "( mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) "
                       "(X360 BrnRaceCarEntityModule.cpp:1726) -- raised once, not per frame\n";
        }
    }

    lpInput->LockForRead();
    lpOutput->LockForWrite();

    // [FLAG PC bring-up] the paused / not-paused split and its nine calls -- see the banner.

    mPlaceOnTrackManager.PrePhysicsUpdate( lpInput, lpOutput );

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
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
//   UpdateActiveRaceCarTransforms, ReadUpdatedActiveRaceCarDataFromPhysics,
//   (UpdateActiveRaceCarColours RETIRED from this list 2026-08-02 -- it is COMPLETE above.
//    It was never a [VMX] function: its only vector work is two lvx128/stvx128 pairs that
//    copy a whole Vector4 out of the palette, which is a plain assignment in C++.)
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
