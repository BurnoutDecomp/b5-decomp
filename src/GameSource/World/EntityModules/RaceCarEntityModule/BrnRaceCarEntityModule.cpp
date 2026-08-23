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
//   ⭐ RaceCarEntityModule::ProcessPlayerVehicleInput  X360 0x822FFE30  (player-input wave
//      2026-08-11) -- the pad-to-physics hop; COMPLETE, and wired at the console's own slot
//      inside PrePhysicsUpdate. See its banner for the PPC float-arg signature and the FLAGs.
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
#include "rw/math/vpu/matrix44affine_operation.h"                                        // rw::math::vpu::IsValid(Matrix44Affine) / Mult
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec::WheelSpec (the authored wheel placements)
#include "GameSource/Physics/DeformationManager/SharedIO/BrnVehicleLocatorData.h"                // VehicleLocatorData (the rest-pose light-locator stand-in)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                 // VehiclePhysics::SeatTransformFromCreateLegBringUp (the analytic rest seat, seat wave 2026-08-05)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"             // BrnPhysics::Vehicle::BrnPlayerDriverControls (the 72-byte player record)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"       // VehicleDriverInputInterface::AddTargetAssist / GetUpdateDriverQueue
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"       // BrnWorld::BoostStrategy::IsBoosting (vtable slot 19)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"            // BrnPhysics::Vehicle::VehicleManagerOutputInterface (the create-vehicle result queue)
#include "GameSource/World/BrnEntityTypes.h"                                                 // BrnWorld::E_ENTITYTYPE_RACECAR (the VolumeInstanceId owner byte)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"                // InSceneUpdateInterface::SetVolumeInstanceTransform / SetEntityPosition / ClearEntityVolumesPadding

#include <cstring>   // memset
#include <cstdlib>   // getenv  ([motion] opt-in probe)
#include <cmath>     // sqrtf

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
    mbIsInOnlineGameMode      = false;
    mbOnlineModeJustFinished  = false;
    mbCarSelectAllowedInGameMode = false;
    mbInCarSelectScreen       = false;
    mbCarSelectDontStreamAudio = false;

    mpVehicleList        = 0;
    mpWheelList          = 0;
    mbCarColoursBound    = false;

    // The three render switches the dispatch leg reads (see the header for the offset
    // fit). The console seeds them from its debug-variable table, which is not live on
    // this build; body, coronas and wheels all default ON.
    mbRenderCarsDuringCrash = true;
    mbRenderRaceCarCoronas  = true;

    // ON since the wheel-render wave (2026-08-03). RenderRaceCar's wheel block is now
    // reconstructed, Model::SetupShaderConstantsForInstancing has a body, and the
    // instanced submission is unrolled into per-instance mesh commands in
    // DrawRenderable::Interpret, so no draw ever reaches the
    // DrawInstancedIndexedPrimitive_Custom trap.
    mbRenderWheels = true;
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
        // Breaker RaceCarEntityModule::Prepare @0x82303FB0..0x82303FB8. This
        // establishes the selected B5 strategy before the first physics frame.
        mBoostManager.Prepare();
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

        // ⭐⭐ [teleport] THE ALREADY-ACTIVE RE-RESET ARM, 2026-08-21 (gateui r9). It used to be
        // a bare `return` with the banner's "[FLAG PC bring-up] ... UNREACHABLE on this build:
        // nothing re-requests placement for a car that is already ACTIVE". It IS reached now --
        // the harness teleport (BrnPlaceOnTrackManager.cpp, the [teleport] block) issues exactly
        // such a request -- and this arm is the ONLY way a live car's transform can be written.
        //
        // WHAT THE CONSOLE DOES HERE (asm 0x822F4990..0x822F4B2C), and what is reproduced:
        //   0x822F4990  lbz r11, 0x52A(car)   -- mPhysicsState.mbCrashing
        //   crashing:                                                     ⛔ PARKED, see below
        //     IsDriveableAfterCrash()        -> resetTransform=0, resetDeformation=0
        //     else IsDeformationFixedAfterCrash() -> resetTransform=1, resetDeformation=1,
        //                                            resettingAfterWreck = IsPlayer()
        //     else                           -> resetTransform=1, resetDeformation=0
        //   NOT crashing (0x822F4A00):
        //     resetTransform      = 1        (`li r26, 1` at 0x822F4960, never overwritten)
        //     resetDeformation    = IsWrecked()   (the cntlzw/extrwi/xori idiom at 0x822F4A04..10)
        //     resettingAfterWreck = 0        (`mr r22, r25` with r25 == 0)
        //   0x822F4A14  the deformation-amount pair: when this slot IS the player's and the
        //               module's one-shot request byte (+99548) is set, resetDeformation is
        //               forced and the byte cleared; otherwise the reset TYPE comes from +99536
        //               and the amount from +99544. Defaults (nothing pending) are -1 / 0.0f.
        //   0x822F4B2C  VehicleInputInterface::ResetRaceCar(...)                 ⭐ REPRODUCED
        //   0x822F4B30+ the module's reset BitArray bit (+65760) and
        //               ActiveRaceCar::ResetAfterCrash @0x822BF3A0                ⛔ PARKED
        //
        // ⛔ PARK 1 -- the CRASHING classification. ActiveRaceCar::IsDriveableAfterCrash and
        //    ::IsDeformationFixedAfterCrash have no declaration or body anywhere in this tree;
        //    guessing either would decide whether a wrecked car's transform is reset at all.
        //    A crashing car is therefore left alone, LOUDLY.
        // ⛔ PARK 2 -- the three unnamed module members at +99536/+99544/+99548 (the deformation
        //    reset type, its amount, and its one-shot flag) are inside maTailPadB1 and unnamed,
        //    exactly as this function's own banner records. Their NOTHING-PENDING values are the
        //    console's own initialisers -- type -1 (`li r29, -1` @0x822F4A24) and amount
        //    flt_82001CC0 == 0.0f (@0x822F4A2C) -- and those are what is passed. That is the
        //    console's behaviour whenever no deformation reset is queued, which is every frame
        //    outside a crash.
        // ⛔ PARK 3 -- the reset BitArray + ResetAfterCrash, unchanged from the banner above.
        //    Neither is on the transform path: the bit is read by the deformation legs and
        //    ResetAfterCrash re-seats crash bookkeeping.
        if( lpActiveRaceCar->IsCrashing() )
        {
            static bool sbLoggedCrashingReset = false;
            if( !sbLoggedCrashingReset && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedCrashingReset = true;
                *CgsDev::Log::gpDebugPrint
                    << "[teleport] ResetActiveRaceCar PARK: a CRASHING active car was re-requested;"
                       " the console's classification needs ActiveRaceCar::IsDriveableAfterCrash /"
                       " ::IsDeformationFixedAfterCrash, neither of which exists in this tree --"
                       " the car is left where it is\n";
            }
            return;
        }

        RaceCar* lpGlobalRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
        if( lpGlobalRaceCar == 0 || mpVehicleList == 0 || lpVehicleInputInterface == 0 )
        {
            return;
        }

        // 0x822F493C/0x822F4950 -- the model index the console hands to ResetRaceCar's r6 slot.
        // Nothing reads it (see that function's banner); it is passed because the console does.
        const s32 liResetModelIndex =
            mpVehicleList->GetVehicleIndex( lpGlobalRaceCar->GetModelId() );

        const bool lbResetTransform      = true;                             // r26
        const bool lbResetDeformation    = lpActiveRaceCar->IsWrecked();     // r24
        const bool lbResettingAfterWreck = false;                            // r22
        const f32  lfHowCloseToTotalled  = 0.0f;                             // f31, flt_82001CC0
        const BrnPhysics::Deformation::DeformationResetType leDeformationResetType =
            static_cast<BrnPhysics::Deformation::DeformationResetType>( -1 );   // r29

        // 0x822F4B0C `vspltisw v2, 0` -- the ANGULAR velocity argument is a hard zero; only the
        // linear velocity this call was handed travels through.
        const Vector3 lZeroAngularVelocity = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

        lpVehicleInputInterface->ResetRaceCar(
            static_cast<u32>( leActiveRaceCarIndex ),
            lrTransform, lrVelocity, lZeroAngularVelocity,
            static_cast<u8>( liResetModelIndex ),
            lbResetTransform, lbResetDeformation,
            lfHowCloseToTotalled, lbResettingAfterWreck,
            leDeformationResetType );

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[teleport] ResetActiveRaceCar RE-RESET car "
                << static_cast<s32>( leActiveRaceCarIndex ) << " -> road ("
                << lrTransform.wAxis.x << ", " << lrTransform.wAxis.y << ", "
                << lrTransform.wAxis.z << ") vel (" << lrVelocity.x << ", " << lrVelocity.y
                << ", " << lrVelocity.z << ") resetDeform="
                << ( lbResetDeformation ? 1 : 0 ) << "\n";
        }
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

    // [FLAG PC bring-up] the OTHER half of that create event. AddHandlingModel has just
    // published lrTransform to the vehicle manager; on the console the next tick's
    // RaceCarState carries back the transform the create leg PRODUCED -- which is NOT the
    // raw ground transform: RaceCarPhysics::Prepare @0x82639CB8 runs the ANALYTIC SEAT
    // (VehiclePhysics::SetTransformFromPositionOnRoad @0x825D1C00) over it, planting the
    // handling-frame origin `radius1 - localY1 - 0.035` above the road point. Seeding the
    // RAW transform here (as this build did until the seat wave 2026-08-05) is what put the
    // car in the ground: the render composition body = COM * physics subtracts 0.740575 from
    // whatever is seeded, so the body origin sat ~0.74 BELOW the road.
    //
    // Now the seat runs at the seam (the create leg's own math over the resident spec's own
    // data -- see VehiclePhysics::SeatTransformFromCreateLegBringUp), and the SHIPPED
    // model-space->handling-space matrix (spec+1552) lands in mCentreOfMassTransform so the
    // two halves compose: physics ~= ground + 1.4459, body ~= ground + 0.7054 (PUSMC01).
    // DELETE-WHEN ReadUpdatedActiveRaceCarDataFromPhysics is wired to a real producer.
    {
        const RaceCarStreamer::PhysicsResourcePtr& lrSeatPhysicsResource =
            mRaceCarStreamer.GetPhysicsResourceBringUp(
                static_cast<s32>( lpActiveRaceCar->GetActiveRaceCarIndex() ) );

        if( lrSeatPhysicsResource.HasMemoryResource() )
        {
            const BrnPhysics::Deformation::StreamedDeformationSpec* lpSeatSpec =
                lrSeatPhysicsResource.operator->();

            const Matrix44Affine lSeatedTransform =
                BrnPhysics::Vehicle::VehiclePhysics::SeatTransformFromCreateLegBringUp(
                    lpSeatSpec, lrTransform );

            lpActiveRaceCar->SetCentreOfMassTransformBringUp(
                lpSeatSpec->mCarModelSpaceToHandlingBodySpaceTransform );

            // [FLAG PC bring-up] OnResourcesLoaded @0x822EB2FC leg 2, landed at this promote
            // seam exactly like the +1552 matrix above (wheel-transform wave 2026-08-13): the
            // console sets the four render wheel SCALES once at resource load, from the same
            // streamed spec (spec + 96 + 48*i == maWheelSpecs[i].mScale). Needed here because
            // the per-frame wheel stand-in (which also published scale) no longer runs for
            // physics-owned cars now that the real transform producer is landed -- without
            // this the wheels draw unit-scale. DELETE-WHEN OnResourcesLoaded's alias leg lands.
            for( s32 liScaleWheel = 0; liScaleWheel < 4; ++liScaleWheel )
            {
                const BrnPhysics::Deformation::WheelSpec* lpScaleWheelSpec =
                    lpSeatSpec->GetWheelSpec( liScaleWheel );
                if( lpScaleWheelSpec != 0 )
                {
                    lpActiveRaceCar->GetRenderParams()->SetWheelScale(
                        static_cast<u32>( liScaleWheel ), lpScaleWheelSpec->mScale );
                }
            }

            lpActiveRaceCar->SeedPhysicsStateFromCreateEventBringUp( lSeatedTransform );

            // ---- WITNESS PRINTS (both ends of the seat, every promote) -----------------
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                const BrnPhysics::Deformation::WheelSpec* lpWheel1 = lpSeatSpec->GetWheelSpec( 1 );
                const Matrix44Affine& lrM1552 =
                    lpSeatSpec->mCarModelSpaceToHandlingBodySpaceTransform;
                *CgsDev::Log::gpDebugPrint
                    << "[seat] car " << static_cast<s32>( leActiveRaceCarIndex )
                    << " ground y " << lrTransform.wAxis.y
                    << " -> physics y " << lSeatedTransform.wAxis.y
                    << " | radius1 " << 0.5f * lpWheel1->mScale.y
                    << " specY1 " << lpWheel1->mPosition.y
                    << " COMeff (" << lpSeatSpec->mMeshOffset.x << ", "
                    << lpSeatSpec->mMeshOffset.y << ", " << lpSeatSpec->mMeshOffset.z
                    << ") -M1552.w (" << -lrM1552.wAxis.x << ", " << -lrM1552.wAxis.y
                    << ", " << -lrM1552.wAxis.z << ")\n";
            }
        }
        else
        {
            // Pre-seat behaviour, kept as the loud fallback: without the spec there are no
            // wheel radii to seat with, so the raw ground transform is seeded and SAID SO.
            lpActiveRaceCar->SeedPhysicsStateFromCreateEventBringUp( lrTransform );
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[seat] car " << static_cast<s32>( leActiveRaceCarIndex )
                    << " SPEC NOT RESIDENT -- raw ground transform seeded, car will sit low\n";
            }
        }
    }

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
// physics def is read, mCentreOfMassTransform is the IDENTITY Prepare/Attach left behind,
// so for a stationary car the console's own CalcBodyTransform here produces bit-for-bit
// what UpdatePhysicsState would have stored. Nothing about the pose is invented: the maths
// is the console's and the transform is the one the placement published.
//
// ⛔ CORRECTED 2026-08-02 (car-placement wave). This banner used to say the transform was
// "what Attach seeded from RaceCar::GetTransform()", and treated that as equivalent. It is
// NOT: Attach seeds the SPAWN ANCHOR, and the console's place-on-track line test then moves
// the car onto the ground before ResetActiveRaceCar publishes it. Reading the Attach seed
// rendered the junkyard car 4.534 m above the junkyard floor for as long as this helper has
// existed. ResetActiveRaceCar now closes the create-event round trip explicitly
// (ActiveRaceCar::SeedPhysicsStateFromCreateEventBringUp), so mPhysicsState.mTransform is
// the placement's transform, which is what the console's readback would carry.
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
void RaceCarEntityModule::PublishRenderPoseWithoutPhysicsBringUp( ActiveRaceCar* lpActiveRaceCar,
                                                                  s32 liActiveRaceCar )
{
    ActiveRaceCar::RenderParams* lpRenderParams = lpActiveRaceCar->GetRenderParams();

    lpRenderParams->SetLOD( CgsGraphics::Model::E_STATE_LOD_0 );
    lpRenderParams->SetDamaged( false );

    Matrix44Affine lBodyTransform;
    lpActiveRaceCar->CalcBodyTransform( lBodyTransform );

    // ⛔ THE "GRAPHICS-FRAME STEP" IS GONE, 2026-08-17 -- IT WAS THE CAR-WARPING BUG.
    //
    // This used to publish `Mult( spec.mCarModelSpaceToHandlingBodySpaceTransform,
    // lBodyTransform )` -- the shipped +1552 matrix applied ONCE MORE on top of
    // CalcBodyTransform -- under a FLAG admitting the console mechanism for it "is NOT yet
    // recovered" and that it was a fit measured against a stationary car ("a 1 mm fit").
    //
    // IT IS NOT A CONSOLE MECHANISM. Read off the ARTIST asm, both ends:
    //
    //   * ActiveRaceCar::UpdatePhysicsState @0x822D4418 -- the console's ONLY producer of
    //     mBodyTransform -- calls CalcBodyTransform into a stack matrix (@0x822D47A4) and
    //     copies its four rows STRAIGHT into `this + 0x7E0` (== &mRenderParams, and
    //     mBodyTransform is RenderParams+0) at 0x822D47A8-DC. No second factor, no table.
    //   * RenderRaceCar @0x822CF6A0 composes each part as world = Mult( partLocator,
    //     mBodyTransform ) -- the vmulfp/vmaddfp cascade at 0x822D0190-0x822D0230, with the
    //     body's four rows held in v124/v126/v125/v117 and the locator rows loaded from
    //     spec+20. The GraphicsSpec's mppRigidBodyToSkinMatrixTransforms table (spec+32),
    //     which the old FLAG offered as the possible carrier, is NOT REFERENCED ANYWHERE in
    //     that function -- searched: zero loads of spec+32 or spec+28 in all 2008 lines.
    //
    // So the console applies the +1552 matrix EXACTLY ONCE, and it applies it as the CENTRE
    // OF MASS TRANSFORM inside CalcBodyTransform @0x822B8828 (`Mult( mCentreOfMassTransform,
    // mPhysicsState.mTransform )`). Our promote seam already seeds mCentreOfMassTransform
    // with that very matrix (SetCentreOfMassTransformBringUp, this file), so the multiply
    // below was applying it a SECOND time.
    //
    // WHY IT LOOKED RIGHT AND WAS STILL WRONG: an extra RIGID factor cannot deform anything,
    // so on a stationary, axis-aligned car it reads as a small offset -- which is exactly
    // what the original measurement tuned away. But the WHEELS deliberately did NOT get it
    // (the old FLAG says so in its own last sentence: "The WHEELS keep composing against the
    // MODEL frame"). Body and wheels were therefore in two different frames, and the moment
    // the car ROTATES the extra factor swings the shell against its own wheels -- the car
    // visibly coming apart. The car-select carousel rotates it continuously.
    //
    // Publishing CalcBodyTransform verbatim is what UpdatePhysicsState does, and it puts the
    // body back in the same frame the wheels have always been composed in.
    lpRenderParams->SetBodyTransform( lBodyTransform );

    // ---- THE WHEEL POSE -----------------------------------------------------
    // ⭐ SPLIT OUT 2026-08-12 (carrender wave). It used to be inline here, which meant the
    // BODY half and the WHEEL half shared one gate -- and that is exactly how the wheels
    // vanished. See PublishWheelPoseWithoutPhysicsBringUp's banner.
    PublishWheelPoseWithoutPhysicsBringUp( lpActiveRaceCar, liActiveRaceCar );
}

// ============================================================================
// [FLAG PC bring-up] PublishWheelPoseWithoutPhysicsBringUp -- NOT an X360 function.
//
// ⭐⭐ WHY THIS IS ITS OWN FUNCTION NOW (carrender wave 2026-08-12) -- A GATE-SCOPE BUG.
// This code is unchanged; only WHO CALLS IT and WHEN has changed, and that was the whole
// defect. It used to be the tail of PublishRenderPoseWithoutPhysicsBringUp, so it shared
// that function's gate: `IsActive() && !mUsedRaceCars.IsBitSet(i)`.
//
// PublishRenderPoseWithoutPhysicsBringUp stands in for TWO different console producers:
//   * the BODY pose  <- ActiveRaceCar::UpdatePhysicsState @0x822D4418   -- LANDED 2026-08-11
//   * the WHEEL pose <- the SetWheelTransform publish in the physics half -- LANDED 2026-08-13
// mUsedRaceCars answers "does physics own this race-car slot", which is the right question
// for the first and the WRONG question for the second. When VehicleManager::ProcessCreateEvents
// mounted on 2026-08-11 and started setting that bit, the gate correctly retired the body
// stand-in -- and silently retired the wheel stand-in with it, whose producer had not landed.
//
// MEASURED, on the run that motivated this split (BrnGame.log, carrender probe):
//   [carrender]  wheel 0 exists 0 T (0.000000, 0.000000, 0.000000) scaleDiag (1.000000, 1.000000, 1.000000)
//   ... identically for wheels 1..3, and
//   [racecar-wheels] RenderRaceCar wheel block outcome 3 (... 3 no wheel exists ...)
// i.e. the render leg drew NO WHEELS AT ALL, on a car that had drawn them correctly on
// 2026-08-05. Two services were lost on that one switch: SetWheelExists AND SetWheelScale
// (the scale matrix fell back to RenderParams::Reset()'s identity -- see the `scaleDiag`).
//
// ⭐⭐ THE REAL PRODUCER LANDED 2026-08-13 (wheel-transform wave):
// SimpleVehiclePhysics::GetWheelsWorldTransfrom @0x825D8878 is BODIED and
// WriteOutVehicleStats' four SetWheelTransform calls are UNPARKED, so every car PHYSICS
// OWNS gets real per-wheel world matrices (spin + steer + suspension) through
// UpdatePhysicsState. The old DELETE-WHEN's second condition was resolved by a full-image
// scan: RaceCarState::mabWheelExists (+0x446) has NO writer anywhere in the XEX (only the
// copy ctor @0x8220A4C0 propagates it) -- on console the render-side exists comes from the
// DEFORMATION half (L3 -> UpdateWheelPhysicsState @0x822B8738), still parked, so the
// readback loop forces exists true for physics-owned cars ([FLAG] at that call site).
//
// ⚠️ WHY THIS STAND-IN STILL EXISTS: it now covers ONLY the slots physics does NOT own
// (called solely from PublishRenderPoseWithoutPhysicsBringUp's tail, whose call sites gate
// on !mUsedRaceCars / no input buffer) -- a car with no simulation still needs its wheels
// drawn at the authored rest pose, e.g. before the create drain runs. For physics-owned
// cars it no longer runs at all.
// DELETE-WHEN the body-pose stand-in above it is deleted (they retire together).
// ============================================================================
void RaceCarEntityModule::PublishWheelPoseWithoutPhysicsBringUp( ActiveRaceCar* lpActiveRaceCar,
                                                                 s32 liActiveRaceCar )
{
    ActiveRaceCar::RenderParams* lpRenderParams = lpActiveRaceCar->GetRenderParams();

    Matrix44Affine lBodyTransform;
    lpActiveRaceCar->CalcBodyTransform( lBodyTransform );

    const RaceCarStreamer::PhysicsResourcePtr& lrPhysicsResource =
        mRaceCarStreamer.GetPhysicsResourceBringUp( liActiveRaceCar );

    // ⭐ THE FRAME IS UNCHANGED BY THE SPLIT, AND THAT MATTERS. The wheels compose against
    // CalcBodyTransform()'s MODEL-frame output -- which is bit-for-bit the matrix the inline
    // version used (its `lBodyTransform` local) AND bit-for-bit what UpdatePhysicsState now
    // publishes as the body transform (BrnActiveRaceCar.cpp:449-450 stores CalcBodyTransform
    // directly). So a wheel drawn through the readback path lands in exactly the frame it
    // landed in on 2026-08-05, when these wheels were last seen seated in their arches.

    // The same stand-in, for the same missing producer. UpdatePhysicsState /
    // UpdateWheelPhysicsState are the console's ONLY writers of mWheelTransforms[] and
    // mabWheelExists[], and both read the physics snapshot -- so with no physics module
    // every wheel reports "does not exist" and RenderRaceCar's wheel block, faithful or
    // not, draws nothing. (MEASURED: "[racecar-wheels] ... outcome 3" -- no wheel exists.)
    //
    // WHAT IS HONEST ABOUT IT. The positions and scales are NOT invented: they are the
    // car's own authored WheelSpecs, read out of the streamed deformation spec the
    // streamer already has resident on this build (E_LOADFLAG_LOADEDPHYSICS -- the log's
    // "STRM: Physics loaded: N"). That is the same table the console feeds the suspension
    // from, and the same one OnResourcesLoaded @0x822EB2FC reads its four
    // RenderParams::SetWheelScale arguments out of (spec + 96 + 48*i == maWheelSpecs[i].mScale).
    // The composition -- wheel world = wheelLocal * bodyTransform -- is the body-part
    // loop's own, because a WheelSpec position is in the same car model space as a
    // GraphicsSpec part locator.
    //
    // WHAT IS A LIE, stated plainly:
    //   * NO SUSPENSION, NO STEERING, NO SPIN. The console's wheel transform carries the
    //     suspension travel, the steer angle and the rolling rotation the vehicle sim
    //     produces each frame. This publishes the wheel at its rest position with the
    //     body's own orientation, which is exactly right for a car that is not moving --
    //     and this car is not moving, because nothing simulates it.
    //   * mabWheelExists is forced TRUE for all four. On the console that byte is the
    //     wheel's ON-GROUND flag from the physics snapshot, so a wheel torn off in a crash
    //     stops drawing. Nothing here can tear a wheel off yet.
    // DELETE-WHEN ReadUpdatedActiveRaceCarDataFromPhysics + UpdateWheelPhysicsState land,
    // together with the body-pose stand-in above.
    // (lrPhysicsResource is fetched above, where the graphics-frame step needs it first.)
    if ( lrPhysicsResource.HasMemoryResource() )
    {
        const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec =
            lrPhysicsResource.operator->();

        for ( s32 liWheel = 0; liWheel < 4; ++liWheel )
        {
            const BrnPhysics::Deformation::WheelSpec* lpWheelSpec =
                lpSpec->GetWheelSpec( liWheel );
            if ( lpWheelSpec == 0 )
            {
                continue;
            }

            Matrix44Affine lWheelLocal;
            lWheelLocal.SetIdentity();
            lWheelLocal.wAxis.x = lpWheelSpec->mPosition.x;
            lWheelLocal.wAxis.y = lpWheelSpec->mPosition.y;
            lWheelLocal.wAxis.z = lpWheelSpec->mPosition.z;

            // ⭐⭐ THE WHEEL COMPOSES AGAINST THE HANDLING-BODY FRAME, NOT THE MODEL FRAME
            // (corrected 2026-08-12, carrender wave -- MEASURED, and it is a one-matrix
            // change, not a tuned offset).
            //
            // This used to be Mult(lWheelLocal, lBodyTransform), i.e. against
            // CalcBodyTransform()'s output == Mult(mCentreOfMassTransform, physicsTransform).
            // That put every wheel EXACTLY ONE model->handling step below its own arch:
            //   [carrender-y] bodyDraw y -3.534189 wheel0 y -3.966501 archPartWorldY -3.227483
            //   arch - wheel = 0.739018, against M1552's own translation 0.740575 -- 1.6 mm.
            // The car draws its arches at ground + 0.297517 against the seat wave's measured
            // target of ground + 0.290 (a 7 mm fit), so the BODY is right and the WHEEL was
            // the thing carrying the spurious step.
            //
            // WHY THE HANDLING-BODY FRAME IS THE RIGHT ONE, and not just the one that fits:
            // a WheelSpec is read out of the StreamedDeformationSpec -- the SAME table the
            // suspension solver is fed from -- and the suspension works in the handling-body
            // frame. mPhysicsState.mTransform IS that frame; CalcBodyTransform is that frame
            // pushed down into the graphics model frame by mCentreOfMassTransform. Using the
            // undisplaced one removes the step rather than cancelling it with a constant.
            // CROSS-CHECK, independent of the arch: predicted wheel centre
            //   -3.534189 + 0.740575 - 0.409029 = -3.202643
            // against ground + tyre radius = -3.525000 + 0.331333 = -3.193667 -- a 9 mm fit,
            // the same order as the arch's 7 mm, and the radius itself is confirmed by the
            // authored scale this loop publishes (0.662665 / 2 == 0.331333).
            lpRenderParams->GetWheelTransform( static_cast<u32>( liWheel ) ) =
                rw::math::vpu::Mult( lWheelLocal,
                                     lpActiveRaceCar->GetPhysicsState()->mTransform );
            lpRenderParams->SetWheelScale( static_cast<u32>( liWheel ), lpWheelSpec->mScale );
            lpRenderParams->SetWheelExists( static_cast<u32>( liWheel ), true );
        }

        // [DIAG wheel wave] the four authored wheel placements, printed once. Latched on
        // the FIRST WHEEL'S POSITION, not on a "printed once" bool: if the spec pointer
        // ever resolves to a different car the numbers change and the line reprints, and
        // a run where the positions are all zero is immediately distinguishable from a
        // run where this never executed at all.
        {
            static f32 sfLastLoggedWheel0X = 1e30f;
            const BrnPhysics::Deformation::WheelSpec* lpWheel0 =
                lpSpec->GetWheelSpec( 0 );
            if ( lpWheel0 != 0 && lpWheel0->mPosition.x != sfLastLoggedWheel0X
                 && CgsDev::Log::gpDebugPrint != 0 )
            {
                sfLastLoggedWheel0X = lpWheel0->mPosition.x;
                // [seat wave 2026-08-05] the OTHER end of the seat's witness pair: what the
                // renderer actually composes. physics = the seeded handling-frame transform,
                // model = COM * physics (CalcBodyTransform), bodyDraw = M1552 * model (the
                // graphics-frame step). Expected: model = physics - 0.740575, bodyDraw =
                // model - 0.740575 (z +0.170226 each step).
                *CgsDev::Log::gpDebugPrint
                    << "[seat-pose] car " << liActiveRaceCar
                    << " physics y " << lpActiveRaceCar->GetPhysicsState()->mTransform.wAxis.y
                    << " model y " << lBodyTransform.wAxis.y
                    << " bodyDraw y " << lpRenderParams->GetBodyTransform().wAxis.y << "\n";
                *CgsDev::Log::gpDebugPrint << "[racecar-wheels] authored WheelSpecs for car "
                                           << liActiveRaceCar << ":";
                for ( s32 liLog = 0; liLog < 4; ++liLog )
                {
                    const BrnPhysics::Deformation::WheelSpec* lpLog =
                        lpSpec->GetWheelSpec( liLog );
                    if ( lpLog == 0 ) { continue; }
                    *CgsDev::Log::gpDebugPrint
                        << " pos(" << lpLog->mPosition.x << ", " << lpLog->mPosition.y
                        << ", " << lpLog->mPosition.z << ") scale(" << lpLog->mScale.x
                        << ", " << lpLog->mScale.y << ", " << lpLog->mScale.z << ")";
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }
        }
    }
}


// ============================================================================
// [FLAG PC bring-up] PublishRestPoseLightLocatorsBringUp -- NOT an X360 function.
// The REST-POSE LIGHT LOCATORS: the input RaceCarEntityModule::SubmitCoronasForRaceCar
// @0x822D1600 has nothing to do without.
//
// ⛔ WHY IT EXISTS -- the console's producer is a THREE-HOP CHAIN AND ALL THREE HOPS ARE
// DEAD ON THIS BUILD. Each is named, each is already logged at boot, none is guessed at:
//   (1) DeformableObject::PrepareLocators @0x825BA010 copies the streamed spec's three tag
//       lists into the car's live VehicleLocatorData. Committed
//       (BrnDeformableObject_Lifecycle.cpp) with `const u32 luNumLight = 0;   // FLAG:
//       = mpDeformationSpec->mLightTags.GetNumLocatorPoints()` -- the loop body is exact,
//       the SOURCE is pinned to an empty list, so miNumLightLocators comes out 0. (That FLAG
//       is now STALE, by the way: StreamedDeformationSpec's three LocatorPointSpecList
//       members are PUBLIC, and LocatorPointSpecList::GetNumLocatorPoints/GetLocatorSpec are
//       public inlines -- the same staleness the 2026-08-14 walls wave already found and
//       fixed for the tag/driven-point accessors. See the report's CROSS-GROUP list.)
//   (2) DeformationManager::OutputData @0x826225D8 publishes those tables into the
//       entity-module output interface. Its committed body's first pass writes NO locator
//       records ("GROW DEFERRAL ... the three count-bound asserts AND the table writes are
//       deliberately NOT emitted here"), and its TU is not even mounted:
//       BrnGame.log:793 "conductor gate: DeformationManager::OutputData @0x826225D8 (339;
//       real body in the unmounted BrnDeformationManager_Output.cpp) inert".
//   (3) leg L5 of ReadUpdatedActiveRaceCarDataFromPhysics copies them into RenderParams:
//       BrnGame.log:808 "[physics-readback] PARKED deformation legs ... locator-output copy
//       ...".
// With no stand-in, RenderParams::miNumLightLocators is 0 for every car forever, the corona
// producer's loop runs zero times, and the whole subsystem looks healthy while drawing
// nothing -- the exact "the frame looks identical" trap this campaign keeps paying for.
//
// WHAT IS HONEST ABOUT IT:
//   * NOTHING IS INVENTED. Every position and every tag type is read out of the car's own
//     shipped StreamedDeformationSpec::mLightTags -- the same resource the wheel stand-in
//     above already reads its WheelSpecs from, resident on this build ("STRM: Physics loaded:
//     0" and the four real authored wheel scales in BrnGame.log prove the spec parses).
//   * It publishes what the console's own DeformableObject::UpdateLocator @0x825E0EC8 yields
//     for an UNDAMAGED car. That function copies the spec's four locator rows verbatim and
//     then ADDS the car's verlet skin-point displacement to the translation row
//     (`_R8 = 16 * (*(_R28 + 70) + 270); lvx128 v0, r8, r29; vaddfp; stvx128`) -- which is
//     zero at rest -- before the detached-part re-basing arm, which needs a detached part.
//   * It goes through the REAL consumer: RenderParams::SetLightLocators, itself recovered
//     from L5's own inlined asm. Only the INPUT is stood in for.
//
// WHAT IS A LIE, stated plainly: the locators never MOVE. A crushed front wing does not drag
// its head-lamp flare with it, and a torn-off part keeps its lamp floating where the part
// used to be, because both of those come from hops (1)-(3).
// DELETE-WHEN leg (3) unparks -- and it MUST be deleted then, or it will overwrite the
// deformed positions with the rest pose every single frame.
// ============================================================================
void RaceCarEntityModule::PublishRestPoseLightLocatorsBringUp( ActiveRaceCar* lpActiveRaceCar,
                                                               s32 liActiveRaceCar )
{
    // [FLAG PC bring-up] the BringUp resource getter, for the same reason the wheel stand-in
    // above uses it: the console's GetPhysicsResource asserts IsRaceCarLoaded() (ALL five
    // resource bits) and would fire a dev assert every frame on this build.
    const RaceCarStreamer::PhysicsResourcePtr& lrPhysicsResource =
        mRaceCarStreamer.GetPhysicsResourceBringUp( liActiveRaceCar );

    // The table is published on EVERY path (verify_coronaproducer F5): RenderParams::Reset()
    // does not clear miNumLightLocators, so a slot recycled to a car whose spec is not (yet)
    // resident would otherwise keep the previous car's lamp inventory. A spec-less car
    // publishes ZERO locators -- which is what the console's zeroed mLocatorData means.
    BrnPhysics::Deformation::VehicleLocatorData lLocators;
    lLocators.miNumCameraLocators  = 0;
    lLocators.miNumGenericLocators = 0;
    lLocators.miNumLightLocators   = 0;

    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec =
        lrPhysicsResource.HasMemoryResource() ? lrPhysicsResource.operator->() : 0;

    // The streamed light-tag list. muSlot is the resource's own 32-bit pointer slot, rebased
    // in place by StreamedDeformationSpec::FixUp @0x82630E18 -- a zero slot means the car
    // authored no light tags at all, and GetLocatorSpec would then index off null.
    const u32 luNumSpecLocators =
        ( lpSpec != 0 && lpSpec->mLightTags.mpaLocatorPoints.muSlot != 0 )
            ? lpSpec->mLightTags.GetNumLocatorPoints() : 0u;

    for ( u32 luLocator = 0; luLocator < luNumSpecLocators; ++luLocator )
    {
        // PrepareLocators' own bound (its assert is "mLocatorData.miNumLightLocators <
        // KI_MAX_LIGHT_LOCATORS"); a spec with more than 24 light tags is malformed, so stop
        // rather than run off the fixed array.
        if ( lLocators.miNumLightLocators >= BrnPhysics::Deformation::KI_NUM_LIGHT_LOCATORS )
        {
            break;
        }

        const BrnPhysics::Deformation::LocatorPointSpec* lpLocatorSpec =
            lpSpec->mLightTags.GetLocatorSpec( luLocator );
        if ( lpLocatorSpec == 0 )
        {
            continue;
        }

        lLocators.maLightLocators[ lLocators.miNumLightLocators ]     = lpLocatorSpec->mLocatorMatrix;
        lLocators.maLightLocatorTypes[ lLocators.miNumLightLocators ] = lpLocatorSpec->meTagPointType;
        ++lLocators.miNumLightLocators;
    }

    lpActiveRaceCar->GetRenderParams()->SetLightLocators( &lLocators );

    // [DIAG coronas step 1] the car's authored lamp inventory, printed once per distinct car
    // (latched on the first locator's X, the same trick the wheel stand-in above uses, so a
    // run where every position is zero is distinguishable from a run where this never ran).
    {
        static f32 sfLastLoggedLocator0X = 1e30f;
        if ( lLocators.miNumLightLocators > 0
             && lLocators.maLightLocators[0].wAxis.x != sfLastLoggedLocator0X
             && CgsDev::Log::gpDebugPrint != 0 )
        {
            sfLastLoggedLocator0X = lLocators.maLightLocators[0].wAxis.x;
            *CgsDev::Log::gpDebugPrint
                << "[corona-locators] car " << liActiveRaceCar << ": "
                << lLocators.miNumLightLocators << " rest-pose light locators of "
                << luNumSpecLocators << " authored (type:pos)";
            for ( s32 liLog = 0; liLog < lLocators.miNumLightLocators; ++liLog )
            {
                *CgsDev::Log::gpDebugPrint
                    << " " << static_cast<s32>( lLocators.maLightLocatorTypes[liLog] )
                    << ":(" << lLocators.maLightLocators[liLog].wAxis.x
                    << ", " << lLocators.maLightLocators[liLog].wAxis.y
                    << ", " << lLocators.maLightLocators[liLog].wAxis.z << ")";
            }
            *CgsDev::Log::gpDebugPrint << " [FLAG PC bring-up]\n";
        }
    }
}


// ============================================================================
// [RETIRED 2026-08-18, wave Q5 finisher] PublishNewVehicleToDirectorWithoutPhysicsBringUp
// stood here: a NOT-an-X360 function that published the director NewVehicle event on an
// invented trigger (a function-local edge latch on the player slot plus an attribute-residency
// retry) because RaceCarEntityModule::ProcessCreateVehicleEvents @0x822FF620 was parked one
// declaration wide and published nothing.
//
// It is DELETED, not merely unhooked: ProcessCreateVehicleEvents is now complete (see its
// banner below), and its player arm posts the SAME two values -- VehicleList::GetVehicleData(
// GetVehicleIndex(RaceCar::GetModelId()))->GetAttribCollectionKeyHash() and that same model
// index -- through the SAME callee (BrnDirectorVehicleInputInterface::NewVehicle, the real
// @0x822CBA90 body), in the SAME PostPhysicsUpdate slot, on the CONSOLE's own trigger. Keeping
// both would post the director a duplicate NewVehicleEvent the console never emits.
//
// The stand-in's residency gate is not lost, it is superseded: it existed because the invented
// trigger fired ~150 log lines BEFORE the car's attribute vault was streamed. The console
// trigger cannot: the create event is posted by ActiveRaceCar::AddHandlingModel from
// ResetActiveRaceCar, i.e. downstream of OnRaceCarResourcesLoaded (measured on this build --
// build/game/BrnGame.log has the vault 109b0d7b00000000 registered well before
// VehicleManager::ProcessCreateEvents runs).
//
// ⚠️ FOR THE CONDUCTOR: five READ-ONLY files still name this function in prose comments and are
// now stale by one sentence each -- BrnBehaviourGameplayExternal.h:509,
// BrnDirectorVehicleInputInterface.cpp:23, GameBridgeWorldToX.cpp:207 and
// BrnPhysicsModuleUpdateFunctions.cpp:70 / :1007 (five sites, four files) -- plus
// BrnRaceCarEntityModuleIO.cpp:446. Reported, not edited: none of them is this owner's file.
// (BrnActiveRaceCar_wQ5_01.cpp's reference WAS updated -- that partfile is this owner's.)
// ============================================================================


// ============================================================================
// ⚠️ FLAG PC QUALITY-OF-LIFE -- NOT an X360 function.
//
// ApplyRenderPoseInterpolationBringUp: once per RENDERED frame, before the dispatch pass,
// write each active car's display pose as the blend of the last two simulation ticks.
//
// The simulation is paced at a fixed 60 Hz while the renderer runs free, so without this
// the car steps 60 times a second inside a 140-frame-a-second stream and shudders against
// a world that moves smoothly with the camera. Idempotent -- the frame's several dispatch
// passes (main view, three shadow cascades, the env-map faces) may each call it.
// ============================================================================
void RaceCarEntityModule::ApplyRenderPoseInterpolationBringUp( f32 lfAlpha )
{
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar& lrActiveRaceCar = maActiveRaceCars[liCar];
        if( lrActiveRaceCar.IsActive() )
        {
            lrActiveRaceCar.ApplyRenderPoseInterpolation( lfAlpha );
        }
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
// DetachActiveRaceCar  @ 0x822FEDF8   (ghost-car wave 2026-08-17)
//
// The exact inverse of AttachActiveRaceCar above: release the slot's streamed assets, tell
// the AI module the car is no longer simulated, and Detach the slot itself. Two console
// callers -- RemoveRaceCar (below) and UpdateInAndOutOfRangeCars @0x822FF8F8 (the rival
// in/out-of-range sweep, not reconstructed).
//
// SIGNATURE: DWARF BrnRaceCarEntityModule.h:725
//   void DetachActiveRaceCar(RaceCar*, OutputBuffer_PreScene::VehicleInputInterface*,
//                            OutputBuffer_PreScene::RaceCarAIInterface*,
//                            OutputBuffer_PreScene::SceneInputInterface*)
// which is exactly the asm's r4/r5/r6/r7 (0x822FEE04..0x822FEE18). Hex-Rays gets it right
// here for once, INCLUDING the parameter names -- they are the console's own assert strings.
//
// ASM WALK (0x822FEDF8..0x822FEFDC):
//   0x822FEE24  lpRaceCar != NULL                assert BrnRaceCarEntityModule.cpp:0x9A4 == :2468
//   0x822FEE48  lpVehicleInputInterface != NULL  assert :0x9A5 == :2469
//   0x822FEE6C  lpRaceCarAIInterface != NULL     assert :0x9A6 == :2470
//   0x822FEE90  lpSceneInputInterface != NULL    assert :0x9A7 == :2471
//   0x822FEEB4  lpActiveRaceCar = RaceCar::GetActiveRaceCar(lpRaceCar); != NULL  assert :2474
//   0x822FEEE4  lpActiveRaceCar->IsAttached()    assert BrnActiveRaceCar.h:0x441 == :1089
//   0x822FEF14  lwz 0x6F0 -> GetGlobalRaceCar() == lpRaceCar                     assert :2475
//   0x822FEF3C  lwz 0x748 -> leActiveRaceCarIndex; >= 0 assert :2478, < 8 assert :2479
//   0x822FEF88  addis r3,r27,1 ; addi r3,r3,0x1100  == this + 69888 == &mRaceCarStreamer
//               -> RaceCarStreamer::RemoveVehicleData(leActiveRaceCarIndex)
//   0x822FEF98  lbz 0x740 -> `if (!lpActiveRaceCar->IsInactive())`
//               -> RaceCarAIInterface::DeactivateRaceCar(lpRaceCar->GetGlobalRaceCarIndex(),
//                                                        lpRaceCar->IsInCurrentGameMode())
//               NOTE the asm EVALUATES IsInCurrentGameMode first (into r31) and
//               GetGlobalRaceCarIndex second, then passes r4 = index, r5 = flag.
//   0x822FEFD4  ActiveRaceCar::Detach(lpVehicleInputInterface, lpSceneInputInterface)
//
// Nothing else is torn down here. In particular the slot's meActiveRaceCarIndex survives
// (Construct owns it) and the GLOBAL car stays in the world -- RemoveRaceCar is what calls
// RaceCar::RemoveFromWorld afterwards.
// ============================================================================
void RaceCarEntityModule::DetachActiveRaceCar(
        RaceCar* lpRaceCar,
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface,
        BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface,
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInputInterface )
{
    CGS_ASSERT( lpRaceCar != 0, "lpRaceCar != NULL" );                               // X360 :2468
    CGS_ASSERT( lpVehicleInputInterface != 0, "lpVehicleInputInterface != NULL" );    // :2469
    CGS_ASSERT( lpRaceCarAIInterface != 0, "lpRaceCarAIInterface != NULL" );          // :2470
    CGS_ASSERT( lpSceneInputInterface != 0, "lpSceneInputInterface != NULL" );        // :2471

    // PC deviation, narrowed (verify F3): the console has no null tests here. A null car
    // cannot proceed; a null AI interface must NOT skip the streamer release and the state
    // transition below (that is the ghost fix itself) -- only the AI deactivate is gated on it.
    if( lpRaceCar == 0 )
    {
        return;
    }

    ActiveRaceCar* lpActiveRaceCar = lpRaceCar->GetActiveRaceCar();
    CGS_ASSERT( lpActiveRaceCar != 0, "lpActiveRaceCar != NULL" );                    // :2474
    if( lpActiveRaceCar == 0 )
    {
        return;
    }

    CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );                      // BrnActiveRaceCar.h:1089
    CGS_ASSERT( lpActiveRaceCar->GetGlobalRaceCar() == lpRaceCar,
                "lpActiveRaceCar->GetGlobalRaceCar() == lpRaceCar" );                 // :2475

    const EActiveRaceCarIndex leActiveRaceCarIndex = lpActiveRaceCar->GetActiveRaceCarIndex();
    CGS_ASSERT( leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" );                // :2478
    CGS_ASSERT( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );             // :2479

    mRaceCarStreamer.RemoveVehicleData( static_cast<s32>( leActiveRaceCarIndex ) );

    if( !lpActiveRaceCar->IsInactive() && lpRaceCarAIInterface != 0 )
    {
        const bool lbIsInAMode = lpRaceCar->IsInCurrentGameMode();
        lpRaceCarAIInterface->DeactivateRaceCar( lpRaceCar->GetGlobalRaceCarIndex(), lbIsInAMode );
    }

    lpActiveRaceCar->Detach( lpVehicleInputInterface, lpSceneInputInterface );
}

// ============================================================================
// RemoveRaceCar  @ 0x82304440   (ghost-car wave 2026-08-17)
//
// THIS IS THE FUNCTION THAT DELETES THE GHOST. Car Select re-spawns the player's car on every
// model change; without this call the PREVIOUS ActiveRaceCar slot stayed E_STATE_ACTIVE at the
// same world position, rendered every frame on top of the new car (its coarser-LOD wheel
// proxies over the real ones -- the "rectangle wheels") and was left behind in the junkyard
// when the player drove off. MEASURED before:
//   `[racecar-lod] banded 2 cars ... slot0 at (2986.75,-3.54,-2011.51) lod 1..4
//                                   slot1 at (2986.75,-3.54,-2011.51) lod 0 player 1`
//
// SIGNATURE: DWARF BrnRaceCarEntityModule.h:707
//   void RemoveRaceCar(EGlobalRaceCarIndex, OutputBuffer_PreScene*)
// == the asm's r4 (straight into GetGlobalRaceCar and into DetachAIControl) and r5 (the buffer
// whose three write-locked interface accessors it calls).
//
// ASM WALK (0x82304440..0x8230457C):
//   0x82304458  lpRaceCar = GetGlobalRaceCar(leGlobalRaceCarIndex)
//   0x82304464  lbz 0xA4 < 4        assert "muType < E_RACE_CAR_TYPE_COUNT"  BrnRaceCar.h:547
//   0x8230448C  lbz 0xA4 == 3       assert "lpRaceCar->IsInWorld()"          :2046
//                                   (muType == E_RACE_CAR_TYPE_INACTIVE is what FIRES it,
//                                    i.e. the predicate is IsInWorld())
//   0x823044C0  RaceCar::HasActiveRaceCar()  -- everything below is inside this branch
//   0x823044D8  addis r30,r28,2 ; addi r30,r30,-0x7D08  == this + 99064
//               == &mePlayerActiveRaceCarIndex, compared against
//               GetActiveRaceCar()->meActiveRaceCarIndex (lwz 0x748)
//   0x823044F4    if equal: assert lpRaceCar->IsPlayerDriven()               :2056
//   0x82304524              mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID
//   0x8230452C  the THREE write-locked accessors, in this evaluation order:
//                 r30 = OutputBuffer_PreScene::GetSceneInputInterface()      (IO.h:286)
//                 r29 = OutputBuffer_PreScene::GetRaceCarAIInterface()       (IO.h:301)
//                 r5  = OutputBuffer_PreScene::GetVehicleInputInterface()    (IO.h:283)
//               (IDA prints these as the truncated `...::Output`,
//                `...::OutputBuffer_PreScene::G` and `...::OutputBuffer_PreSce`; the addresses
//                0x822B4F78 / 0x822B52C0 / 0x822B4ED0 and the baked assert lines 286/301/283
//                identify them exactly, and they line up one-for-one with
//                DetachActiveRaceCar's own parameter names.)
//               -> DetachActiveRaceCar(lpRaceCar, vehicle, ai, scene)
//   0x82304564  RaceCarAIInterface::DetachAIControl(leGlobalRaceCarIndex)  -- OUTSIDE the
//               HasActiveRaceCar branch: a car with no active slot still had AI control
//               attached (SpawnRaceCar posts AttachAIControlEvent before any Attach).
//   0x82304574  RaceCar::RemoveFromWorld()  -- asserts mpActiveRaceCar == NULL, which is why
//               the Detach chain above has to run first.
//
// The evaluation order of the three accessors is preserved because each fires its own
// "Not locked for writing" tripwire; re-ordering them would change which assert a mis-locked
// buffer reports.
//
// THE OTHER SEVEN CONSOLE CALLERS (xrefs_to) and whether they are live on PC:
//   0x82304580 RemoveAllRaceCars              not reconstructed -- DEAD
//   0x82305688 HandleSetupNetworkCarAction    not reconstructed -- DEAD (game action 5)
//   0x823058F8 SetUpPlayerCarForMode          not reconstructed -- DEAD
//   0x82305E00 RemoveRivals                   not reconstructed -- DEAD
//   0x82305F28 RemoveAllRivalsFromWorld       not reconstructed -- DEAD
//   0x82306028 RemoveAllNetworkCarsFromWorld  not reconstructed -- DEAD
//   0x8230BE08 HandleGameActions              PRESENT but a partial slice: only cases 0 and 79
//                                             are reproduced, and case 0 reaches RemoveRaceCar
//                                             only through HandleResetPlayerCarAction below.
// So HandleResetPlayerCarAction is the ONE live caller on this build.
// ============================================================================
void RaceCarEntityModule::RemoveRaceCar(
        EGlobalRaceCarIndex leGlobalRaceCarIndex,
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput )
{
    RaceCar* lpRaceCar = GetGlobalRaceCar( leGlobalRaceCarIndex );
    CGS_ASSERT( lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                "muType < E_RACE_CAR_TYPE_COUNT" );                       // BrnRaceCar.h:547
    CGS_ASSERT( lpRaceCar->IsInWorld(), "lpRaceCar->IsInWorld()" );       // X360 :2046

    // PC deviation (verify F3): the console never null-tests the buffer (r27 is dereferenced
    // unconditionally and RemoveFromWorld is always reached @0x82304574). Assert first so a
    // null here is loud, then early out rather than dereference.
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );
    if( lpOutput == 0 )
    {
        return;
    }

    if( lpRaceCar->HasActiveRaceCar() )
    {
        if( mePlayerActiveRaceCarIndex == lpRaceCar->GetActiveRaceCar()->GetActiveRaceCarIndex() )
        {
            CGS_ASSERT( lpRaceCar->IsPlayerDriven(), "lpRaceCar->IsPlayerDriven()" );   // :2056
            mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }

        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInputInterface =
            lpOutput->GetSceneInputInterface();
        BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface =
            lpOutput->GetRaceCarAIInterface();
        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface =
            lpOutput->GetVehicleInputInterface();

        DetachActiveRaceCar( lpRaceCar, lpVehicleInputInterface,
                             lpRaceCarAIInterface, lpSceneInputInterface );
    }

    lpOutput->GetRaceCarAIInterface()->DetachAIControl( leGlobalRaceCarIndex );

    lpRaceCar->RemoveFromWorld();
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
            // ⛔ STALE BANNER CORRECTED 2026-08-17 (ghost-car wave). The old text said the
            // module bool at +99141 "has not been fitted"; it HAS been, since the player-input
            // wave (2026-08-11): BrnRaceCarEntityModule.h names it mbIsInOnlineGameMode (DWARF
            // :371, +0x18345 == 99141), pinned by ProcessPlayerVehicleInput @0x822FF318's
            // `lbzx r10, r31, 0x18345`. The console reads exactly that byte here
            // (`if (*(v6 + 99141)) v31 = RaceCar::IsInCurrentGameMode(lpOldCar) != 0;`), so the
            // gate is reproduced by name. It is FALSE on this build (nothing sets it -- there
            // is no online session), so the arm stays inert; what changes is that it is now
            // the console's condition instead of a hard-coded false.
            if( mbIsInOnlineGameMode )
            {
                lbWasInGameMode = lpOldCar->IsInCurrentGameMode();
            }
            // The console carries the OLD car's colour across ONLY when the model is unchanged.
            if( lpOldCar->GetModelId() == lpAction->mCarModelId )
            {
                liColourPalette = lpOldCar->GetColourPalette();
                liColourIndex   = lpOldCar->GetColourIndex();
                CGS_ASSERT( liColourPalette < 4, "Invalid Number of Palettes: " );   // :7535
                CGS_ASSERT( liColourIndex >= 0, "Invalid car colour: " );            // :7536
            }
            // ⭐ THE GHOST-CAR FIX (2026-08-17). The console removes the OLD player car here,
            // and this is the call the "[FLAG PC] RemoveRaceCar is not reconstructed" banner
            // used to stand in for. Asm at the call site:
            //     0x823052FC  mr r3, r28                 ; r28 == lpOldCar
            //     0x82305300  bl RaceCar::GetGlobalRaceCarIndex
            //     0x82305304  mr r4, r3                  ; the returned global index
            //     0x82305308  mr r3, r22                 ; this
            //     0x8230530C  mr r5, r25                 ; lpOutput (the pre-scene OUTPUT buffer)
            //     0x82305310  bl RaceCarEntityModule::RemoveRaceCar
            // The old banner's premise ("UNREACHED on the start-of-game path -- 
            // mePlayerActiveRaceCarIndex is still INVALID there") was true for the FIRST
            // record only. Car Select posts a fresh ResetPlayerCarAction on every car change,
            // and from the second one onwards the index IS valid, so this branch runs and the
            // previous slot has to be released here or it stays E_STATE_ACTIVE for ever.
            RemoveRaceCar( lpOldCar->GetGlobalRaceCarIndex(), lpOutput );
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
// REPRODUCED: the queue walk, cases 0 and 79 for the player-car spawn/paint pair, plus the
// boost actions 15, 34, 70, 71 and 198. Those latter cases are the retail state seam
// that enables earning once play starts, applies an explicit earning gate, and cancels a
// boost in progress; dropping them leaves a prepared strategy permanently unable to earn.
//
// [FLAG PC bring-up] every other case is DROPPED, not paraphrased. The named handlers the
// console dispatches to and that are still un-reconstructed:
//   3   RaceCar::RequestResetOnTrack        4   HandleSetPlayerOpponentsAction
//   5   HandleSetupNetworkCarAction         7   the player-control-changed AI publish
//   11  HandleRemotePlayerDisconnected      23  HandlePrepareForModeAction
//   39  HandleStopModeAction
//   73/74/76/77     the car-select / drive-thru arms
//   126 SwitchCarColourAction (an AI car's colour; asserts :7393/:7397/:7398)
//   219 the network setup-car arm, which also writes the colour pair (:7212/:7215)
//   97/98/99        the network add/remove arms       + ~80 more.
// Because the walk itself is real, adding any one of them later is a case label, not a
// re-derivation. DELETE-WHEN the handlers land.
// ============================================================================
void RaceCarEntityModule::HandleCarStatsUpdate(BrnResource::ECarType leCarType,
                                                s32 liBoostLevel,
                                                s32 liBoostLossLevel)
{
    // ARTIST @0x822A4720..0x822A4770: unsigned 0/1/2 selection, then the
    // manager prefix at this+0x17890.
    switch (leCarType)
    {
    case BrnResource::E_CARTYPE_DANGER:
        mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT2);
        break;
    case BrnResource::E_CARTYPE_AGGRESSION:
        mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT3);
        break;
    case BrnResource::E_CARTYPE_STUNTS:
        mBoostManager.SetBoostStrategy(BoostManager::E_BOOSTSTRATEGY_BURNOUT5);
        break;
    default:
        CGS_ASSERT(false, "Unknown car type");
        break;
    }

    // ARTIST @0x822A4774..0x822A4798 stores +0x454/+0x458 and dispatches
    // virtual slot 43 on the selected strategy.
    mBoostManager.ApplyCarStats(liBoostLevel, liBoostLossLevel);
}

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

        // ARTIST 0x8230C418..0x8230C450: payload assertion followed by the
        // selected strategy's vtable slot 48.
        case BrnGameState::GameStateModuleIO::E_ACTION_COMPLETED_STUNT: // 15
        {
            const BrnGameState::GameStateModuleIO::CompletedStuntAction* lpCompletedStunt =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::CompletedStuntAction*>(lpEvent);
            CGS_ASSERT(lpCompletedStunt != 0, "lpCompletedStuntAction != NULL");
            mBoostManager.UpdateStuntBoost(lpCompletedStunt);
            break;
        }

        // ARTIST 0x8230C75C..0x8230C76C forwards this/action/output with no
        // reshaping. The handler's non-Showtime boost spine is reconstructed
        // in BrnRaceCarEntityModule_ModeArming.cpp.
        case BrnGameState::GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE: // 23
            HandlePrepareForModeAction(
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::PrepareForModeAction*>(lpEvent),
                lpOutput);
            break;

        // ARTIST 0x8230C7A0..0x8230C7E0.  The trailing non-boost stores and
        // optional donut-start placement live at 0x8230C7E4..0x8230C880 and
        // remain with the wider mode-action reconstruction.
        case BrnGameState::GameStateModuleIO::E_ACTION_START_PLAYING_MODE: // 34
            CGS_ASSERT(mbIsInGameMode, "mbIsInGameMode");
            SetAllCarsOnStartLine(ActiveRaceCar::E_RACE_START_STATE_RACING, true);
            mBoostManager.SetBoostEarningEnabled(true);
            break;

        // ARTIST 0x8230C3D8..0x8230C408.  DecFIGS gives the exact one-bool
        // AllowBoostEarningAction declaration used here.
        case BrnGameState::GameStateModuleIO::E_ACTION_ALLOW_BOOST_EARNING: // 70
        {
            const BrnGameState::GameStateModuleIO::AllowBoostEarningAction* lpAllow =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::AllowBoostEarningAction*>(lpEvent);
            CGS_ASSERT(lpAllow != 0, "lpAllowBoostEarningAction != NULL");
            mBoostManager.SetBoostEarningEnabled(lpAllow->mbAllowBoostEarning);
            break;
        }

        // ARTIST 0x8230C40C..0x8230C414 is the selected strategy's +0xC5
        // mbBoosting byte store.  TurnOffBoosting is that exact named base body.
        case BrnGameState::GameStateModuleIO::E_ACTION_STOP_BOOSTING: // 71
            mBoostManager.TurnOffBoosting();
            break;

        // X360 `case 79`: ChangePlayerCarColour(payload[0], payload[4]). The payload is
        // CarSelectChangeColourAction -- PALETTE first (see the record's banner in
        // BrnGameActions.h). This is the action that carries the car's AUTHORED default
        // colour out of the VehicleList and into the world, so dropping it painted every
        // car in palette 0 / colour 0.
        case BrnGameState::GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR:   // 79
        {
            const BrnGameState::GameStateModuleIO::CarSelectChangeColourAction* lpColour =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::CarSelectChangeColourAction*>(
                        lpEvent );
            ChangePlayerCarColour( lpColour->muPaletteIndex, lpColour->muColourIndex );
            break;
        }

        // ARTIST case 198 reads +0x14/+0x0C/+0x08 in that order and calls
        // HandleCarStatsUpdate @0x822A4700.
        case BrnGameState::GameStateModuleIO::E_ACTION_UPDATE_CAR_STATS: // 198
        {
            const BrnGameState::GameStateModuleIO::SendCarStatsAction* lpStats =
                reinterpret_cast<
                    const BrnGameState::GameStateModuleIO::SendCarStatsAction*>(lpEvent);
            CGS_ASSERT(lpStats != 0, "lpSendCarStatsAction != NULL");
            HandleCarStatsUpdate(
                lpStats->meCarType, lpStats->miCarBoost, lpStats->miCarControl);
            break;
        }

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
        // The latch is on the {palette, colour} PAIR, not a plain "logged once" bool: the pair
        // starts at 0/0 on every fresh spawn and only becomes the car's authored default when
        // ChangePlayerCarColour runs, several frames later. A one-shot latch would print the
        // fallback and never the real value.
        {
            static s32 siLastLoggedPalette = -2;
            static s32 siLastLoggedColour  = -2;
            if( ( liPaletteIndex != siLastLoggedPalette || liColourIndex != siLastLoggedColour )
                && CgsDev::Log::gpDebugPrint != 0 )
            {
                siLastLoggedPalette = liPaletteIndex;
                siLastLoggedColour  = liColourIndex;
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
// X360 0x822D27B0 -- ChangePlayerCarColour. COMPLETE (colour wave 2026-08-03).
//
// ⭐ THIS IS WHERE A CAR'S AUTHORED COLOUR ENTERS THE WORLD. UpdateActiveRaceCarColours
// above resolves RaceCar::{miColourPalette, miColourIndex} against mCarColoursResource with
// the console's own -1 -> 0 fallback -- but nothing on this build ever wrote those two
// members with anything except 0, so every car took the fallback and rendered palette 0 /
// colour 0 == (0.784314, 0, 0, 1), RED. The VehicleList authors PUSMC01 (the Hunter
// Cavalry) as colour 13 / palette 0, and 367 of its 431 entries author a pair that is not
// (0,0), so the whole list was being ignored.
//
// ⚠️ HandleResetPlayerCarAction is NOT the missing writer, and its committed body is
// faithful: the console really does spawn a fresh car at 0/0 there (it only carries an OLD
// car's pair across when the model id is unchanged). The authored default arrives
// afterwards, as game action 79, which this build was dropping in HandleGameActions'
// `default:` arm. Three console writers of RaceCar+148/+152 exist and all three are in this
// module -- this one, `case 219` (the network setup-car arm, asserts :7212/:7215) and
// `case 126` (SwitchCarColourAction, colour only, asserts :7393/:7397/:7398). None is in
// ActiveRaceCar::Attach / SpawnRaceCar / AddHandlingModel.
//
// The console body, in asm order (0x822D27B0..0x822D2984):
//   assert luPaletteIndex < 4                          "Invalid Palette Index"        :8465
//   lpActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex)   // this+0x182F8
//   assert lpActiveRaceCar->IsAttached()               BrnActiveRaceCar.h:1089
//   lpPlayerCar = lpActiveRaceCar->mpRaceCar           // +0x6F0, GetGlobalRaceCar inlined
//   assert lpPlayerCar != 0                            "lpPlayerCar"                  :8468
//   assert luPaletteIndex < 4          (StrStream)     "Invalid Number of Palettes: " :8471
//   assert luColourIndex < maPalettes[luPaletteIndex].miNumColours
//                                      (StrStream)     "Invalid car colour: "         :8472
//   lpPlayerCar->miColourPalette = luPaletteIndex;     // +152
//   lpPlayerCar->miColourIndex   = luColourIndex;      // +148
//
// ⚠️ SIGNATURE FROM THE ASM, not from Hex-Rays' argument count: `r4` is compared with
// `cmplwi ..,4` (unsigned, the palette) and `r5` with `cmpw` against miNumColours (signed).
// The DWARF declares both parameters uint32_t; the signed compare is reproduced with the
// cast the console's own `cmpw` implies.
//
// ⚠️ The palette is asserted TWICE with two different messages -- the plain :8465 on entry
// and the StrStream :8471 further down. That is not a transcription slip; both are in the
// asm, and both are kept.
// ============================================================================
void RaceCarEntityModule::ChangePlayerCarColour( u32 luPaletteIndex, u32 luColourIndex )
{
    CGS_ASSERT( luPaletteIndex < static_cast<u32>( E_NUM_PALETTES ),
                "Invalid Palette Index" );                                   // X360 :8465

    ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );

    CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );  // BrnActiveRaceCar.h:1089

    // The console loads mpRaceCar straight out of +0x6F0 here; GetGlobalRaceCar() is that
    // load plus the IsAttached() assert the line above already reproduces.
    RaceCar* lpPlayerCar = lpActiveRaceCar->GetGlobalRaceCar();

    CGS_ASSERT( lpPlayerCar != 0, "lpPlayerCar" );                           // X360 :8468

    if( lpPlayerCar == 0 )
    {
        // The X360 asserts and then stores through the null pointer anyway. Bail instead of
        // faulting -- the same treatment ProgressionManager::GetCarColourAndPalette's
        // "lpVehicleListEntry" assert already gets.
        return;
    }

    // The X360 streams the offending index into the assert buffer through StrStream
    // ("Invalid Number of Palettes: " << idx). CGS_ASSERT forwards a plain string here, the
    // same shortening HandleResetPlayerCarAction's :7535/:7556 pair already uses.
    CGS_ASSERT( luPaletteIndex < static_cast<u32>( E_NUM_PALETTES ),
                "Invalid Number of Palettes: " );                            // X360 :8471

    CGS_ASSERT( static_cast<s32>( luColourIndex ) <
                    mCarColoursResource->maPalettes[luPaletteIndex].GetNumColours(),
                "Invalid car colour: " );                                    // X360 :8472

    lpPlayerCar->SetColourPalette( static_cast<s32>( luPaletteIndex ) );      // RaceCar +152
    lpPlayerCar->SetColourIndex( static_cast<s32>( luColourIndex ) );         // RaceCar +148

    // [PC diagnostic] the paired print at the far end of this transfer is the
    // "[racecar-paint]" line in UpdateActiveRaceCarColours -- print BOTH ends.
    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[racecar-paint] ChangePlayerCarColour: player slot "
            << static_cast<s32>( mePlayerActiveRaceCarIndex )
            << " -> palette " << static_cast<s32>( luPaletteIndex )
            << " colour " << static_cast<s32>( luColourIndex ) << "\n";
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

        // [motion] opt-in harness probe (BRN_MOTION_PROBE=1, flow_run.ps1 -MotionProbe): the player
        // slot's pose/heading/velocity every 30 presents. Off by default; not console code.
        {
            static s32 siMotionProbe = -1;
            if( siMotionProbe < 0 )
            {
                const char* lpcEnv = getenv( "BRN_MOTION_PROBE" );
                siMotionProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
            }
            if( siMotionProbe == 1 && leSlot == mePlayerActiveRaceCarIndex )
            {
                static u32 suMotionCount = 0;
                ++suMotionCount;
                if( ( suMotionCount % 30u ) == 0u && CgsDev::Log::gpDebugPrint != 0 )
                {
                    const BrnPhysics::Vehicle::RaceCarState* lpS =
                        lpActiveRaceCar->GetPhysicsState();
                    const Vector3& lP = lpS->mTransform.Pos();
                    const Vector3& lA = lpS->mTransform.At();
                    const Vector3& lV = lpS->mLinearVelocity;
                    const f32 lfSpeed = sqrtf( lV.x * lV.x + lV.y * lV.y + lV.z * lV.z );
                    *CgsDev::Log::gpDebugPrint
                        << "[motion] n " << static_cast<s32>( suMotionCount )
                        << " pos " << lP.x << " " << lP.y << " " << lP.z
                        << " at "  << lA.x << " " << lA.y << " " << lA.z
                        << " vel " << lV.x << " " << lV.y << " " << lV.z
                        << " |v| " << lfSpeed
                        << " mph " << lpS->mfSpeedMPH
                        << " gear " << static_cast<s32>( lpS->mi8Gear )
                        << " rpm " << lpS->mfRPM
                        << " gas " << lpS->mfGas
                        << " steer " << lpS->mfSteering
                        << " engine " << static_cast<s32>( lpActiveRaceCar->GetEngineState() )
                        << "\n";
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

    // ---- step 12: LATCH THIS FRAME'S PAD STATE ------------------------------
    // ⚠️ Load-bearing: ProcessPlayerVehicleInput reads mPlayerVehicleControls by name at
    // fifteen sites, so dropping this latch serves a zero-initialised control set for ever and
    // throws away every control the player (or the harness) pressed one hop before it is used.
    //
    // The console's three consecutive stores, verbatim from PreSceneUpdate @0x8230D928:
    //     *(a1 + 99300) = InputBuffer_PreScene::GetActivePaybackType(v5);
    //     *(a1 + 99304) = InputBuffer_PreScene::GetActivePaybackAggressor(v5);
    //     v56 = sub_822B4B88(v5);            // == GetPlayerVehicleControls: its "Not locked
    //                                        //    for reading" assert cites
    //                                        //    BrnRaceCarEntityModuleIO.h:160, which is the
    //                                        //    line this tree annotates that getter with
    //     memcpy(a1 + 99240, v56, 60);       // -> mPlayerVehicleControls
    // reached by NAME here; +99240/+99300/+99304 are quoted only as the console's own proof of
    // which member each store lands in (see the member banners in the header).
    meActivePaybackType       = lpInput->GetActivePaybackType();
    meActivePaybackAggressor  = lpInput->GetActivePaybackAggressor();
    {
        const BrnWorld::PlayerVehicleControls* lpPlayerControls =
            lpInput->GetPlayerVehicleControls();
        if( lpPlayerControls != 0 )
        {
            // [FLAG PC bring-up] the null test is the deviation: the console's getter cannot
            // return null (it is `this + 496`, an embedded sub-object), but this build's
            // producer chain is still being assembled and a null here would be a fault, not a
            // dropped frame. The copy itself is the console's 60-byte memcpy.
            mPlayerVehicleControls = *lpPlayerControls;
        }
    }

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

// ============================================================================
// ⭐⭐ ReadUpdatedActiveRaceCarDataFromPhysics @ 0x822E87B8 -- THE PHYSICS RETURN PATH
// (physics-return-path wave 2026-08-11).
//
// This is the console's ONE producer of every active car's pose. Everything the render
// side, the output interfaces and the director read about where a car IS comes out of
// here. Its only caller is PostPhysicsUpdate @0x82307538 (`bl` at 0x8230761C, BEFORE
// UpdateActiveRaceCarColours / UpdateOutputInterfaces -- the order this file's
// PostPhysicsUpdate banner already records from the asm).
//
// SIGNATURE from the asm prologue (0x822E87D8 `mr r26, r3` / 0x822E87DC `mr r16, r4`):
// two arguments, this + the post-physics input buffer. Hex-Rays renders twelve; the extra
// ten are stack-frame artifacts of the assert-message builder.
//
// THE CONSOLE BODY, leg by leg (addresses from the ARTIST listing):
//   L1  0x822E87E8..0x822E8880  the EIGHT-SLOT CAR LOOP.
//       per slot: if IsActive()
//                    UpdatePhysicsState(GetVehicleOutputInterface()->GetRaceCar(i),
//                                       &mWorldMap2D)
//                    UpdateRaceCarCollisionTagging(i, GetRaceCar(i))
//                    UpdateDeformationState(GetDeformationOutputInterface()
//                                               ->mpDeformationState)
//                 then UNCONDITIONALLY `stw r14, 0x1598(r30)` -- see L1b.
//   L1b 0x822E8874                ActiveRaceCar+0x1598 == mRenderParams(+0x7E0) + 0xDB8
//                                 == maDetachedParts.miLength, i.e. the per-frame
//                                 GetDetachedPartQueue().Clear() that L6 then refills.
//   L2  0x822E888C..0x822E8D68  the GLASS SMASH/CRACK drain over
//                               GetDeformationOutputInterface()->mGlassSmashOrCrackQueue.
//   L3  0x822E8D6C..~0x822E8E30 the WHEEL-STATE publish: for each live entry of
//                               GetDeformationOutputInterfaceForEntityModules()
//                               (muNumEntries / maBaseIDs / maWheelStates) call
//                               ActiveRaceCar::UpdateWheelPhysicsState.
//   L4  the SKINNED-MODEL verlet copy (maSkinData -> ActiveRaceCar+0x2280, 128 x 16B).
//   L5  the LOCATOR-OUTPUT copy (maLocatorData -> the car's light-locator block).
//   L6  the DETACHED-PART render events -> maDetachedParts.AddEventSafe.
//
// ⭐ LANDED HERE: L1 (the transform/velocity/gear/wheel return path -- the mission) and
// L1b -- i.e. the whole vehicle-physics half of the readback is BODIED here.
//
// ⚠️ "BODIED" IS NOT "RUNNING", AND THIS PARAGRAPH USED TO BLUR THAT (softened 2026-08-11).
// L1's per-car call sits behind the mUsedRaceCars gate described further down. That gate is
// the console's OWN liveness test -- VehicleOutputInterface::GetUsedCarsBitArray, the bitset
// the physics side sets when it takes a race-car slot -- not a bring-up invention, but on
// every build up to this wave the bit was never set by anything, so L1 was gated OFF and the
// landed code did no work. The only writer of that bitset in the XEX is
// VehicleManager::ProcessCreateEvents, and THAT LANDED AND MOUNTED THIS WAVE
// (BrnVehicleManager_ProcessCreateEvents.cpp + its build_game_exe.bat line), so the path goes
// live with the create drain rather than with this file. Read the claim below as what the
// leg DOES once the bit is up.
//
// UpdatePhysicsState
// @0x822D4418 memcpy's the published 1120-byte RaceCarState into mPhysicsState, drives
// RaceCar::UpdatePositioningData + UpdateVelocity, publishes mRenderParams.mBodyTransform
// from the console's own CalcBodyTransform, copies the four wheel transforms + existence
// flags, and runs the brake/reverse/engine-off tail. It is what retires BOTH bring-up
// stand-ins (SeedPhysicsStateFromCreateEventBringUp and
// PublishRenderPoseWithoutPhysicsBringUp) -- retirement is the conductor's consolidation
// once the rest of this wave lands, NOT this file's job today.
//
// ⛔ PARKED, LOUDLY, WITH A LOG-ONCE GATE EACH (never a silent no-op):
//   * UpdateRaceCarCollisionTagging @0x822D2280 (159 insns) -- absent from the tree; over
//     the wave's land-it size bar and it reaches the un-homed collision-group interior.
//   * UpdateDeformationState @0x822D4A58 (107 insns) -- absent; small enough by size, but
//     its body stores through EIGHT ActiveRaceCar offsets (+0x157C, +0x6E0, +0x1320,
//     +0x1326) that have NO named member in BrnActiveRaceCar.h, and it needs
//     DeformationState::GetCarStateF. Landing it would mean minting eight members from
//     offsets alone -- the exact live-corruption bug class this project keeps paying for.
//   * L2 / L3 / L4 / L5 / L6 -- the whole DEFORMATION-OUTPUT half. All five read
//     DeformationOutputInterface / DeformationOutputInterfaceForEntityModules, whose
//     producer (the deformation manager's per-frame publish) is not on this build, and
//     four of them need private members of a header outside this tree's ownership.
//     UpdatePhysicsState already publishes the four wheel transforms from the RaceCarState,
//     so L3's absence does NOT hold the wheels back.
//
// ⚠️ THE BRING-UP GATE, AND WHY IT IS THE CONSOLE'S OWN SIGNAL, NOT AN INVENTION.
// The console loops on IsActive() alone, because its own flow guarantees that an ACTIVE
// entity-module slot has a matching live physics slot (VehicleManager::ProcessCreateEvents
// makes both, together). On THIS build that guarantee does not hold: nothing populates
// VehicleOutputInterface::maRaceCarStates yet -- which is precisely the fact
// BrnPhysicsModuleUpdateFunctions.cpp's PC-BUILD GUARD #2 measured (663 assert dialogs
// from an identically-ZERO mEntityId sailing through the console's own sentinel test).
// Running the readback against that zero would memcpy an all-zero RaceCarState over a car
// that HAS a good placement pose and teleport it to the world origin -- strictly worse
// than today. So the per-slot call is gated on mUsedRaceCars, the vehicle output's OWN
// "physics owns this race-car slot" bitset (VehicleOutputInterface::GetUsedCarsBitArray,
// DWARF :382, zeroed by its Construct and set by the physics side when it takes a slot).
// It invents nothing: it is the console's own liveness bit, tested where the console's own
// flow would have made it redundant, and the moment the sibling producer sets it the real
// path runs with no further edit here.
// ⭐ AS OF 2026-08-11 THAT PRODUCER IS MOUNTED: VehicleManager::ProcessCreateEvents
// @0x82616770 -- the XEX's only setter of mUsedRaceCars -- is bodied and in
// build_game_exe.bat, so the gate now passes for every slot the create drain has taken and
// L1 does real work on those cars. The gate itself STAYS (it is the console's own bit, and
// it is what keeps a still-unclaimed slot from memcpy'ing a zero RaceCarState over a good
// placement pose); what is retired is the assumption that it never passes.
// DELETE-WHEN the maRaceCarStates publish is proven for every active slot on a booted run.
//
// ⚠️ DIVERGENCE, stated plainly: the console has NO `lpInput != NULL` early-out and no
// bring-up gate. Both are additions, and both are marked.
// ============================================================================
void RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics(
        RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput )
{
    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );
    if( lpInput == 0 )
    {
        return;
    }

    // The console's two buffer reads, in its own order: the deformation output first
    // (0x822E87E8, only to lift mpDeformationState out of it at +0x70), then the vehicle
    // output (0x822E87F8).
    const BrnPhysics::Deformation::DeformationOutputInterface* lpDeformationOutput =
        lpInput->GetDeformationOutputInterface();
    const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput =
        lpInput->GetVehicleOutputInterface();

    // `lwz r27, 0x70(r11)` -- the DeformationState the parked UpdateDeformationState leg
    // takes as its second argument. Read here, at the console's own point, so the parked
    // leg's input is visible in the reconstruction rather than implied.
    const BrnPhysics::Deformation::DeformationState* lpDeformationState =
        lpDeformationOutput->mpDeformationState;

    const CgsContainers::BitArray<8u>& lrUsedRaceCars = lpVehicleOutput->GetUsedCarsBitArray();

    bool lbSawActiveCarWithoutPhysicsSlot = false;

    // ---- L1 : the eight-slot car loop ---------------------------------------
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );

        if( lpActiveRaceCar->IsActive() )
        {
            // [FLAG PC bring-up] the mUsedRaceCars gate -- see the banner.
            if( lrUsedRaceCars.IsBitSet( static_cast<u32>( liCar ) ) )
            {
                // ⭐ THE RETURN PATH. `bl ActiveRaceCar::UpdatePhysicsState` @0x822E8844,
                // r4 = GetRaceCar(i), r5 = this + 0x18300 == &mWorldMap2D.
                lpActiveRaceCar->UpdatePhysicsState(
                    lpVehicleOutput->GetRaceCar( static_cast<u32>( liCar ) ),
                    &mWorldMap2D );

                // ⛔ PARKED: UpdateRaceCarCollisionTagging(liCar, GetRaceCar(liCar))
                //            @0x822D2280 and UpdateDeformationState(lpDeformationState)
                //            @0x822D4A58 run here on the console. Both are absent from the
                //            tree; see the banner for why each is parked rather than
                //            guessed. Reported once per boot, never per frame.
                static bool sbReportedParkedPerCarLegs = false;
                if( !sbReportedParkedPerCarLegs )
                {
                    sbReportedParkedPerCarLegs = true;
                    if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
                        && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[physics-readback] PARKED per-car legs: "
                               "RaceCarEntityModule::UpdateRaceCarCollisionTagging "
                               "(X360 0x822D2280) and ActiveRaceCar::UpdateDeformationState "
                               "(X360 0x822D4A58) are NOT reconstructed -- collision tagging "
                               "and deformation state will not update. DeformationState ptr "
                            << ( lpDeformationState != 0 ? "present" : "NULL" ) << "\n";
                    }
                }
            }
            else
            {
                lbSawActiveCarWithoutPhysicsSlot = true;
            }
        }

        // L1b -- `stw r14, 0x1598(r30)`: UNCONDITIONAL for all eight slots (it sits after
        // the IsActive branch rejoins), and it is the queue L6 refills.
        lpActiveRaceCar->GetRenderParams()->GetDetachedPartQueue().Clear();
    }

    if( lbSawActiveCarWithoutPhysicsSlot )
    {
        // [FLAG PC bring-up] the honest consequence of the deferral -- see the banner.
        static bool sbReportedNoPhysicsSlot = false;
        if( !sbReportedNoPhysicsSlot )
        {
            sbReportedNoPhysicsSlot = true;
            if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
                && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[physics-readback] an ACTIVE race car has NO physics slot "
                       "(VehicleOutputInterface::mUsedRaceCars bit clear) -- the readback is "
                       "held back for it so the placement pose is not overwritten with zeros. "
                       "DELETE WHEN VehicleManager::ProcessCreateEvents populates "
                       "maRaceCarStates.\n";
            }
        }
    }

    // ---- L5 (the LOCATOR-OUTPUT copy), stood in for -------------------------
    // [FLAG PC bring-up] at leg L5's own position in the console's body. The rest-pose
    // locators do NOT depend on physics ownership -- they are authored data in the car's
    // handling-body frame -- so this runs for every ACTIVE slot, not only the ones the
    // mUsedRaceCars gate above lets through. See PublishRestPoseLightLocatorsBringUp's
    // banner for the three dead producer hops it stands in for.
    // DELETE-WHEN L5 below unparks.
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
        if( lpActiveRaceCar->IsActive() )
        {
            PublishRestPoseLightLocatorsBringUp( lpActiveRaceCar, liCar );
        }
    }

    // ---- L2 / L3 / L4 / L5 / L6 : the deformation-output half ----------------
    // ⛔ PARKED as a block -- see the banner. Loud once, never a silent no-op.
    static bool sbReportedParkedDeformationLegs = false;
    if( !sbReportedParkedDeformationLegs )
    {
        sbReportedParkedDeformationLegs = true;
        if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
            && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[physics-readback] PARKED deformation legs of "
                   "ReadUpdatedActiveRaceCarDataFromPhysics (X360 0x822E87B8): glass "
                   "smash/crack drain, wheel-state publish (UpdateWheelPhysicsState), "
                   "skinned-model verlet copy, locator-output copy and detached-part render "
                   "events. Their producer (the deformation manager publish) is absent on "
                   "this build; the wheel POSE is still published by UpdatePhysicsState.\n";
        }
    }
}

// ============================================================================
// ⭐⭐ THE THREE SCENE LEGS OF PostPhysicsUpdate  (wave Q5, cluster G1, 2026-08-18)
//
// These are the CAR's half of the scene volume-collision middle: the leg that puts a race
// car's collision box INTO the scene (ProcessCreateVehicleEvents -> OnHandlingModelAdded ->
// AddToScene), the leg that MOVES it every frame (GenerateSceneUpdateEvents ->
// InSceneUpdateInterface::SetVolumeInstanceTransform), and the per-frame culling/collision
// refresh (SendRaceCarSceneUpdates -> ActiveRaceCar::SendSceneUpdatesPostPhysics).
//
// All three are called ONLY from PostPhysicsUpdate @0x82307538, and the call order + the
// paused-branch membership below were re-derived from that function's asm this wave, not
// taken from any earlier note:
//     0x823075BC  lpInput->LockForRead()          <- the whole body runs inside this lock
//     0x823075C4  lpOutput->LockForWrite()
//     0x823075D8  CopyActiveRaceCarToPlayerScoringMappingToOutput
//     0x823075E8  ProcessRaceCarCrashEvents_PostPhysics
//     0x823075F8 ⭐ ProcessCreateVehicleEvents(lpInput, lpOutput)
//     0x82307604  ReadOutOfRangeRaceCarDataFromAI
//     0x82307610  bne -> 0x823076C0                <- `if ((lUpdateSet & 1) == 0)`, the SIM-PAUSED skip
//       0x8230761C  ReadUpdatedActiveRaceCarDataFromPhysics
//       0x8230763C  the 5-byte AggressiveDrivingFlags copy (input +0x6C00 -> module +0x1836C)
//       0x82307658 ⭐ GenerateSceneUpdateEvents(lpOutput)
//       0x82307664  UpdateRaceCarContacts / ProcessPropContactQueue / UpdateCrashingPlayerContacts
//       0x82307688  UpdateActiveRaceCarTransforms / UpdateCurrentWorldRegion / UpdateHidingEvents
//       0x823076BC  StorePlayerRoutePortalPositions
//     0x823076C4  UpdateActiveRaceCarColours
//     0x823076D8  UpdateOutputBoostInfo
//     0x8230771C  UpdateOutputInterfaces
//     0x82307730  TransmitCarsInRaceToQueryManager
//     0x8230773C ⭐ SendRaceCarSceneUpdates(lpOutput)          <- OUTSIDE the paused skip
//     0x82307744  bne -> 0x82307930                <- the SECOND paused skip (near-miss/air-time)
//     0x82307938  SendGameEvents / SendStreamerEvents / ReplayIO::RegisterSerialiser
//     0x82307980  UnlockForRead / UnlockForWrite
// ============================================================================

// ============================================================================
// ProcessCreateVehicleEvents  @0x822FF620 (182)
//
// ⭐ THE STALE-BANNER RE-VERIFICATION THE WAVE ASKED FOR, AND ITS ANSWER.
// The now-retired PublishNewVehicleToDirectorWithoutPhysicsBringUp stand-in (its seat, and the
// record of its deletion, is above) claimed this function's input queue "is permanently empty
// on this build, and it is not a mount away from being filled". That was true when it was
// written and is NOT true now. Re-checked 2026-08-18 against the tree and the current boot log:
//   * BrnVehicleManager_ProcessCreateEvents.cpp exists, is REAL, and IS on
//     tools/build/build_game_exe.bat;
//   * its per-car body ends in `lpManagerOutputInterface->AddCreateVehicleResult(lResult)`
//     with mbSuccess = true;
//   * build/game/BrnGame.log:883 carries that body's own one-shot line
//     ("[FLAG PC bring-up] ProcessCreateEvents: race-car debug component vptr is NULL"),
//     which is emitted INSIDE the per-race-car loop three statements before that Add --
//     i.e. the queue is written on this build, at least once, on the junkyard drive;
//   * BrnGame.log:819 records BridgePhysicsModuleToRaceCarModule_PostPhysics legs 1-2
//     (vehicle output + VEHICLE-MANAGER output) as LIVE, so the written interface really
//     does reach this module's post-physics input buffer.
// The ordering worry the stand-in's banner raised (publishing before the car's attribute
// collection is resident) also resolves by itself: the create event is posted by
// ActiveRaceCar::AddHandlingModel from ResetActiveRaceCar, which only runs after
// OnRaceCarResourcesLoaded, so the result lands AFTER the vault load. The log shows exactly
// that ordering -- vault 109b0d7b00000000 at :853, E_STATE_ACTIVE at :881, ProcessCreateEvents
// at :883.
//
// THE BODY, leg for leg from the asm -- LANDED IN FULL 2026-08-18 (wave Q5 finisher); this
// list is now a map of the code below, not a transcription waiting to be pasted:
//   assert lpInput/lpOutput (X360 :5174 / :5175)
//   lpInput->GetVehicleOutputInterface()            <- result DISCARDED (see the note below)
//   queue = lpInput->GetVehicleManagerOutputInterface()->GetCreateVehicleResults()
//   for each event:
//       if (id.GetEntityIDOwner() != E_ENTITYTYPE_RACECAR) continue;   // `srwi r11,r29,24; cmplwi 1`
//       assert(mbSuccess)                                              // "Add vehicle failed\n", :5202
//       slot = id.GetEntityIDEntityIndex()                             // `extrwi r31,r29,14,8`
//       assert slot >= 0 / slot < COUNT                                // :5206 / :5207
//       car = GetActiveRaceCar(slot)
//       assert(car->IsAttached())                                      // BrnActiveRaceCar.h:1089
//                                                                      // asm 0x822FF7F0..0x822FF814
//       modelIndex = mpVehicleList->GetVehicleIndex(car->GetGlobalRaceCar()->GetModelId())
//       assert modelIndex >= 0                                         // :5211
//       car->OnHandlingModelAdded(lpOutput->GetSceneInputInterface(),
//                                 lpOutput->GetVehicleInputInterface(), mfTimeStep)
//       if (car->IsPlayer())
//           lpOutput->GetDirectorVehicleInputInterface()->NewVehicle(
//               mpVehicleList->GetVehicleData(modelIndex)->GetAttribCollectionKeyHash(),
//               modelIndex)
//
// ⚠️ THE DISCARDED FIRST ACCESSOR IS THE CONSOLE'S, NOT A SLIP. The asm calls
// InputBuffer_PostPhysics::GetVehicleOutputInterface (@0x822FF694, the +16 accessor) and
// throws r3 away one instruction later, then calls GetVehicleManagerOutputInterface
// (@0x822FF69C, +27680) for the queue. That is a second source-level local the optimiser
// dropped; the call itself could not be elided because the accessor is out of line and
// carries its own "Not locked for reading" tripwire. Reproduced as-is.
//
// ⚠️ THE THREE INTERFACE HOPS ARE NAMED, NOT OFFSET-DERIVED, AND TWO OF THE NAMES IN
// BrnRaceCarEntityModuleIO.h's DECLARATION COMMENTS ARE WRONG (reported to the conductor,
// not fixed here -- that file is not this cluster's). Measured from the accessor bodies:
//     sub_822B63E0  -> this + 8224    (h:576)  == mSceneInputInterface        -> GetSceneInputInterface()
//     0x822B6488    -> this + 826992  (h:579)  == mDirectorVehicleInputInterface
//     0x822B6920    -> this + 855200  (h:600)  == mVehicleInputInterface      -> GetVehicleInputInterface()
// which agrees with OutputBuffer_PostPhysics::Construct's own offset comments in that header
// (+8224 scene, +826992 director, +855200 vehicle) and disagrees with the per-declaration
// "(0x822B6488)" / "(0x822B6920)" attributions three lines below them. The names used here
// are the ones Construct attests.
// ============================================================================
void RaceCarEntityModule::ProcessCreateVehicleEvents(
        const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );      // X360 :5174
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );    // X360 :5175

    if( lpInput == 0 || lpOutput == 0 )
    {
        return;
    }

    // The console's two accessor hops, in its own order -- the first result is discarded.
    lpInput->GetVehicleOutputInterface();
    const BrnPhysics::Vehicle::VehicleManagerOutputInterface* lpVehicleManagerOutput =
        lpInput->GetVehicleManagerOutputInterface();

    if( lpVehicleManagerOutput == 0 )
    {
        return;
    }

    // ⭐ UNPARKED 2026-08-18 (wave Q5 finisher). The queue is reached through the DWARF's own
    // accessor, which now exists (BrnVehicleOutputInterface.h:148 -- dumpfile :232 --
    // `const VehicleManagerOutputInterface::CreateVehicleResultQueue* GetCreateVehicleResults()
    // const`). The X360 inlines it: `addi r30, r3, 0x6C0` @0x822FF6A0 is exactly
    // mCreateVehicleResultQueue's seat, and the walk below is that queue's own
    // miLength/GetEvent pair (`lwz r11, 8(r30)` @0x822FF6A8 and @0x822FF8DC -- the console
    // RE-READS the length every iteration, so the loop condition is re-evaluated here too).
    const BrnPhysics::Vehicle::VehicleManagerOutputInterface::CreateVehicleResultQueue*
        lpCreateVehicleResults = lpVehicleManagerOutput->GetCreateVehicleResults();

    for( s32 liResult = 0; liResult < lpCreateVehicleResults->GetLength(); ++liResult )
    {
        const BrnPhysics::Vehicle::CreateVehicleResult& lrResult =
            lpCreateVehicleResults->GetEvent( liResult );

        // `ld r11,0(r3) ; srdi r11,r11,32 ; clrlwi r29,r11,0` then `srwi r11,r29,24 ;
        // cmplwi 1 ; bne next` -- the embedded entity word's OWNER byte. Anything that is not
        // a race car is skipped without touching the rest of the body.
        if( lrResult.mVolumeInstanceID.GetEntityIDOwner() !=
            static_cast<u8>( E_ENTITYTYPE_RACECAR ) )
        {
            continue;
        }

        CGS_ASSERT( lrResult.mbSuccess, "Add vehicle failed\n" );          // X360 :5202

        // `extrwi r31, r29, 14, 8` -- bits [10..23] of the entity word == the slot index.
        const s32 liActiveRaceCarIndex =
            static_cast<s32>( lrResult.mVolumeInstanceID.GetEntityIDEntityIndex() );

        CGS_ASSERT( liActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                    "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" );  // X360 :5206
        CGS_ASSERT( liActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" ); // X360 :5207

        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liActiveRaceCarIndex ) );

        // The console's own tripwire between the slot fetch and the model read
        // (0x822FF7F0 `bl IsAttached` -> 0x822FF804 `li r5, 0x441` == BrnActiveRaceCar.h:1089).
        CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );        // BrnActiveRaceCar.h:1089

        // `lwz r3, 0x6F0(r30)` == mpRaceCar (GetGlobalRaceCar), then GetModelId, then the
        // vehicle list read through module+0x18434 == mpVehicleList.
        //
        // PC-SAFETY GUARD (not console behaviour, stated so): the console dereferences
        // mpVehicleList unconditionally (`lwz r3, 0(r29)` @0x822FF828). It is non-NULL by
        // construction here on this build too -- RaceCarStreamer::Prepare takes the list before
        // any car can be created -- but the guard is preferred to a null deref, and it covers
        // ONLY the model-index legs: OnHandlingModelAdded below runs either way, so a missing
        // list can never cost the car its SCENE REGISTRATION, which is the whole point of this
        // leg. A missing list still reports itself through the console's own :5211 assert.
        const s32 liModelIndex =
            ( mpVehicleList != 0 )
                ? mpVehicleList->GetVehicleIndex( lpActiveRaceCar->GetGlobalRaceCar()->GetModelId() )
                : -1;

        CGS_ASSERT( liModelIndex >= 0, "liModelIndex >= 0" );               // X360 :5211

        // ⭐ THE LEG THE WHOLE WAVE HANGS OFF: this is the console's ONLY caller of
        // ActiveRaceCar::OnHandlingModelAdded -> AddToScene -> AddToCollision.
        // Argument order re-derived from the asm: the VEHICLE interface is fetched first
        // (0x822FF85C, stashed in var_B8 -> r5) and the SCENE interface second
        // (0x822FF86C sub_822B63E0 -> r4); f1 is module+0x18398 == mfTimeStep
        // (`lfsx f31, r21, r15` @0x822FF858).
        lpActiveRaceCar->OnHandlingModelAdded( lpOutput->GetSceneInputInterface(),
                                               lpOutput->GetVehicleInputInterface(),
                                               mfTimeStep );

        if( lpActiveRaceCar->IsPlayer() )
        {
            // 0x822FF898..0x822FF8C4: GetVehicleData(liModelIndex) -> `addi r3,r3,0xA0` ->
            // AttribSysCollectionKey::GetHashKey -> GetDirectorVehicleInputInterface() ->
            // NewVehicle(key, liModelIndex).
            //
            // The NULL test on the entry is NOT an added guard: VehicleList::GetVehicleData
            // (VehicleList.cpp:260) already carries its own marked deviation returning 0 for an
            // out-of-range index or an unregistered slot, where the console log-and-continues
            // and indexes anyway. Honouring that contract is required, not optional.
            const BrnResource::VehicleListEntry* lpVehicleData =
                ( mpVehicleList != 0 ) ? mpVehicleList->GetVehicleData( liModelIndex ) : 0;

            if( lpVehicleData == 0 )
            {
                continue;
            }

            const u64 lxAttribsKey = lpVehicleData->GetAttribCollectionKeyHash();

            lpOutput->GetDirectorVehicleInputInterface()->NewVehicle( lxAttribsKey, liModelIndex );

            // [diag, one-shot -- NOT console code] the retired stand-in
            // (PublishNewVehicleToDirectorWithoutPhysicsBringUp) printed this exact line, and
            // it is the head of the chain that ends in the two shared gameplay cameras'
            // Parameters::mbIsValid. Kept, in the leg that now owns the publish, so a boot
            // check for "[newveh] RaceCarEntityModule" still proves the key leaves this module
            // -- i.e. so retiring the stand-in is falsifiable rather than assumed.
            static bool sbLoggedFirstNewVehiclePublish = false;
            if( !sbLoggedFirstNewVehiclePublish
                && ( CgsDev::Message::gxMessageFilterFlags & 1 )
                && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstNewVehiclePublish = true;
                *CgsDev::Log::gpDebugPrint
                    << "[newveh] RaceCarEntityModule::ProcessCreateVehicleEvents: published "
                       "NewVehicle( key hi "
                    << static_cast<s32>( lxAttribsKey >> 32 ) << " lo "
                    << static_cast<s32>( lxAttribsKey & 0xFFFFFFFFu ) << ", modelIndex "
                    << liModelIndex << " ) for active race car slot "
                    << liActiveRaceCarIndex << "\n";
            }
        }
    }
}

// ============================================================================
// SendRaceCarSceneUpdates  @0x822F6B08 (56)
//
// For every ACTIVE slot, run the per-car post-physics scene leg. The console body is one
// eight-iteration loop with the range-guarded EActiveRaceCarIndex `operator++` inlined --
// which is what bakes the "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT" assert
// (BurnoutConstants.h:39) the asm carries at 0x822F6BB4..0x822F6BD0, exactly as the
// same-shaped loops elsewhere in this tree already do.
//
// ⚠️ ARGUMENT ORDER AND THE FLOAT. The asm loads the vehicle-input interface first
// (0x822F6B8C, the +855200 accessor -> r24 -> r5) and the scene-input interface second
// (0x822F6B98, sub_822B63E0 -> r4), then passes (this, scene, vehicle, f1) -- i.e. the
// DWARF's (SceneInputInterface*, VehicleInputInterface*, float32_t). The float is
// module +0x18398 == mfTimeStep (`lis r11,1 ; ori r30,r11,0x8398 ; lfsx f31,r28,r30`), the
// value PreSceneUpdate latches. ActiveRaceCar::SendSceneUpdatesPostPhysics does not read it
// (no f-register touch anywhere in its 46 instructions) -- it is passed because the
// declaration takes it; see that function's own banner.
// ============================================================================
void RaceCarEntityModule::SendRaceCarSceneUpdates(
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );    // X360 :5086

    if( lpOutput == 0 )
    {
        return;
    }

    for( EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++ )
    {
        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leActiveRaceCarIndex );

        if( lpActiveRaceCar->IsActive() )
        {
            lpActiveRaceCar->SendSceneUpdatesPostPhysics( lpOutput->GetSceneInputInterface(),
                                                          lpOutput->GetVehicleInputInterface(),
                                                          mfTimeStep );
        }
    }
}

// ============================================================================
// GenerateSceneUpdateEvents  @0x822D2500 (100)
//
// ⭐ THE CAR'S PER-FRAME SCENE-TRANSFORM PRODUCER. For each of the eight slots the console
// body does (asm 0x822D2540..0x822D2668, loop counter r23 = 8, stride r28 += 0x1CD0):
//     if (!IsActive()) continue;
//     assert IsAttached()                       // BrnActiveRaceCar.h:1096 and :1111, TWICE each
//     id        = *(u64*)(car + 0xD0)           // `ld r30, 0xD0(r28)`   == mHandlingBodyVolumeId
//     transform =  (car + 0x2D0)                // `addi r29, r31, 0x200` (r31 == car+0xD0)
//     sceneInterface->SetVolumeInstanceTransform(id, transform)
//     assert IsAttached() x2 again              // the two accessors' own tripwires, re-emitted
//     entityWord = (u32)(id >> 32)              // `ld; srdi r11,r11,32; clrlwi r30,r11,0`
//     position   = *(Vector4*)(car + 0x300)     // `lvx128 v0, r31, r24` with r24 == 0x230
//     sceneInterface->SetEntityPosition(entityWord, position)
// then, once, sceneInterface->SetPaddingForResetRaceCars-equivalent tail call.
//
// THE TWO OFFSETS ARE NAMED MEMBERS HERE, and both are already attested in this tree:
//   +0x0D0 == ActiveRaceCar::mHandlingBodyVolumeId (BrnActiveRaceCar.h's own layout block);
//   +0x2D0 == ActiveRaceCar::mPhysicsState.mTransform -- mPhysicsState sits at +0xE0 and
//            RaceCarState::mTransform at +496, and 224 + 496 == 720 == 0x2D0 exactly. The
//            position lane at +0x300 is that same matrix's translation row (0x2D0 + 0x30),
//            i.e. `mTransform.Pos()`, NOT a separate member.
// ============================================================================
void RaceCarEntityModule::GenerateSceneUpdateEvents(
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    // The console has no null test here (its caller has already asserted the buffer); this
    // guard is the PC one every leg in this file carries.
    if( lpOutput == 0 )
    {
        return;
    }

    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface =
        lpOutput->GetSceneInputInterface();

    for( EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActiveRaceCarIndex++ )
    {
        ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( leActiveRaceCarIndex );

        if( !lpActiveRaceCar->IsActive() )
        {
            continue;
        }

        // The console emits this tripwire four times per car (the two inlined accessors,
        // each of which asserts twice); it is one predicate, so it is raised once here.
        CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );  // BrnActiveRaceCar.h:1096/:1111

        const Matrix44Affine& lrTransform = lpActiveRaceCar->GetPhysicsState()->mTransform;

        // ⭐ UNPARKED 2026-08-18 (wave Q5 finisher). The handle is read through the DWARF's
        // own accessor, which now exists (BrnActiveRaceCar.h:716 `VolumeInstanceId
        // GetHandlingBodyVolumeId() const` -- BY VALUE, as the DWARF spells it). The console
        // reads the member directly (`ld r30, 0xD0(r28)` @0x822D2578) because it inlines the
        // accessor; nothing here re-derives the handle from (owner, slot).
        const CgsSceneManager::VolumeInstanceId lVolumeInstanceId =
            lpActiveRaceCar->GetHandlingBodyVolumeId();

        lpSceneInterface->SetVolumeInstanceTransform( lVolumeInstanceId, lrTransform );

        // `srdi r11,r11,32 ; clrlwi r30,r11,0` -- the embedded 32-bit entity word, and
        // `lvx128 v0, r31, r24` with r24 == 0x230 (car+0x300 == mTransform + 0x30 == Pos()).
        lpSceneInterface->SetEntityPosition(
            CgsSceneManager::EntityId( static_cast<u32>(
                lVolumeInstanceId.muId
                >> CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_START_INDEX ) ),
            lrTransform.Pos() );
    }

    // ⚠️ WHY THE HANDLE IS READ AND NOT REBUILT -- kept because it is the measured answer, and
    // the next reader will otherwise "simplify" the accessor away. The handle could in fact be
    // rebuilt from (owner, slot) today: ActiveRaceCar::Attach @0x822BEEE0 seeds it with
    // `std r30(=0), 0xD0(r31)`, splices the owner byte with `clrlwi/oris 0x100/sldi 32/or/std`
    // (== E_ENTITYTYPE_RACECAR) and then calls
    // VolumeInstanceId::SetEntityIDEntityIndex(meActiveRaceCarIndex) -- so on this build the
    // value is exactly ((1u << 24) | (slot << 10)) << 32, with the reserved bits and the 8-bit
    // volume index both zero, and nothing ever splices a volume index into a CAR's handle
    // (AddToScene reuses the same 64-bit word as both the VolumeId and the VolumeInstanceId --
    // see BrnActiveRaceCar_wQ5_01.cpp's AddVolumeInstance call). It is deliberately NOT done:
    // the console READS THE MEMBER, and a duplicated composition rule diverges silently the
    // first time the Attach/AddToScene pair changes how it is composed.
    //
    // Both publish callees are REAL and MOUNTED (CgsSceneManagerIO_SceneUpdate.cpp:298 and :55,
    // on build_game_exe.bat line 636).

    // The console's tail call. Runs whether or not the loop published anything.
    SetPaddingForResetRaceCars( lpSceneInterface );
}

// ============================================================================
// SetPaddingForResetRaceCars  @0x822CEEA8
//
// The tail call of GenerateSceneUpdateEvents, and the only reader of mabResetThisFrame.
// The console body is the BitArray<8> set-bit iterator inlined whole (the
// `x & -x` / `cntlzd` lowest-set-bit idiom at 0x822CEEF0..0x822CEF10 and again at
// 0x822CF168..0x822CF188, with the 64-bit-block re-scan in between), so it is written here
// with the container's own GetFirstNonZeroBit / GetNextNonZeroBit -- same walk, by name.
//
//   for each set bit i:
//       if (maActiveRaceCars[i].IsActive())
//           sceneInterface->ClearEntityVolumesPadding( EntityId(E_ENTITYTYPE_RACECAR, i, 0) )
//   mabResetThisFrame.UnSetAll()
//
// The entity id is built by the console as `slwi r11,r31,10 ; oris r4,r11,0x100`
// (0x822CEFB4/0x822CEFBC) -- index at bit 10, owner byte 1 -- which is EntityId::Set(1, i, 0).
// Only ONE of Set()'s three asserts survives in the asm (CgsEntityId.h:116, the entity-index
// bound); the other two are compile-time provable for a literal owner of 1 and part of 0.
// Using Set() reproduces the emitted one exactly and does not add the other two at run time
// beyond what the shared header already does for every caller.
//
// The trailing clear is unconditional and is reached from EVERY exit path in the console
// (`std r22, 0(r19)` at loc_822CF194 with r22 == 0, which all four early branches jump to).
// ============================================================================
void RaceCarEntityModule::SetPaddingForResetRaceCars(
        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface )
{
    if( lpSceneInterface != 0 )
    {
        for( s32 liActiveRaceCar = mabResetThisFrame.GetFirstNonZeroBit();
             liActiveRaceCar != CgsContainers::BitArray<8u>::KI_INVALID_BITINDEX;
             liActiveRaceCar = mabResetThisFrame.GetNextNonZeroBit( liActiveRaceCar ) )
        {
            ActiveRaceCar* lpActiveRaceCar =
                GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liActiveRaceCar ) );

            if( lpActiveRaceCar->IsActive() )
            {
                CgsSceneManager::EntityId lEntityId;
                lEntityId.Set( static_cast<u32>( E_ENTITYTYPE_RACECAR ),
                               static_cast<u32>( liActiveRaceCar ),
                               0 );
                lpSceneInterface->ClearEntityVolumesPadding( lEntityId );
            }
        }
    }

    mabResetThisFrame.UnSetAll();
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
//   ⭐ THREE MORE LEGS LANDED 2026-08-18 (wave Q5, cluster G1 -- the scene-collision middle):
//     ProcessCreateVehicleEvents (0x823075F8), GenerateSceneUpdateEvents (0x82307658, inside
//     the paused skip) and SendRaceCarSceneUpdates (0x8230773C). Their bodies and the full
//     re-derived console call order are in the block banner above them.
// [FLAG PC bring-up] the rest is dropped, NOT paraphrased. DELETE-WHEN the interior lands.
//
// ⭐ THE INPUT READ-LOCK MOVED TO THE CONSOLE'S OWN POSITION (2026-08-18). It used to be
// taken half-way down, inside the `if (lpInput != 0)` arm around the physics readback. The
// console takes it at 0x823075BC -- BEFORE the first leg -- and holds it to 0x82307980, and
// ProcessCreateVehicleEvents' own input accessors carry "Not locked for reading" tripwires,
// so calling that leg at its console slot required the console's lock scope.
void RaceCarEntityModule::PostPhysicsUpdate(
        RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput,
        BrnUpdateSet lUpdateSet )
{
    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );      // X360 :2640
    CGS_ASSERT( lpOutput != 0, "lpOutput != NULL" );    // X360 :2641

    if( lpOutput == 0 )
    {
        return;
    }

    // Bit 0 of the update set is the "sim paused" bit (the same bit PreSceneUpdate's time-step
    // latch tests). The console's `bne cr6, loc_823076C0` at 0x82307610 skips the whole
    // physics-readback / scene-update / contact block on it.
    const bool lbSimPaused = ( ( lUpdateSet & 1 ) != 0 );

    if( lpInput != 0 )
    {
        lpInput->LockForRead();
    }
    lpOutput->LockForWrite();

    // ⚠️ FLAG PC quality-of-life: UN-BLEND BEFORE THIS TICK'S PRODUCERS RUN.
    // RenderParams currently holds the last rendered frame's interpolated pose; put the real
    // tick pose back, so the latch at the bottom of this function can only ever capture a
    // genuine tick pose and never its own blended output. Pairs with the latch loop below --
    // the two must stay in the same function, bracketing every pose producer.
    // (Currently changes no value here -- every active slot is written every tick by either
    // the readback or a stand-in -- but see the scope note on ActiveRaceCar.)
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
        if( lpActiveRaceCar->IsActive() )
        {
            lpActiveRaceCar->RestoreTickRenderPose();
        }
    }

    // ⭐⭐ THE REAL LEG, at its own console position (@0x823075F8 -- early, BEFORE the
    // physics readback at @0x8230761C). Landed 2026-08-18 (wave Q5); its own banner carries
    // the re-verification that killed the "the queue is permanently empty" claim.
    //
    // ⭐ THE STAND-IN THAT USED TO SIT HERE IS GONE. PublishNewVehicleToDirectorWithoutPhysics-
    // BringUp published the director NewVehicle event on an invented trigger because this leg
    // was parked one declaration wide; that declaration landed (wave Q5 finisher,
    // VehicleManagerOutputInterface::GetCreateVehicleResults) and the loop below now publishes
    // it on the CONSOLE's trigger -- the create-vehicle result itself, in the player arm, with
    // the same two values from the same two sources. Keeping both would post the director a
    // duplicate NewVehicleEvent the console never emits, so the stand-in and its edge latch are
    // deleted rather than left beside the real leg.
    ProcessCreateVehicleEvents( lpInput, lpOutput );

    // ⭐⭐ THE PHYSICS READBACK, at the console's own position (`bl` at 0x8230761C, before
    // UpdateActiveRaceCarColours @0x823076C4 and UpdateOutputInterfaces @0x8230771C).
    // Landed 2026-08-11 (physics-return-path wave). It is the real producer of every active
    // car's pose; the bring-up pose publish below now only covers the slots the readback
    // holds back (see ReadUpdatedActiveRaceCarDataFromPhysics' mUsedRaceCars banner).
    if( lpInput != 0 )
    {
        // (The console's read lock is taken at the top of this function, at its own slot --
        // see the banner. It used to be taken here.)
        ReadUpdatedActiveRaceCarDataFromPhysics( lpInput );

        // [FLAG PC bring-up] the pose STAND-IN. Until the vehicle manager populates
        // VehicleOutputInterface::maRaceCarStates / mUsedRaceCars the readback holds back
        // every active car, and nothing else publishes a render pose at all -- so the
        // stand-in still runs, for exactly those slots. See
        // PublishRenderPoseWithoutPhysicsBringUp's banner.
        // ⚠️ It is deliberately SKIPPED for any slot the readback has just posed, so the
        // stand-in's rest-pose approximation can never overwrite a real physics pose. The
        // two are conductor-retired together once the producer lands.
        const CgsContainers::BitArray<8u>& lrUsedRaceCars =
            lpInput->GetVehicleOutputInterface()->GetUsedCarsBitArray();

        for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
        {
            ActiveRaceCar* lpActiveRaceCar =
                GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
            if( lpActiveRaceCar->IsActive()
                && !lrUsedRaceCars.IsBitSet( static_cast<u32>( liCar ) ) )
            {
                // Physics does NOT own this slot: both halves of the pose are stand-ins.
                // (PublishRenderPoseWithoutPhysicsBringUp calls the wheel half at its own
                // tail -- the pre-split pairing; the 2026-08-12 wheel-only exists-gate that
                // used to sit in the else-arm below is retired with the real producer.)
                PublishRenderPoseWithoutPhysicsBringUp( lpActiveRaceCar, liCar );
            }
            // ⭐⭐ THE REAL WHEEL POSE (wheel-transform wave 2026-08-13). Physics owns this
            // slot: SimpleVehiclePhysics::GetWheelsWorldTransfrom @0x825D8878 is BODIED and
            // WriteOutVehicleStats' four SetWheelTransform calls are UNPARKED, so
            // ActiveRaceCar::UpdatePhysicsState has just copied REAL per-wheel world
            // matrices (spin + steer + suspension) into mRenderParams.
            //
            // [FLAG PC bring-up] THE EXISTS FLAG ALONE is still stood in for, and the
            // divergence is named: the console's writer of the render-side wheel exists is
            // the DEFORMATION half of this very readback (L3 -> ActiveRaceCar::
            // UpdateWheelPhysicsState @0x822B8738, from the deformation output's per-wheel
            // on-ground bytes) -- RaceCarState::mabWheelExists (+0x446) has NO writer
            // anywhere in the XEX (full 30,084-export store scan, 2026-08-13; only the copy
            // ctor @0x8220A4C0 propagates it), so UpdatePhysicsState's copy of it is false
            // by construction on both platforms. Until the deformation publish lands, force
            // the four road wheels visible exactly as L3 would for an intact car.
            // DELETE-WHEN L3 (the deformation-output wheel-state publish) lands.
            else if( lpActiveRaceCar->IsActive() )
            {
                for( u32 luWheel = 0; luWheel < 4u; ++luWheel )
                {
                    lpActiveRaceCar->GetRenderParams()->SetWheelExists( luWheel, true );
                }

                static bool sbReportedWheelExistsSeam = false;
                if( !sbReportedWheelExistsSeam && CgsDev::Log::gpDebugPrint != 0 )
                {
                    sbReportedWheelExistsSeam = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] wheel EXISTS forced true for the four road "
                           "wheels of physics-owned cars: the transforms are REAL "
                           "(GetWheelsWorldTransfrom @0x825D8878 landed) but the exists "
                           "byte's console producer is the parked deformation leg "
                           "(UpdateWheelPhysicsState @0x822B8738). DELETE-WHEN L3 lands.\n";
                }
            }
        }
    }
    else
    {
        // [FLAG PC bring-up] no input buffer at all -> no readback is possible, so the
        // stand-in owns every active slot exactly as it did before this wave.
        for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
        {
            ActiveRaceCar* lpActiveRaceCar =
                GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
            if( lpActiveRaceCar->IsActive() )
            {
                PublishRenderPoseWithoutPhysicsBringUp( lpActiveRaceCar, liCar );
            }
        }
    }

    // ⭐⭐ THE CAR'S PER-FRAME SCENE PUBLISH, at the console's own position: the `bl` at
    // 0x82307658, i.e. immediately after the physics readback (0x8230761C) and the five-byte
    // AggressiveDrivingFlags copy, and INSIDE the sim-paused skip that starts at 0x82307610.
    // Landed 2026-08-18 (wave Q5, cluster G1).
    //
    // [FLAG PC bring-up] the console leg BETWEEN the readback and this call is NOT reproduced:
    // the 5-byte copy of VehicleOutputInterface::mAggressiveDrivingFlags (input +0x6C00) into
    // module +0x1836C (asm 0x82307628..0x8230764C, a `mtctr 5` byte loop). Its destination is
    // still inside this module's maTailPadB0 span -- no DWARF-named member is pinned there --
    // so it is named here rather than faked. Nothing in the scene chain reads it.
    if( !lbSimPaused )
    {
        GenerateSceneUpdateEvents( lpOutput );
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
    // [DIAG pose-scale, one-shot] Is the published render pose ORTHONORMAL? The interpolator
    // blends it, and every blend this engine owns (rw::math::vpu::SLerp @0x82216858,
    // OrthoNormalize3x3 @0x82203B28) re-normalises the three axis rows -- which is only
    // lossless on a matrix whose rows are already unit length. Print the row magnitudes and
    // the row dot products once, so the question is answered by measurement.
    {
        static bool sbLoggedPoseScale = false;
        if( !sbLoggedPoseScale && CgsDev::Log::gpDebugPrint != 0 )
        {
            for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT && !sbLoggedPoseScale; ++liCar )
            {
                ActiveRaceCar* lpCar = GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
                if( !lpCar->IsActive() )
                    continue;
                sbLoggedPoseScale = true;
                const Matrix44Affine& lrB = lpCar->GetRenderParams()->GetBodyTransform();
                const Matrix44Affine& lrW = lpCar->GetRenderParams()->GetWheelTransform( 0u );
                #define KI_ROWMAG( r ) sqrtf( ( r ).x * ( r ).x + ( r ).y * ( r ).y + ( r ).z * ( r ).z )
                #define KI_ROWDOT( a, b ) ( ( a ).x * ( b ).x + ( a ).y * ( b ).y + ( a ).z * ( b ).z )
                *CgsDev::Log::gpDebugPrint
                    << "[pose-scale] car " << liCar
                    << " body |x| " << KI_ROWMAG( lrB.xAxis )
                    << " |y| " << KI_ROWMAG( lrB.yAxis )
                    << " |z| " << KI_ROWMAG( lrB.zAxis )
                    << " x.y " << KI_ROWDOT( lrB.xAxis, lrB.yAxis )
                    << " x.z " << KI_ROWDOT( lrB.xAxis, lrB.zAxis )
                    << " y.z " << KI_ROWDOT( lrB.yAxis, lrB.zAxis )
                    << " | wheel0 |x| " << KI_ROWMAG( lrW.xAxis )
                    << " |y| " << KI_ROWMAG( lrW.yAxis )
                    << " |z| " << KI_ROWMAG( lrW.zAxis )
                    << " x.y " << KI_ROWDOT( lrW.xAxis, lrW.yAxis ) << "\n";
                #undef KI_ROWMAG
                #undef KI_ROWDOT
            }
        }
    }

    // ⚠️ FLAG PC quality-of-life: LATCH THIS TICK'S RENDER POSE, for every active slot.
    //
    // Here and nowhere else, because THIS is the one point in the tick at which every
    // producer has finished: the physics readback above owns the slots in mUsedRaceCars and
    // the two stand-ins own the rest, and both have run by now. Latching inside either
    // producer would double-latch the slots the other one also touches, and a double latch
    // silently collapses the blend to a no-op -- the judder would come back looking like it
    // had never been fixed. See ActiveRaceCar::LatchTickRenderPose.
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar( static_cast<EActiveRaceCarIndex>( liCar ) );
        if( lpActiveRaceCar->IsActive() )
        {
            lpActiveRaceCar->LatchTickRenderPose();
        }
    }

    UpdateActiveRaceCarColours();

    // The console reads the four interfaces off the output buffer in this order
    // (replayGlobal, replayActive, global, active) and passes them
    // (active, global, replayActive, replayGlobal).
    UpdateOutputInterfaces( lpOutput->GetActiveRaceCarOutputInterface(),
                            lpOutput->GetGlobalRaceCarOutputInterface(),
                            lpOutput->GetReplayActiveRaceCarOutputInterface(),
                            lpOutput->GetReplayGlobalRaceCarOutputInterface() );

    // ⭐⭐ THE PER-CAR CULLING / LATE-COLLISION REFRESH, at the console's own position: the
    // `bl` at 0x8230773C, i.e. after UpdateOutputInterfaces (0x8230771C) and
    // TransmitCarsInRaceToQueryManager (0x82307730), and OUTSIDE the sim-paused skip (the
    // second skip only starts at 0x82307744, one instruction later). Landed 2026-08-18.
    SendRaceCarSceneUpdates( lpOutput );

    SendStreamerEvents( lpOutput );

    // The console's own unlock order: read first (0x82307980), then write (0x82307988).
    if( lpInput != 0 )
    {
        lpInput->UnlockForRead();
    }
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

    // [FLAG PC bring-up] the paused / not-paused split and its EIGHT remaining calls -- see the
    // banner. ⭐ ProcessPlayerVehicleInput is no longer one of them (player-input wave
    // 2026-08-11): it sits at the console's own slot in the not-paused arm, between UpdateBoost
    // and ProcessTakedownEvents (X360 call site 0x8230732C, immediately after the
    // UpdateBoost @0x82307318 call and immediately before ProcessTakedownEvents @0x82307340).
    // The console passes the SIM time step in f1 here (`lfs f1, 0(r31)` at 0x82307324, r31 ==
    // &mfTimeStep); mfTimeStep is the member PreSceneUpdate latches, so it is what we forward.
    // ⚠️ AND THE ARM IS PAUSE-GATED (verifier catch, 2026-08-11): the console runs the whole
    // not-paused arm only when bit 0 of lUpdateSet is CLEAR (`clrlwi r30, r31, 31` at insn 14,
    // tested at 0x82307230) -- an unconditional call would publish a driver-controls event on
    // PAUSED frames. The gate below is the console's own test, not a PC addition.

    if( ( lUpdateSet & 1 ) == 0 )
    {
        // ⭐⭐ THE IGNITION SLOT (engine wave 2026-08-12). The console runs UpdateActiveCars
        // at 0x823072F0, BEFORE ProcessPlayerVehicleInput at 0x8230732C, in the same
        // not-paused arm -- so the engine reaches RUNNING on the same frame the driver record
        // that reads it is built. That ORDER is load-bearing: swap the two and the gas lags
        // the ignition by a frame.
        //
        // All three floats are loaded by the console right here, from the module, at
        // 0x82307294..0x823072C4 (`lfs f31, 0(r31)` with r31 == &mfTimeStep, then
        // `lfsx f29, r29, 0x183C8` and `lfsx f30, r29, 0x183CC`), and 0x183C8 / 0x183CC are
        // mPlayerVehicleControls + 32 / + 36 == mfAcceleration / mfBraking.
        UpdateActiveCars( mfTimeStep,
                          mPlayerVehicleControls.mfAcceleration,
                          mPlayerVehicleControls.mfBraking );

        // Breaker @0x823072FC..0x82307318: tailgate state is updated first,
        // then the writable game-event queue is passed to UpdateBoost. The PC
        // bring-up path can tick before a player is attached; use the same
        // temporary precondition gate as ProcessPlayerVehicleInput below.
        if( static_cast<u32>( mePlayerActiveRaceCarIndex ) < E_ACTIVE_RACE_CAR_INDEX_COUNT
            && GetActiveRaceCar( mePlayerActiveRaceCarIndex )->IsAttached() )
        {
            UpdateTailgateTimer( mfTimeStep );
            UpdateBoost( mfTimeStep, lpInput, lpOutput->GetGameEventQueue() );
        }

        ProcessPlayerVehicleInput( mfTimeStep, lpInput, lpOutput );
    }

    mPlaceOnTrackManager.PrePhysicsUpdate( lpInput, lpOutput );

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
}

// ============================================================================
// UpdateActiveCars  @ 0x822FF250   (73 instructions)   -- PARTIAL SLICE
//   (engine wave 2026-08-12)
//
// The eight-slot active-car tick. Structurally the whole console body is here: the loop base
// `addi r24, r31, 0x1A60` (== &maActiveRaceCars[0]), the count `li r20, 8`, the stride
// `addi r24, r24, 0x1CD0` (7376 == sizeof(ActiveRaceCar) on the console), the IsActive gate
// at 0x822FF2EC and the Update call at 0x822FF344.
//
// ---- THE CONSOLE'S ARGUMENTS -------------------------------------------------------------
// PrePhysicsUpdate @0x823072CC..0x823072F0 passes thirteen things; this slice forwards the
// three the ignition needs and drops the rest, each named:
//   f1  mfTimeStep (+0x18398)                      ✔ forwarded
//   f4  mPlayerVehicleControls.mfAcceleration      ✔ forwarded
//   f5  mPlayerVehicleControls.mfBraking           ✔ forwarded
//   f2  module +0x183A0        f3  module +0x183A4 (-> ActiveRaceCar::CalculateWheelAngular-
//                                                   Velocities, which does not exist yet)
//   r4  sub_822B5EA0(lpOutput)         r10 InputBuffer_PrePhysics::GetInHardStopCamera()
//   stack: lpVehicleOutput, module +0x18490 (the RNG), module +0x18368 (meGameModeType)
//   v1/v2  two Vector3s from module +0x18720 / +0x18730 (the route vectors)
// Every one of those feeds a part of ActiveRaceCar::Update this build has not landed -- see
// that function's banner for the drop list.
//
// ---- [FLAG PC bring-up] DROPPED HERE ------------------------------------------------------
//  * the `lpVehicleOutput != NULL` assert (X360 :4335) -- the pointer is not plumbed.
//  * the tail call SendAddedForCollisionStateToPhysics(lpVehicleOutput) @0x822FF360: it walks
//    each car's mAddRemoveNetworkCarForCollisionQueue, and the producer for that queue
//    (ActiveRaceCar::SendAddedRemovedNetworkCarForCollisionEvents @0x822BF840) is itself
//    dropped by Update's slice, so running it here would drain a queue nothing fills.
// ============================================================================
void RaceCarEntityModule::UpdateActiveCars( f32 lfTimeStep, f32 lfAcceleration, f32 lfBraking )
{
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        ActiveRaceCar& lrCar = maActiveRaceCars[liCar];

        if( lrCar.IsActive() )                                   // 0x822FF2EC
        {
            // mbIsInOnlineGameMode / mbInCarSelectScreen are the console's own
            // `lbzx r10, r31, 0x18345` / `lbzx r8, r31, 0x186C9` -- both read from `this`.
            lrCar.Update( lfTimeStep, lfAcceleration, lfBraking,
                          mbIsInOnlineGameMode, mbInCarSelectScreen );
        }
    }
}

// ============================================================================
// UpdateBoost @ 0x82304690 -- regular gameplay branch.
//
// The signature is taken from the PPC call at 0x82307300..0x82307318, not the
// decompiler: r3=this, f1=lfTimeStep, r5=lpInput and r6=lpEventQueue. All
// speed/time inputs below are scalar `lfs` values. Only the in-air rotations
// are a genuine Vector3 load; fsubs/fsel then chooses max(abs(y), abs(z)).
//
// Two regular-branch side effects remain outside this bounded boost closure:
// the post-takedown AI RenderDamaged latch needs the unhomed damaged-car count,
// and OnSlammed needs the still-opaque aggressive-driving flags at +0x1836C.
// ============================================================================
void RaceCarEntityModule::UpdateBoost(
        f32 lfTimeStep,
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::GameEventQueue* lpEventQueue )
{
    const f32 KF_MPH_TO_MPS                 = 0.447039992f; // bits 0x3EE4E26D
    const f32 KF_MIN_SPEED_FOR_BOOST_MPH    = 25.0f;
    const f32 KF_MIN_IN_AIR_SPEED_MPH       = 2.0f;
    const f32 KF_MIN_TAILGATE_DURATION      = 1.0f;
    const f32 KF_DEFAULT_BOOST_MODIFIER     = 1.0f;
    const f32 KF_FIRST_PLACE_BOOST_MODIFIER = 0.5f;

    f32 lfBoostModifier = KF_DEFAULT_BOOST_MODIFIER;
    const BrnGameState::GameStateModuleIO::ScoringOutputInterface* lpScoringInterface =
            lpInput->GetScoringInterface();

    CGS_ASSERT( mePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                "mePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" );
    CGS_ASSERT( mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

    ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
    CGS_ASSERT( lpActiveRaceCar->IsAttached(), "lpActiveRaceCar->IsAttached()" );

    const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveRaceCar->GetPhysicsState();
    const Vector3& lrRotations = lpActiveRaceCar->GetCurrentInAirRotations();
    const f32 lfAbsYaw  = std::fabs( lrRotations.y );
    const f32 lfAbsRoll = std::fabs( lrRotations.z );

    mBoostManager.SetSpinAngle( lfAbsYaw >= lfAbsRoll ? lfAbsYaw : lfAbsRoll );
    mBoostManager.SetCrashing( lpRaceCarState->mbCrashing );
    mBoostManager.SetWrecking(
            lpActiveRaceCar->IsWrecked() && lpActiveRaceCar->IsCrashing(),
            mbIsInOnlineGameMode );

    const bool lbBoostRequested =
            ( mPlayerVehicleControls.mbBoost
              || meActivePaybackType == BrnNetwork::E_PAYBACK_TYPE_BOOST_LOCK )
            && !lpRaceCarState->mbCrashing
            && lpRaceCarState->mfSpeedMPH >= KF_MIN_SPEED_FOR_BOOST_MPH
            && lpActiveRaceCar->GetEngineState()
                    == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING
            && !lpActiveRaceCar->IsInAnyRaceStartState();
    mBoostManager.SetBoostRequested( lbBoostRequested );

    const bool lbInAir = lpRaceCarState->mfTimeInAir > 0.0f
                         && std::fabs( lpRaceCarState->mfSpeedMPH )
                                > KF_MIN_IN_AIR_SPEED_MPH;
    // The DecFIGS manager wrapper takes f32 and applies its 0.5f threshold;
    // Breaker has already reduced this call site's conditions to a boolean.
    mBoostManager.SetInAir( static_cast<f32>( lbInAir ) );
    mBoostManager.SetSpeed( lpRaceCarState->mfSpeedMPH * KF_MPH_TO_MPS );
    mBoostManager.SetDrifting( lpRaceCarState->mfTimeDrifting > 0.0f );
    mBoostManager.SetTailgating(
            mfCurrentTailgateDuration > KF_MIN_TAILGATE_DURATION,
            meIndexOfCarPlayerIsTailgating );

    const RaceCarEntityModuleIO::TakedownEventQueue* lpTakedownQueue =
            lpInput->GetTakedownEventQueue();
    for( s32 liEvent = 0; liEvent < lpTakedownQueue->GetLength(); ++liEvent )
    {
        const BrnGameState::TakedownEvent& lrEvent = lpTakedownQueue->GetEvent( liEvent );
        if( static_cast<s32>( lrEvent.meAggressorIndex )
                == static_cast<s32>( mePlayerActiveRaceCarIndex ) )
        {
            mBoostManager.GetBoostStrategy()->OnTakedown();
        }
    }

    const BrnGameState::GameStateModuleIO::CarScoreData& lrPlayerScore =
            lpScoringInterface->maCarScoreData[mePlayerActiveRaceCarIndex];
    const s32 liRacePosition = lrPlayerScore.GetRacePosition();
    if( lpScoringInterface->mbIsOnlineGameMode
        && liRacePosition > 0
        && lpScoringInterface->miNumPlayersInGame > 1
        && liRacePosition <= lpScoringInterface->miNumPlayersInGame )
    {
        CGS_ASSERT( liRacePosition <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    "liRacePosition <= E_ACTIVE_RACE_CAR_INDEX_COUNT" );

        const f32 lfPositionRatio =
                static_cast<f32>( liRacePosition - 1 )
                / static_cast<f32>( lpScoringInterface->miNumPlayersInGame - 1 );
        CGS_ASSERT( lfPositionRatio > -0.01f, "lfPositionRatio > -0.01f" );
        CGS_ASSERT( lfPositionRatio < 1.01f, "lfPositionRatio < 1.01f" );
        lfBoostModifier = KF_FIRST_PLACE_BOOST_MODIFIER + lfPositionRatio;
    }

    mBoostManager.UpdateChainExploits( lpActiveRaceCar->GetPosition() );
    mBoostManager.UpdateJustBounceBoostedTimer( lfTimeStep );
    mBoostManager.Update( lpEventQueue, lfTimeStep, lfBoostModifier );
}

// ============================================================================
// ProcessPlayerVehicleInput  @ 0x822FFE30   (572 instructions)   -- COMPLETE
//   (player-input wave 2026-08-11)
//
// ⭐⭐ THE MISSING LINK OF THE CONTROLS CHAIN. Everything upstream of it was already landed
// (pad -> CgsInputPads -> GameBridgeControllerToX -> BrnWorldIO::UpdateInputBuffer ->
// WorldBridgeInputToEntityModules -> InputBuffer_PreScene::SetPlayerVehicleControls ->
// PreSceneUpdate's `memcpy(module + 99240, controls, 60)`) and everything downstream was already
// landed (WorldBridgeEntityModulesToPhysics -> PhysicsModule input -> VehicleManager::
// UpdateDrivers -> VehiclePhysics::UpdateDriving). This is the ONE hop between them: it turns
// the latched BrnWorld::PlayerVehicleControls into the 72-byte
// BrnPhysics::Vehicle::BrnPlayerDriverControls event and AddEvents it into the output buffer's
// VehicleDriverInputInterface queue. Nothing else in the XEX produces that record for the
// player.
//
// ---- SIGNATURE (PPC FLOAT-ARG TRAP, re-derived from the asm) -------------------------------
// Hex-Rays prints eight ints and a trailing double. The caller says otherwise:
//     0x82307324  lfs  f1, 0(r31)        r31 == &mfTimeStep     -> f1  = lfTimeStep
//     0x82307320  mr   r5, r24           r24 == lpInput         -> r5  = lpInput
//     0x8230731C  mr   r6, r26           r26 == lpOutput        -> r6  = lpOutput
//     0x82307328  mr   r3, r29           r29 == the module      -> r3  = this
// and the callee's prologue agrees: `mr r23, r5` (lpInput), `mr r19, r6` (lpOutput), `mr r28, r3`
// (this). r4 is NEVER READ in the callee -- the float takes f1 and SKIPS its GPR slot, which is
// exactly the PPC float-arg rule. The body never references f1 either, so lfTimeStep is an
// unused parameter here; it is kept because it is what the call site passes.
//
// ---- THE RECORD ----------------------------------------------------------------------------
// Every one of the 26 fields is filled, and BrnPlayerDriverControls::_AssertLayout() pins all 26
// offsets (the record crosses the module boundary by memcpy, so a slip is live corruption).
// meDriverType (+0x44) is set by the default constructor -- see that ctor's banner for why the
// `stw r26, var_BC` at instruction #14 is a ctor and not a body store.
//
// ---- SHAPE ---------------------------------------------------------------------------------
//   asserts (lpInput, mePlayerActiveRaceCarIndex >= 0, IsAttached, muType in range)
//   if (the player's paired global car is an AI car) return;              // muType == 1
//   record.miVehicleID = mePlayerActiveRaceCarIndex
//   the taken-down latch (clear it in Showtime, else let the crash state gate the controller)
//   if (controller active && engine RUNNING)   -> the LIVE fill
//   else                                       -> the ZERO fill (steering survives if the
//                                                  controller is active)
//   record.mfBoostMaxSpeedScale = 1.0f
//   the "online mode just finished" park (gas 0, handbrake 1, steering hard to +/-1)
//   record.miVehicleIDToMerge = -1
//   if (online) { the per-mode tweak (race catch-up / burning-home-run blue-team cap) then the
//                 payback "dirty trick" switch }
//   AddEvent into lpOutput->GetVehicleDriverInterface()'s driver queue
//
// ---- CONSTANTS (all asm-literal) ------------------------------------------------------------
//   flt_82001CC0 0.0    flt_82001C98 1.0    flt_82014A8C 1.5     flt_82013F90 0.001
//   flt_820148D0 4.5    flt_820148D4 0.55   flt_82014930 0.8     flt_820037C8 -1.0
//   flt_8201F7F8 0.1    flt_82005450 0.9
//
// ---- DIVERGENCES / FLAGS --------------------------------------------------------------------
//  1. The default arm of the payback switch streams the offending value into the assert message
//     on the console (`"Unknown dirty trick type " << meActivePaybackType`); CGS_ASSERT takes a
//     fixed string, so the value is dropped from the TEXT only -- the assert itself fires at the
//     same place, on the same condition.
//  2. The tilt-steering remap is emitted as a VMX sign/deadzone sequence with no console symbol;
//     it is outlined below as a file-static helper (NOT a console function -- see its banner).
//  3. [FLAG PC bring-up] a LOUD log-once early-out when there is no attached player car. The
//     console has no such test -- see the gate's own comment for why the console body would
//     otherwise index maActiveRaceCars[-1] on this build's first pre-physics frames.
// ============================================================================

// [FLAG] NOT an X360 symbol. The tilt("sixaxis")-steering remap ProcessPlayerVehicleInput emits
// inline at 0x823001BC..0x8230025C, outlined here for legibility per the project's
// undo-inlining rule. Transcribed instruction for instruction:
//     f0  = mfXSensor * 4.5f                              (fmuls, flt_820148D0)
//     v0  = (mfXSensor >= 0) ? ((mfXSensor > 0) ? 1.0f : 0.0f) : -1.0f
//                                                         (vcmpgefp/vcmpgtfp + two vsel)
//     f13 = |f0| - 0.55f                                  (fabs/fsubs, flt_820148D4)
//     f13 = (-(f13) >= 0) ? 0.0f : f13                    (fsel f13, -f13, 0.0f, f13)
//                                                         i.e. clamp the excess at zero
//     result = v0 * f13 + f0                              (vmulfp128 then fadds)
// -- i.e. a signed, deadzone-widened AMPLIFICATION of the scaled tilt, not a subtraction. The
// VMX splats are the compiler broadcasting a scalar through a vector register; only lane 0 is
// ever read back (`lfs f30, var_120`), so the scalar form below is exact.
static f32 RemapTiltSteering( f32 lfTiltSensor )
{
    const f32 lfScaled = lfTiltSensor * 4.5f;

    f32 lfSign = -1.0f;
    if( lfTiltSensor >= 0.0f )
    {
        lfSign = ( lfTiltSensor > 0.0f ) ? 1.0f : 0.0f;
    }

    f32 lfExcess = ( lfScaled < 0.0f ? -lfScaled : lfScaled ) - 0.55f;
    if( lfExcess <= 0.0f )
    {
        lfExcess = 0.0f;
    }

    return lfSign * lfExcess + lfScaled;
}

void RaceCarEntityModule::ProcessPlayerVehicleInput(
        f32 lfTimeStep,
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    // f1 is passed by the call site and never read by the console body (no f1 reference in the
    // whole 572-instruction listing). Kept in the signature to match the call.
    (void)lfTimeStep;

    // The ctor's `meDriverType = E_DRIVER_TYPE_PLAYER` is the X360's instruction-#14
    // `stw r26, 0x170+var_BC(r1)`.
    BrnPhysics::Vehicle::BrnPlayerDriverControls lControls;

    CGS_ASSERT( lpInput != 0, "lpInput" );                                          // X360 :6037

    bool lbControllerActive = lpInput->GetControllerActive();


    // ⛔ [FLAG PC bring-up] LOUD GATE, NOT A SILENT NO-OP -- and it is LOAD-BEARING, not
    // defensive padding. The console's own flow guarantees a valid, attached player slot by the
    // time PrePhysicsUpdate runs; this build does not (see PrePhysicsUpdate's :1726 banner --
    // mePlayerActiveRaceCarIndex stays E_ACTIVE_RACE_CAR_INDEX_INVALID == -1 until the junkyard
    // reset action lands, and the world ticks pre-physics frames before that). Without this gate
    // the very next line indexes maActiveRaceCars[-1] and the line after dereferences a NULL
    // mpRaceCar, i.e. the console body would take the process down on frame one. The console has
    // NO such test; the two CGS_ASSERTs above and below are its whole guarantee.
    // DELETE-WHEN a player car is attached before the world's first pre-physics frame.
    if( static_cast<u32>( mePlayerActiveRaceCarIndex ) >= E_ACTIVE_RACE_CAR_INDEX_COUNT
        || !GetActiveRaceCar( mePlayerActiveRaceCarIndex )->IsAttached() )
    {
        static bool sbReportedNoAttachedPlayerCar = false;
        if( !sbReportedNoAttachedPlayerCar )
        {
            sbReportedNoAttachedPlayerCar = true;
            if( CgsDev::Log::gpDebugPrint != 0 )
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] RaceCarEntityModule::ProcessPlayerVehicleInput: no "
                       "attached player active race car -- the frame's BrnPlayerDriverControls "
                       "record is NOT published (X360 has no such gate; its asserts at :6041 / "
                       "BrnActiveRaceCar.h:1089 are the console's only guarantee). "
                       "Reported once, not per frame\n";
        }
        return;
    }

    // ⚠️ CONSOLE-ORDER DEVIATION (conductor, 2026-08-11, measured): the X360 fires this assert
    // BEFORE any test (:6041, its first act after GetControllerActive). On this build the world
    // spine ticks pre-physics frames from the MARKETING SCREENS on, so the console order halted
    // the game once per frame -- 243 assert dialogs in 78 s, boot never reached the flyby. The
    // assert is now scoped BEHIND the bring-up gate above: on every frame the console's
    // precondition can hold (an attached player car exists) it is checked exactly as the console
    // wrote it; on the pre-car frames the gate's log-once carries the signal instead.
    // DELETE-WHEN the gate above goes (then restore the console position).
    CGS_ASSERT( mePlayerActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                "mePlayerActiveRaceCarIndex >= 0" );                                // X360 :6041

    ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );

    CGS_ASSERT( lpActiveRaceCar->IsAttached(), "IsAttached()" );          // BrnActiveRaceCar.h:1089

    const RaceCar* lpGlobalRaceCar = lpActiveRaceCar->GetGlobalRaceCar();

    CGS_ASSERT( lpGlobalRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                "muType < E_RACE_CAR_TYPE_COUNT" );                            // BrnRaceCar.h:603

    // The player's paired global slot being an AI car means there is no player input to publish
    // this frame; the console branches straight to the epilogue (0x822FFF44 -> 0x82300714),
    // i.e. it does NOT AddEvent.
    if( lpGlobalRaceCar->GetType() == E_RACE_CAR_TYPE_AI )
    {
        return;
    }

    lControls.miVehicleID = mePlayerActiveRaceCarIndex;

    // The taken-down latch. In Showtime a takedown is consumed outright; otherwise the car keeps
    // its controller only while the physics snapshot says it is not crashing.
    if( lpActiveRaceCar->IsTakenDown() )
    {
        if( lpActiveRaceCar->IsInShowtime() )
        {
            lpActiveRaceCar->SetTakenDown( false );
        }
        else
        {
            lbControllerActive = !lpActiveRaceCar->GetPhysicsState()->mbCrashing;
        }
    }

    // lfGas / lfSteering shadow the console's f27 / f30, which survive past the two fill arms
    // because the online tweaks below re-derive the record's gas and steering from them.
    f32 lfGas      = 0.0f;
    f32 lfSteering = 0.0f;

    if( lbControllerActive
        && lpActiveRaceCar->GetEngineState()
               == RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING )
    {
        // ---- the LIVE fill ----------------------------------------------------------------
        // Breaker virtual-dispatches IsBoosting through the selected strategy
        // unconditionally. Prepare stage 0 now establishes that pointer.
        lControls.mbBoost = mBoostManager.GetBoostStrategy()->IsBoosting();

        lControls.mbReset  = mPlayerVehicleControls.mbReset;
        lControls.mbToggle = mPlayerVehicleControls.mbToggle;

        const f32 lfInvulnerabilityTime = lpActiveRaceCar->GetInvulnerabilityTime();
        lControls.mbIsInvulnerableToVehicles = lfInvulnerabilityTime > 0.0f;
        lControls.mbIsInvulnerableToWorld    = lfInvulnerabilityTime > 0.0f;

        if( lpActiveRaceCar->IsCrashing() && lpActiveRaceCar->IsWrecked() )
        {
            lfGas                  = 0.0f;
            lControls.mfGas        = 0.0f;
            lControls.mfBrake      = 0.0f;
            lControls.mfHandBrake  = 0.0f;

            // A wrecked Showtime car is pinned: full brake AND full handbrake.
            if( lpActiveRaceCar->IsInShowtime() )
            {
                lControls.mfBrake     = 1.0f;
                lControls.mfHandBrake = 1.0f;
            }
        }
        else
        {
            lfGas                 = mPlayerVehicleControls.mfAcceleration;
            lControls.mfGas       = mPlayerVehicleControls.mfAcceleration;
            lControls.mfBrake     = mPlayerVehicleControls.mfBraking;
            lControls.mfHandBrake = mPlayerVehicleControls.mfHandBrake;
        }

        lControls.mfForwardSteering = mPlayerVehicleControls.mfYAxis0;
        lControls.mfSpin            = mPlayerVehicleControls.mfSpin;
        lControls.mfRequestedGas    = mPlayerVehicleControls.mfAcceleration;
        lControls.mfAftertouchLevel = mCrashPlayManager.GetAftertouchLevel();

        // Hand this frame's stomped/leaped cars to the physics side's target-assist list.
        for( s32 liStompee = 0; liStompee < miStoredStompeeCount; ++liStompee )
        {
            lpOutput->GetVehicleDriverInterface()->AddTargetAssist(
                    mStoredStompees[liStompee].mPosition,
                    mStoredStompees[liStompee].mEntityId );
        }

        // Tilt steering is only armed when it is switched on, we are not in Showtime, and the
        // player is not already on a wheel.
        if( ( !mbSixaxisSteeringEnabled && !mbPaybackSixaxisSteering )
            || mCrashPlayManager.IsInShowtime()
            || mPlayerVehicleControls.mbIsWheel )
        {
            lControls.mbIsSteeringWheel = mPlayerVehicleControls.mbIsWheel;
            lfSteering                  = mPlayerVehicleControls.mfSteering;
        }
        else
        {
            lControls.mbIsSteeringWheel = true;
            lfSteering                  = RemapTiltSteering( mPlayerVehicleControls.mfXSensor );
        }

        lControls.mfSteering    = lfSteering;
        lControls.mfXSensor     = mPlayerVehicleControls.mfXSensor;
        lControls.mfYSensor     = mPlayerVehicleControls.mfYSensor;
        lControls.mfZSensor     = mPlayerVehicleControls.mfZSensor;
        lControls.mfGSensor     = mPlayerVehicleControls.mfGSensor;
        lControls.mbForceDrift  = false;
        lControls.mbBoostBounce = mCrashPlayManager.IsBounceBoosting();
        lControls.mbHorn        = mPlayerVehicleControls.mbHorn;
        lControls.mbIsOnStartLine =
                lpActiveRaceCar->IsOnRaceStartState( ActiveRaceCar::E_RACE_START_STATE_ON_START_LINE );
    }
    else
    {
        // ---- the ZERO fill ----------------------------------------------------------------
        // Everything off. The one survivor is the raw steering, and only while the controller is
        // active (an engine that is off/starting/stopping still lets the wheels be turned).
        lControls.mbBoost                    = false;
        lControls.mbReset                    = false;
        lControls.mbToggle                   = false;
        lControls.mbForceDrift               = false;
        lControls.mbBoostBounce              = false;
        lControls.mbIsInvulnerableToVehicles = false;
        lControls.mbIsInvulnerableToWorld    = false;

        lfGas                       = 0.0f;
        lControls.mfGas             = 0.0f;
        lControls.mfBrake           = 0.0f;
        lControls.mfHandBrake       = 0.0f;
        lControls.mfForwardSteering = 0.0f;
        lControls.mfAftertouchLevel = 0.0f;
        lControls.mfSpin            = 0.0f;
        lControls.mfRequestedGas    = 0.0f;

        lfSteering = lbControllerActive ? mPlayerVehicleControls.mfSteering : 0.0f;

        lControls.mfSteering        = lfSteering;
        lControls.mfXSensor         = 0.0f;
        lControls.mfYSensor         = 0.0f;
        lControls.mfZSensor         = 0.0f;
        lControls.mfGSensor         = 0.0f;
        lControls.mbIsOnStartLine   = false;
        lControls.mbIsSteeringWheel = false;
        lControls.mbHorn            = false;
    }

    lControls.mfBoostMaxSpeedScale = 1.0f;

    // The end-of-online-mode park: coast to a stop with the handbrake on and the wheels at full
    // lock, keeping the sign the player last had.
    if( mbOnlineModeJustFinished )
    {
        lfSteering            = ( lfSteering >= 0.0f ) ? 1.0f : -1.0f;
        lControls.mfBrake     = 0.0f;
        lControls.mfHandBrake = 1.0f;
        lfGas                 = 0.0f;
        lControls.mfGas       = 0.0f;
        lControls.mfSteering  = lfSteering;
    }

    lControls.miVehicleIDToMerge = -1;

    if( mbIsInOnlineGameMode )
    {
        if( meGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE )
        {
            CGS_ASSERT( lpInput != 0, "lpInput" );                                  // X360 :6191
            CGS_ASSERT( lpInput->GetScoringInterface() != 0,
                        "lpInput->GetScoringInterface()" );                         // X360 :6192

            const RaceCarEntityModuleIO::InputBuffer_PrePhysics::ScoringInterface* lpScoring =
                    lpInput->GetScoringInterface();

            if( lpScoring->miNumPlayersInGame > 1 )
            {
                const EActiveRaceCarIndex leActiveRaceCarIndex =
                        lpActiveRaceCar->GetActiveRaceCarIndex();

                CGS_ASSERT( leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                            "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" );  // X360 :6205
                CGS_ASSERT( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                            "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" ); // X360 :6206

                // Online-race catch-up: the leader keeps 90% throttle, last place keeps 100%,
                // linearly between. The console does both int->float converts with fcfid/frsp,
                // i.e. a plain (f32) cast of the s32.
                const f32 lfPositionFromFront =
                        static_cast<f32>( lpScoring->maCarScoreData[leActiveRaceCarIndex]
                                                  .GetRacePosition() - 1 );
                const f32 lfPositionRange =
                        static_cast<f32>( lpScoring->miNumPlayersInGame - 1 );

                lControls.mfGas =
                        ( ( lfPositionFromFront / lfPositionRange ) * 0.1f + 0.9f ) * lfGas;
            }
        }
        else if( meGameModeType
                 == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN )
        {
            const EActiveRaceCarIndex leActiveRaceCarIndex =
                    lpActiveRaceCar->GetActiveRaceCarIndex();

            CGS_ASSERT( leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                        "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0" );      // X360 :6225
            CGS_ASSERT( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );   // X360 :6226

            // The blue (chasing) team is boost-speed capped in Burning Home Run.
            if( lpInput->GetOnlineScoringInterface()->maePlayerTeam[leActiveRaceCarIndex]
                    == BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM )
            {
                lControls.mfBoostMaxSpeedScale = 0.8f;
            }
        }

        // The payback "dirty tricks" the aggressor gets to play on this car.
        switch( meActivePaybackType )
        {
        case BrnNetwork::E_PAYBACK_TYPE_REVERSE_STEERING:
            lControls.mfBrake     = 0.0f;
            lControls.mfSteering  = -lfSteering;
            lControls.mfHandBrake = 0.0f;
            break;

        case BrnNetwork::E_PAYBACK_TYPE_BOOST_LOCK:
            lControls.mfBrake     = 0.0f;
            lControls.mbBoost     = true;
            lControls.mfHandBrake = 0.0f;
            break;

        case BrnNetwork::E_PAYBACK_TYPE_AGGRESSORS_CONTROLS_AFFECTS_VICTIM:
            CGS_ASSERT( meActivePaybackAggressor >= E_ACTIVE_RACE_CAR_INDEX_0,
                        "meActivePaybackAggressor >= E_ACTIVE_RACE_CAR_INDEX_0" );  // X360 :6266
            CGS_ASSERT( meActivePaybackAggressor < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        "meActivePaybackAggressor < E_ACTIVE_RACE_CAR_INDEX_COUNT" ); // X360 :6267

            // The console narrows the index to a byte here (`stb r11, var_C8`); the record's
            // slot is an s8.
            lControls.miVehicleIDToMerge = static_cast<s8>( meActivePaybackAggressor );
            break;

        case BrnNetwork::E_PAYBACK_TYPE_SIX_AXIS_STEERING:
            // Disarm the trick and give the car its boost economy back.
            mbPaybackSixaxisSteering = false;
            mfRandomBoostTime        = -1.0f;
            mbRandomBoostOn          = false;
            mBoostManager.SetBoostEarningEnabled( true );
            break;

        default:
            // The console streams the offending value into the message; see divergence 2.
            CGS_ASSERT( false, "Unknown dirty trick type" );                        // X360 :6297
            break;
        }
    }

    // ⭐ THE PUBLISH. r3 == GetVehicleDriverInterface(lpOutput) (0x822B5CA8 returns
    // buffer + 142192, which is mVehicleDriverInterface -- NOT mGameEventQueue, see that
    // accessor's own note), r4 == &record, r5 == 0 (the event type). The queue is the
    // interface's first member, which is why the console passes the interface pointer straight
    // to VariableEventQueue<5040,16>::AddEvent<BrnPlayerDriverControls>.

    lpOutput->GetVehicleDriverInterface()->GetUpdateDriverQueue()->AddEvent( &lControls, 0 );
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
//   HandleStopModeAction, HandleGameActions,
//   (ProcessPlayerVehicleInput RETIRED from this list 2026-08-11 -- it is COMPLETE above.
//    It was never a [VMX] function either: the only vector work is the tilt-steering
//    sign/deadzone sequence, which is scalar maths the compiler happened to vectorise, plus a
//    plain Vector3 load in the stompee loop. Landing it needed the CrashPlayManager scalar
//    tail, the BoostManager's BoostStrategy pointer and mPlayerVehicleControls modelled --
//    all three are now named members in the header.)
//   (ProcessCreateVehicleEvents RETIRED from this list 2026-08-18, wave Q5 -- it is bodied
//    above, COMPLETE, with no park at all (the queue-reader declaration it waited on landed in
//    the same wave). It was never a [VMX] function: its 182 instructions contain no vector
//    opcode at all. What actually blocked it was the "the create-vehicle result queue has no
//    producer" claim, which this wave re-verified and found STALE.)
//   ProcessRaceCarCrashCompleteEvents, ProcessResetOnTrackResultQueue,
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
//   SendGameEvents, SendStreamerEvents, SendAddedForCollisionStateToPhysics,
//   (SendRaceCarSceneUpdates RETIRED from this list 2026-08-18, wave Q5 -- COMPLETE above,
//    with no park at all. Its one "un-homed interior" callee, ActiveRaceCar::
//    SendSceneUpdatesPostPhysics, landed in the same wave. GenerateSceneUpdateEvents and
//    SetPaddingForResetRaceCars are bodied above too and were never on this list.)
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
