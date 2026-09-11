// ============================================================================
// BrnTrafficEntityModule_wG_PostScene.cpp
//
// The post-scene stage helpers TrafficEntityModule::PostSceneUpdate calls, plus the
// AI-entity builder one of them uses:
//   PostNearbyTrafficSceneQueryRequest   -- the player-centred coarse sphere query
//   CleanUpCrashedVehicles               -- drain the crash module's cleanup queue
//   HandleCrashingNetworkTraffic         -- drain the network crash-start queue
//   ConvertSceneResultsToTrafficDataForAI-- last frame's per-race-car nearby-traffic
//                                           lists into the AI interface
//   AIPostSceneQueryRequests             -- one coarse frustum query per active race car
//   CreateTrafficAIEntity                -- build one AI-visible traffic record
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"      // RaceCarState
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h"

namespace BrnTraffic
{

namespace
{
    // The two scene-query ids this module registers for its own coarse queries. Both are
    // image .data constants read back from the retail build; the matching result consumers
    // (ProcessNearbyTrafficSceneQueryResults / StoreAISceneResultsForNextFrame, neither
    // reconstructed yet) compare against the same two words, the AI one as a base plus the
    // active-race-car index.
    const u32 KU_NEARBY_TRAFFIC_SPHERE_QUERY_ID = 99u;
    const u32 KU_AI_FRUSTUM_QUERY_ID_BASE       = 31438u;

    // The sphere query around the frame camera: radius, and the entity-type mask it carries.
    const f32 KF_NEARBY_TRAFFIC_QUERY_RADIUS  = 70.0f;
    const u32 KU_NEARBY_TRAFFIC_ENTITY_TYPES  = 12u;

    // The per-race-car AI query camera: a wide, shallow slab centred ahead of the car.
    const f32 KF_AI_QUERY_CAMERA_FOV_HORIZONTAL = 0.5f;
    const f32 KF_AI_QUERY_CAMERA_ASPECT_RATIO   = 6.0f;
    const f32 KF_AI_QUERY_CAMERA_NEAR_CLIP      = 26.0f;
    const f32 KF_AI_QUERY_CAMERA_FAR_CLIP       = 130.0f;
    // How far back along the car's At axis the query camera sits.
    const f32 KF_AI_QUERY_CAMERA_PULLBACK       = 30.0f;
    const u32 KU_AI_QUERY_ENTITY_TYPES          = 8u;

    // The traffic-vehicle entity owner. FLAG: BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE has no
    // home in the tree yet; the literal matches the same spelling already used by
    // BrnCrashModeScoring.cpp. Fold both onto the enum when it lands.
    const u32 KU_OWNER_TRAFFIC_VEHICLE = 2u;
}

// ----------------------------------------------------------------------------
// PostNearbyTrafficSceneQueryRequest
//
// While the player car is active, post one coarse sphere test centred on last frame's
// camera position. Its results come back through ProcessNearbyTrafficSceneQueryResults.
// ----------------------------------------------------------------------------
void TrafficEntityModule::PostNearbyTrafficSceneQueryRequest(
    const BrnTrafficIO::InputBuffer_PostScene* lpInput,
    BrnTrafficIO::OutputBuffer_PostScene*      lpOutput )
{
    if ( !lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive() )
    {
        return;
    }

    CgsSceneManager::SceneManagerIO::InEventSphereTest lEvent;
    lEvent.mCentre                = mCameraLastFrame.GetPosition();
    lEvent.mQueryId.mId           = KU_NEARBY_TRAFFIC_SPHERE_QUERY_ID;
    lEvent.mx32EntityTypeFlags    = KU_NEARBY_TRAFFIC_ENTITY_TYPES;
    lEvent.mfRadius               = KF_NEARBY_TRAFFIC_QUERY_RADIUS;

    lpOutput->GetSceneCoarseQueryQueue()->AddEvent(
        &lEvent, CgsSceneManager::SceneManagerIO::E_IN_EVENT_SPHERE_TEST );
}

// ----------------------------------------------------------------------------
// CleanUpCrashedVehicles
//
// The crash module hands back the traffic volumes it has finished with. Each one that is
// still alive here is removed from the world and loses its "added to the crash module" bit.
//
// NOT CleanUpCrashedVehiclePhysics: that one takes the pre-physics OUTPUT buffer.
// ----------------------------------------------------------------------------
void TrafficEntityModule::CleanUpCrashedVehicles( const BrnTrafficIO::InputBuffer_PostScene* lpInput )
{
    CGS_ASSERT( lpInput != NULL, "lpInput != NULL" );

    const BrnWorld::CrashIO::TrafficOutputInterface::CleanupTrafficEventQueue& lrQueue =
        lpInput->GetCrashTrafficOutputInterface()->GetCleanupTrafficEventQueue();

    for ( s32 liEvent = 0; liEvent < lrQueue.GetLength(); ++liEvent )
    {
        const u32 luVehicle =
            lrQueue.GetEvent( liEvent ).mVolumeInstanceId.GetEntityIDEntityIndex();
        CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luVehicle < KU_MAX_TOTAL_TRAFFIC" );

        if ( GetVehicle( luVehicle )->IsAlive() )
        {
            RemoveVehicle( luVehicle );
            mVehiclesAddedToCrashModule.UnSetBit( luVehicle );
        }
    }
}

// ----------------------------------------------------------------------------
// HandleCrashingNetworkTraffic
//
// Online only. The crash module names the network-replicated traffic vehicles that must
// start crashing here; they are checked for duplicates, then queued for this frame's
// crash pass.
// ----------------------------------------------------------------------------
void TrafficEntityModule::HandleCrashingNetworkTraffic( const BrnTrafficIO::InputBuffer_PostScene* lpInput )
{
    CGS_ASSERT( lpInput != NULL, "lpInput != NULL" );
    CGS_ASSERT( mbIsOnlineGameMode, "IsPlayingOnlineGameMode()" );

    const BrnWorld::CrashIO::TrafficOutputInterface::CrashNetworkTrafficQueue& lrQueue =
        lpInput->GetCrashTrafficOutputInterface()->GetStartCrashingNetworkTrafficQueue();

    for ( s32 liEvent = 0; liEvent < lrQueue.GetLength(); ++liEvent )
    {
        const u16 lu16Vehicle = lrQueue.GetEvent( liEvent ).muVehicleId;

        for ( s32 liOther = liEvent + 1; liOther < lrQueue.GetLength(); ++liOther )
        {
            CGS_ASSERT( lu16Vehicle != lrQueue.GetEvent( liOther ).muVehicleId,
                        "Duplicate crash body message in input buffer" );
        }
    }

    for ( s32 liEvent = 0; liEvent < lrQueue.GetLength(); ++liEvent )
    {
        const u32 luVehicle = lrQueue.GetEvent( liEvent ).muVehicleId;
        CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luVehicle < KU_MAX_TOTAL_TRAFFIC" );

        maNewCrashedNetworkVehicles.Append( static_cast<u16>( luVehicle ) );
    }
}

// ----------------------------------------------------------------------------
// CreateTrafficAIEntity
//
// Build one AI-visible record for traffic vehicle luIndex: its planar centre and velocity
// (both world XZ) and its eight world-space bounding-box corners.
// ----------------------------------------------------------------------------
void TrafficEntityModule::CreateTrafficAIEntity( u32                             luIndex,
                                                 const VehicleTypeRuntime*       lpVehicleTypeRuntime,
                                                 EActiveRaceCarIndex             leRaceCarIndex,
                                                 BrnTrafficIO::TrafficAIEntity*  lpOutEntity ) const
{
    CGS_ASSERT( luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" );

    const Matrix44Affine lTransform = GetVehicleTransform( luIndex );
    const Vehicle*       lpVehicle  = GetVehicle( luIndex );

    const Vector3 lBBoxOffset   = lpVehicleTypeRuntime->GetBBoxOffset();
    const Vector3 lBBoxHalfSize = lpVehicleTypeRuntime->GetBBoxHalfSize();

    lpOutEntity->meNearbyRaceCarIndex = leRaceCarIndex;
    lpOutEntity->mVelocity.SetZero();

    // The AI works in the world XZ plane: lane 0 is world X, lane 1 is world Z. Only those
    // two lanes of mCentre are written -- the record's remaining centre lanes are not read.
    lpOutEntity->mCentre.x = lTransform.Pos().x + lBBoxOffset.x;
    lpOutEntity->mCentre.y = lTransform.Pos().z + lBBoxOffset.z;

    const f32 lfSpeed = lpVehicle->GetSpeed().x;
    lpOutEntity->mVelocity.x = lTransform.At().x * lfSpeed;
    lpOutEntity->mVelocity.y = lTransform.At().z * lfSpeed;

    // Corner i takes +X for bit 0, +Y for bit 1 and -Z for bit 2, all about the box offset,
    // then rides the vehicle transform into world space.
    for ( s32 liCorner = 0; liCorner < BrnTrafficIO::KI_BB_NUM_CORNERS; ++liCorner )
    {
        const f32 lfLocalX = lBBoxOffset.x
                           + ( ( liCorner & 1 ) != 0 ?  lBBoxHalfSize.x : -lBBoxHalfSize.x );
        const f32 lfLocalY = lBBoxOffset.y
                           + ( ( liCorner & 2 ) != 0 ?  lBBoxHalfSize.y : -lBBoxHalfSize.y );
        const f32 lfLocalZ = lBBoxOffset.z
                           + ( ( liCorner & 4 ) != 0 ? -lBBoxHalfSize.z :  lBBoxHalfSize.z );

        Vector3 lWorld;
        lWorld.x = lTransform.Right().x * lfLocalX + lTransform.Up().x * lfLocalY
                 + lTransform.At().x    * lfLocalZ + lTransform.Pos().x;
        lWorld.y = lTransform.Right().y * lfLocalX + lTransform.Up().y * lfLocalY
                 + lTransform.At().y    * lfLocalZ + lTransform.Pos().y;
        lWorld.z = lTransform.Right().z * lfLocalX + lTransform.Up().z * lfLocalY
                 + lTransform.At().z    * lfLocalZ + lTransform.Pos().z;
        lWorld.w = 0.0f;

        lpOutEntity->maBBCorners[ liCorner ] = lWorld;
    }
}

// ----------------------------------------------------------------------------
// ConvertSceneResultsToTrafficDataForAI
//
// Turn last frame's per-race-car nearby-traffic id lists into this frame's AI interface
// contents, then empty each list.
// ----------------------------------------------------------------------------
void TrafficEntityModule::ConvertSceneResultsToTrafficDataForAI( BrnTrafficIO::OutputBuffer_PostScene* lpOutput )
{
    CGS_ASSERT( lpOutput != NULL, "lpOutput != NULL" );

    for ( s32 liRaceCar = 0; liRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCar )
    {
        StoredAITrafficData& lrStored = maStoredAITrafficData[ liRaceCar ];

        for ( s32 liEntry = 0; liEntry < lrStored.miNumTrafficIDs; ++liEntry )
        {
            const CgsSceneManager::EntityId lTrafficID( lrStored.maTrafficEntityIDs[ liEntry ].muValue );

            CGS_ASSERT( lTrafficID.GetOwner() == KU_OWNER_TRAFFIC_VEHICLE,
                        "lTrafficID.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE" );
            CGS_ASSERT( lTrafficID.GetEntityIndex() < KU_MAX_TOTAL_TRAFFIC,
                        "lTrafficID.GetEntityIndex() < KU_MAX_TOTAL_TRAFFIC" );

            const u32      luIndex   = lTrafficID.GetEntityIndex();
            const Vehicle* lpVehicle = GetVehicle( luIndex );

            if ( lpVehicle->IsAlive() )
            {
                CGS_ASSERT( lpVehicle->HasEntity(), "lpVehicle->HasEntity()" );

                BrnTrafficIO::TrafficAIEntity lEntity;
                CreateTrafficAIEntity( luIndex,
                                       GetVehicleTypeRuntime( lpVehicle->GetVehicleType() ),
                                       static_cast<EActiveRaceCarIndex>( liRaceCar ),
                                       &lEntity );

                lpOutput->GetTrafficAIInterface()->AddTrafficEntity( lEntity );
            }
        }

        lrStored.miNumTrafficIDs = 0;
    }
}

// ----------------------------------------------------------------------------
// AIPostSceneQueryRequests
//
// One coarse frustum query per active race car, from a camera pulled back along the car's
// At axis and looking down it. The results become next frame's maStoredAITrafficData.
// ----------------------------------------------------------------------------
void TrafficEntityModule::AIPostSceneQueryRequests(
    const BrnTrafficIO::InputBuffer_PostScene* lpInput,
    BrnTrafficIO::OutputBuffer_PostScene*      lpOutput )
{
    for ( s32 liRaceCar = 0; liRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCar )
    {
        const EActiveRaceCarIndex leRaceCar = static_cast<EActiveRaceCarIndex>( liRaceCar );

        if ( !lpInput->GetActiveRaceCarOutputInterface()->IsRaceCarActive( leRaceCar ) )
        {
            continue;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpInput->GetActiveRaceCarOutputInterface()->GetRaceCarState( leRaceCar );

        CgsGraphics::Camera lQueryCamera;
        lQueryCamera.Construct( KF_AI_QUERY_CAMERA_FOV_HORIZONTAL,
                                KF_AI_QUERY_CAMERA_ASPECT_RATIO,
                                KF_AI_QUERY_CAMERA_NEAR_CLIP,
                                KF_AI_QUERY_CAMERA_FAR_CLIP );
        lQueryCamera.Release();

        const Vector3& lAt  = lpRaceCarState->mTransform.At();
        const Vector3& lUp  = lpRaceCarState->mTransform.Up();
        const Vector3& lPos = lpRaceCarState->mTransform.Pos();

        Vector3 lEye;
        lEye.x = lPos.x - KF_AI_QUERY_CAMERA_PULLBACK * lAt.x;
        lEye.y = lPos.y - KF_AI_QUERY_CAMERA_PULLBACK * lAt.y;
        lEye.z = lPos.z - KF_AI_QUERY_CAMERA_PULLBACK * lAt.z;
        lEye.w = lPos.w - KF_AI_QUERY_CAMERA_PULLBACK * lAt.w;

        Vector3 lTarget;
        lTarget.x = lPos.x + lAt.x;
        lTarget.y = lPos.y + lAt.y;
        lTarget.z = lPos.z + lAt.z;
        lTarget.w = lPos.w + lAt.w;

        lQueryCamera.LookAt( lEye, lUp, lTarget );

        CgsGeometric::Frustum lFrustum;
        lQueryCamera.GetCgsFrustum( lFrustum );

        CgsSceneManager::SceneManagerIO::InEventFrustumTestVp lEvent;
        lEvent.mViewProjection = lQueryCamera.GetViewProjectionMatrix();
        for ( s32 liPlane = 0; liPlane < 8; ++liPlane )
        {
            lEvent.maFrustumPlanes[ liPlane ] = lFrustum.maSwizzledPlanes[ liPlane ];
        }
        lEvent.mQueryId.mId        = KU_AI_FRUSTUM_QUERY_ID_BASE + static_cast<u32>( liRaceCar );
        lEvent.mx32EntityTypeFlags = KU_AI_QUERY_ENTITY_TYPES;
        lEvent.mxQueryFlags        = 0u;

        lpOutput->GetSceneCoarseQueryQueue()->AddEvent(
            &lEvent, CgsSceneManager::SceneManagerIO::E_IN_EVENT_FRUSTUM_TEST_VP );
    }
}

}
