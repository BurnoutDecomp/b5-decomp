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
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"                      // Attrib::Gen::burnoutcarasset (the new-vehicle residency gate)
#include "rw/math/vpu/vector3_operation.h"                                               // rw::math::vpu::IsValid(Vector3)
#include "rw/math/vpu/matrix44affine_operation.h"                                        // rw::math::vpu::IsValid(Matrix44Affine) / Mult
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec::WheelSpec (the authored wheel placements)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"                 // VehiclePhysics::SeatTransformFromCreateLegBringUp (the analytic rest seat, seat wave 2026-08-05)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverControls.h"             // BrnPhysics::Vehicle::BrnPlayerDriverControls (the 72-byte player record)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"       // VehicleDriverInputInterface::AddTargetAssist / GetUpdateDriverQueue
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostStrategy.h"       // BrnWorld::BoostStrategy::IsBoosting (vtable slot 19)

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

    // ---- THE GRAPHICS-FRAME STEP (seat wave 2026-08-05) ---------------------
    // CalcBodyTransform yields the MODEL-frame pose (COM * physics -- the console's own
    // composition, now fed by the real seat + the shipped spec+1552 matrix). But the shipped
    // GraphicsSpec PART LOCATORS are NOT authored in that frame: every locator translation in
    // VEH_PUSMC01_GR.BIN is POSITIVE-Y (bumpers +0.73, arch parts +0.325 -- real part heights
    // above the GROUND), i.e. the locator frame sits ONE MORE model->handling step below the
    // model frame. MEASURED, not tuned: with body = M1552 * modelFrame the front-arch locators
    // (+0.325) land at ground+0.290 against wheel centres at ground+0.296 -- a 1 mm fit --
    // and the pre-seat builds' look confirms it (body drawn AT the raw ground transform looked
    // grounded; wheels composed with the same matrix hung 0.4 m under the floor).
    // The factor is the SHIPPED spec+1552 matrix applied once more -- no invented offset.
    // ⚠️ FLAG: the console mechanism carrying this step (RenderRaceCar's own consumption of
    // mBodyTransform, or the GraphicsSpec's mppRigidBodyToSkinMatrixTransforms table this leg
    // ignores) is NOT yet recovered; this is the measured stand-in inside an already-flagged
    // bring-up leg. The WHEELS keep composing against the MODEL frame (WheelSpec positions are
    // model-space -- verified by the same measurement).
    const RaceCarStreamer::PhysicsResourcePtr& lrPhysicsResource =
        mRaceCarStreamer.GetPhysicsResourceBringUp( liActiveRaceCar );

    if ( lrPhysicsResource.HasMemoryResource() )
    {
        lpRenderParams->SetBodyTransform( rw::math::vpu::Mult(
            lrPhysicsResource.operator->()->mCarModelSpaceToHandlingBodySpaceTransform,
            lBodyTransform ) );
    }
    else
    {
        lpRenderParams->SetBodyTransform( lBodyTransform );
    }

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
//   * the WHEEL pose <- the SetWheelTransform publish in the physics half -- STILL PARKED
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
// ⛔ THE REAL PRODUCER, NAMED, NOT HIDDEN. The console fills RaceCarState::maWheelTransforms
// in VehicleManager::WriteOutVehicleStats via SimpleVehiclePhysics::GetWheelsWorldTransfrom
// @0x825D8878 -- 868 VMX128 instructions, declared in BrnSimpleVehiclePhysics.h with NO BODY
// anywhere in this tree (see park (3) in BrnVehicleManager_WriteOutVehicleStats.cpp), and the
// console's writer of RaceCarState::mabWheelExists (+0x446) is not identified in EITHER half
// of the publish. Until BOTH land, nothing on this build can produce a wheel pose, and this
// stand-in is the only thing between the player and an empty pair of arches.
// DELETE-WHEN GetWheelsWorldTransfrom @0x825D8878 is bodied and mabWheelExists has a writer.
//
// ⚠️ THE GATE IT CARRIES NOW is the console's OWN field, not a bring-up invention: it runs
// only for a car for which the physics readback published NO wheel at all
// (mabWheelExists false for all four). The instant the real producer sets even one of those
// bytes, this stops running for that car with no further edit here -- exactly the property
// the mUsedRaceCars gate has for the body half.
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
// [FLAG PC bring-up] PublishNewVehicleToDirectorWithoutPhysicsBringUp -- NOT an X360
// function. It stands in for exactly ONE console leg, in the console's own frame slot.
//
// ⭐⭐ WHAT IT STANDS IN FOR, and why the real function cannot simply be transcribed.
// The console's ProcessCreateVehicleEvents @0x822FF620 (called from PostPhysicsUpdate
// @0x823075F8, i.e. right here) walks
//     lpInput->GetVehicleManagerOutputInterface()->mCreateVehicleResultQueue
// and, for each result whose active-race-car slot IsPlayer(), does
//     lpOutput->GetDirectorVehicleInputInterface()->NewVehicle(
//         VehicleList::GetVehicleData(liModelIndex)->AttribSysCollectionKey.GetHashKey(),
//         liModelIndex );
// (asm @0x822FF898..@0x822FF8C4: GetVehicleData -> `addi r3,r3,0xA0` -> GetHashKey ->
// NewVehicle(interface, key, liModelIndex)).
//
// ⛔ THAT QUEUE IS PERMANENTLY EMPTY ON THIS BUILD, and it is not a mount away from being
// filled. The ONLY producer of a CreateVehicleResult anywhere in the XEX is
// BrnPhysics::Vehicle::VehicleManager::ProcessCreateEvents @0x82616770 -- verified as the
// single entry in the xrefs_to set of BaseEventQueue<CreateVehicleResult>::AddEvent
// @0x825E4EC8. BrnVehicleManager.cpp is not on the build list and that function has no body
// here, so transcribing @0x822FF620 verbatim would add a loop over an always-zero-length
// queue: a body with no input, which is as dead as no body. It stays in this file's FLAG
// INVENTORY, correctly, under [VMX]/[INTERIOR].
//
// WHAT IS HONEST ABOUT THIS STAND-IN:
//   * it publishes ONLY for the player's car, which is the only case the console's own
//     `if (ActiveRaceCar::IsPlayer())` arm reaches NewVehicle in;
//   * the two published values are read from the SAME two sources the console reads them
//     from -- VehicleList::GetVehicleIndex(RaceCar::GetModelId()) for the index and the
//     entry's own AttribSysCollectionKey hash for the key;
//   * it runs in PostPhysicsUpdate at the console's own position for the function it
//     replaces (before the physics readback, not after);
//   * NewVehicle itself is NOT stood in for -- the real @0x822CBA90 body runs, asserts and
//     all (GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.cpp).
//
// WHAT IS A LIE, stated plainly:
//   * the TRIGGER. The console publishes when PHYSICS finishes creating the vehicle; this
//     publishes when the player's ActiveRaceCar slot reports a model the vehicle list knows
//     AND that car's attribute collection has actually arrived. The edge is detected with a
//     function-local static rather than a member because this module's layout is offset-pinned
//     and this state is not the console's.
//   * the console also runs OnHandlingModelAdded per created vehicle; that leg reaches
//     un-homed physics/AI interiors and is NOT reproduced here.
//
// ⚠️⚠️ THE RESIDENCY GATE IS NOT OPTIONAL, and it is the console's ordering, not ours.
// MEASURED (CAM_RUN1, before the gate existed): the publish fired the moment the player's
// ActiveRaceCar attached, which on this build is ~150 log lines BEFORE
// `Vehicles\VEH_PUSMC01_AT.bin` is streamed and its vault registered
// ("[ATTRIBSYS LOAD] Just loaded vault resource with ID 109b0d7b00000000"). So
// Attrib::FindCollection missed, every generated ctor substituted
// Attrib::DefaultDataArea (zeros), and the chain delivered FOUR-TEEN new asserts and a
// VALID-but-all-zero parameter block (mrFOV 0, mfBoostFOV 0) -- the exact "wrong-but-plausible
// data" failure this project keeps hitting. On the console the create-vehicle completion is
// downstream of the car's resource load by construction, so the collection is always there;
// gating on the resolve is how that ordering is reproduced without a physics module.
// The key itself was never wrong: it is byte-present, little-endian, at +0x398 of our own
// ported VEH_PUSMC01_AT.BIN.
//
// DELETE-WHEN VehicleManager::ProcessCreateEvents lands and ProcessCreateVehicleEvents can
// be transcribed against a queue that is actually written.
// ============================================================================
void RaceCarEntityModule::PublishNewVehicleToDirectorWithoutPhysicsBringUp(
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput )
{
    // [FLAG PC bring-up] the edge state -- see the banner.
    static s32 sliPublishedModelIndex = -1;

    if( lpOutput == 0 || mpVehicleList == 0 )
    {
        return;
    }
    if( mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }

    const ActiveRaceCar* lpActiveRaceCar = GetActiveRaceCar( mePlayerActiveRaceCarIndex );
    if( lpActiveRaceCar == 0 || !lpActiveRaceCar->IsAttached() )
    {
        return;
    }

    const RaceCar* lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
    if( lpRaceCar == 0 )
    {
        return;
    }

    // The console's own two lines: model id -> vehicle-list index -> entry -> attrib key.
    const s32 liModelIndex = mpVehicleList->GetVehicleIndex( lpRaceCar->GetModelId() );
    CGS_ASSERT( liModelIndex >= 0, "liModelIndex >= 0" );          // X360 :5211
    if( liModelIndex < 0 || liModelIndex == sliPublishedModelIndex )
    {
        return;
    }

    const BrnResource::VehicleListEntry* lpEntry = mpVehicleList->GetVehicleData( liModelIndex );
    if( lpEntry == 0 )
    {
        return;
    }

    const u64 lxAttribsKey = lpEntry->GetAttribCollectionKeyHash();

    // ⚠️ THE RESIDENCY GATE -- see the banner. Retry every frame until the car's own
    // burnoutcarasset collection is in the attribute database; publishing before it is what
    // the console's physics-driven ordering makes impossible.
    if( lxAttribsKey == 0 ||
        Attrib::FindCollection(
            Attrib::Gen::burnoutcarasset::KU_BURNOUTCARASSET_CLASS_KEY, lxAttribsKey ) == 0 )
    {
        return;
    }

    sliPublishedModelIndex = liModelIndex;

    lpOutput->GetDirectorVehicleInputInterface()->NewVehicle( lxAttribsKey, liModelIndex );

    // [diag, one-shot -- NOT console code] this is the head of the chain that ends in the two
    // shared gameplay cameras' Parameters::mbIsValid; the line proves the key really leaves
    // the race-car module. Remove with the FLAG above.
    if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) && CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[newveh] RaceCarEntityModule: published NewVehicle( key hi "
            << static_cast<s32>( lxAttribsKey >> 32 ) << " lo "
            << static_cast<s32>( lxAttribsKey & 0xFFFFFFFFu ) << ", modelIndex "
            << liModelIndex << " )\n";
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
// REPRODUCED: the queue walk, case 0 (ResetPlayerCarAction) and case 79
// (CarSelectChangeColourAction -> ChangePlayerCarColour). Neither is an arbitrary choice --
// they are the two actions on this build that have a live producer (CarSelectManager) AND a
// fully reachable consumer, and together they are the whole "spawn the player's car, then
// paint it its authored colour" pair.
//
// [FLAG PC bring-up] every other case is DROPPED, not paraphrased. The named handlers the
// console dispatches to and that are still un-reconstructed:
//   3   RaceCar::RequestResetOnTrack        4   HandleSetPlayerOpponentsAction
//   5   HandleSetupNetworkCarAction         7   the player-control-changed AI publish
//   11  HandleRemotePlayerDisconnected      23  HandlePrepareForModeAction
//   34  the payback arm                     39  HandleStopModeAction
//   73/74/76/77     the car-select / drive-thru arms
//   126 SwitchCarColourAction (an AI car's colour; asserts :7393/:7397/:7398)
//   219 the network setup-car arm, which also writes the colour pair (:7212/:7215)
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

    // ---- step 12: LATCH THIS FRAME'S PAD STATE ------------------------------
    // ⛔⛔ THIS WAS A SILENT DROP, AND IT IS THE ONE THAT KEPT THE CAR PARKED.
    // ProcessPlayerVehicleInput reads mPlayerVehicleControls by name at fifteen sites; NOTHING
    // in this tree ever wrote it, so it served its zero-initialised value for ever and every
    // control the player (or the harness) pressed was thrown away one hop before it was used.
    // ⚠️ The label lied: ProcessPlayerVehicleInput's own banner at :2837 lists
    // "PreSceneUpdate's `memcpy(module + 99240, controls, 60)`" among the things that "was
    // already landed". It was not in the body. MEASURED 2026-08-12: `[controls-diag] FIRST
    // NON-ZERO control reached the race-car pre-scene buffer -- accel 1.000000` in the SAME run
    // where the physics probe read `gas 0.000` on every frame.
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
    (void)lUpdateSet;

    if( lpOutput == 0 )
    {
        return;
    }

    lpOutput->LockForWrite();

    // ⭐ THE NEW-VEHICLE PUBLISH, at the console's own position for
    // ProcessCreateVehicleEvents (@0x823075F8 -- early, BEFORE the physics readback at
    // @0x8230761C). See PublishNewVehicleToDirectorWithoutPhysicsBringUp's banner.
    PublishNewVehicleToDirectorWithoutPhysicsBringUp( lpOutput );

    // ⭐⭐ THE PHYSICS READBACK, at the console's own position (`bl` at 0x8230761C, before
    // UpdateActiveRaceCarColours @0x823076C4 and UpdateOutputInterfaces @0x8230771C).
    // Landed 2026-08-11 (physics-return-path wave). It is the real producer of every active
    // car's pose; the bring-up pose publish below now only covers the slots the readback
    // holds back (see ReadUpdatedActiveRaceCarDataFromPhysics' mUsedRaceCars banner).
    if( lpInput != 0 )
    {
        // The console locks the post-physics input buffer around this whole half; the
        // getters' own "Not locked for reading" asserts are what require it.
        lpInput->LockForRead();

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
                PublishRenderPoseWithoutPhysicsBringUp( lpActiveRaceCar, liCar );
            }
            // ⭐⭐ THE WHEEL HALF, ON ITS OWN GATE (carrender wave 2026-08-12).
            // A car the readback HAS posed still has no wheel pose, because the physics
            // side's wheel publish is parked (SimpleVehiclePhysics::GetWheelsWorldTransfrom
            // @0x825D8878 is bodyless -- park (3) of WriteOutVehicleStats). The gate is the
            // console's OWN field: run only when the readback published no wheel at all.
            // MEASURED before this line existed: all four `exists 0`, all four transforms
            // (0,0,0), wheel block outcome 3 -- the arches drew empty.
            else if( lpActiveRaceCar->IsActive()
                     && !lpActiveRaceCar->GetRenderParams()->GetWheelExists( 0u )
                     && !lpActiveRaceCar->GetRenderParams()->GetWheelExists( 1u )
                     && !lpActiveRaceCar->GetRenderParams()->GetWheelExists( 2u )
                     && !lpActiveRaceCar->GetRenderParams()->GetWheelExists( 3u ) )
            {
                PublishWheelPoseWithoutPhysicsBringUp( lpActiveRaceCar, liCar );

                static bool sbReportedWheelStandIn = false;
                if( !sbReportedWheelStandIn && CgsDev::Log::gpDebugPrint != 0 )
                {
                    sbReportedWheelStandIn = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] the WHEEL pose stand-in is running for a car the "
                           "physics readback HAS posed: RaceCarState::mabWheelExists is false for "
                           "all four wheels because SimpleVehiclePhysics::GetWheelsWorldTransfrom "
                           "@0x825D8878 (868 insns) has no body and mabWheelExists (+0x446) has no "
                           "identified writer. Wheels are drawn at their AUTHORED REST positions "
                           "with the body's orientation -- NO suspension travel, NO steer, NO "
                           "spin. DELETE-WHEN both land.\n";
                }
            }
        }

        lpInput->UnlockForRead();
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
        ProcessPlayerVehicleInput( mfTimeStep, lpInput, lpOutput );
    }

    mPlaceOnTrackManager.PrePhysicsUpdate( lpInput, lpOutput );

    lpOutput->UnlockForWrite();
    lpInput->UnlockForRead();
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
//  1. [FLAG PC bring-up] mBoostManager.GetBoostStrategy() is NULL on this build (BoostManager::
//     Prepare is a documented keystone stub, and it is the console's only writer of that
//     pointer). The console dispatches IsBoosting() through it unconditionally; here it is
//     null-guarded so the boot survives, and the guard is the ONLY added behaviour in this body.
//     DELETE-WHEN BoostManager::Prepare lands.
//  2. The default arm of the payback switch streams the offending value into the assert message
//     on the console (`"Unknown dirty trick type " << meActivePaybackType`); CGS_ASSERT takes a
//     fixed string, so the value is dropped from the TEXT only -- the assert itself fires at the
//     same place, on the same condition.
//  3. The tilt-steering remap is emitted as a VMX sign/deadzone sequence with no console symbol;
//     it is outlined below as a file-static helper (NOT a console function -- see its banner).
//  4. [FLAG PC bring-up] a LOUD log-once early-out when there is no attached player car. The
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
        // [FLAG PC bring-up] the null guard -- see divergence 1 in the banner.
        const BoostStrategy* lpBoostStrategy = mBoostManager.GetBoostStrategy();
        lControls.mbBoost = ( lpBoostStrategy != 0 ) && lpBoostStrategy->IsBoosting();

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
